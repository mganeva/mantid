//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include "MantidMDAlgorithms/LoadSQW2.h"
#include "MantidMDAlgorithms/MDWSTransform.h"

#include "MantidAPI/FileProperty.h"
#include "MantidAPI/Progress.h"
#include "MantidAPI/RegisterFileLoader.h"
#include "MantidDataObjects/BoxControllerNeXusIO.h"
#include "MantidGeometry/Crystal/OrientedLattice.h"
#include "MantidGeometry/Instrument/Goniometer.h"
#include "MantidGeometry/MDGeometry/MDHistoDimension.h"
#include "MantidGeometry/MDGeometry/MDHistoDimensionBuilder.h"
#include "MantidKernel/ListValidator.h"
#include "MantidKernel/make_unique.h"
#include "MantidKernel/Matrix.h"
#include "MantidKernel/Memory.h"
#include "MantidKernel/ThreadScheduler.h"
#include "MantidKernel/Timer.h"
#include "MantidKernel/V3D.h"

namespace Mantid {
namespace MDAlgorithms {

using API::ExperimentInfo;
using Geometry::MDHistoDimension;
using Geometry::MDHistoDimensionBuilder;
using Geometry::Goniometer;
using Geometry::OrientedLattice;
using Kernel::BinaryStreamReader;
using Kernel::Logger;
using Kernel::DblMatrix;
using Kernel::Matrix;
using Kernel::V3D;

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
namespace {
/// Defines buffer size for reading the pixel data. It is assumed to be the
/// number of pixels to read in a single call. A single pixel is 9 float
/// fields. 150000 is ~5MB buffer
constexpr int64_t NPIX_CHUNK = 150000;
/// The MD workspace will have its boxes split after reading this many
/// chunks of events;
constexpr int64_t NCHUNKS_SPLIT = 125;
/// Defines the number of fields that define a single pixel
constexpr int32_t FIELDS_PER_PIXEL = 9;
/// 1/2pi
constexpr double INV_TWO_PI = 0.5 / M_PI;
}

// Register the algorithm into the AlgorithmFactory
DECLARE_FILELOADER_ALGORITHM(LoadSQW2)

//------------------------------------------------------------------------------
// Public methods
//------------------------------------------------------------------------------
/// Default constructor
LoadSQW2::LoadSQW2()
    : API::IFileLoader<Kernel::FileDescriptor>(), m_file(), m_reader(),
      m_outputWS(), m_nspe(0), m_outputTransforms(), m_progress(),
      m_outputFrame() {}

/// Default destructor
LoadSQW2::~LoadSQW2() {}

/// Algorithms name for identification. @see Algorithm::name
const std::string LoadSQW2::name() const { return "LoadSQW"; }

/// Algorithm's version for identification. @see Algorithm::version
int LoadSQW2::version() const { return 2; }

/// Algorithm's category for identification. @see Algorithm::category
const std::string LoadSQW2::category() const {
  return "DataHandling\\SQW;MDAlgorithms\\DataHandling";
}

/// Algorithm's summary for use in the GUI and help. @see
/// Algorithm::summary
const std::string LoadSQW2::summary() const {
  return "Load an N-dimensional workspace from a .sqw file produced by "
         "Horace.";
}

/**
* Return the confidence with this algorithm can load the file
* @param descriptor A descriptor for the file
* @returns An integer specifying the confidence level. 0 indicates it will not
* be used
*/
int LoadSQW2::confidence(Kernel::FileDescriptor &descriptor) const {
  // only .sqw can be considered
  const std::string &extn = descriptor.extension();
  if (extn.compare(".sqw") != 0)
    return 0;

  if (descriptor.isAscii()) {
    // Low so that others may try
    return 10;
  }
  // Beat v1
  return 81;
}

//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------

/// Initialize the algorithm's properties.
void LoadSQW2::init() {
  using namespace API;
  using Kernel::PropertyWithValue;
  using Kernel::StringListValidator;

  // Inputs
  declareProperty(
      new FileProperty("Filename", "", FileProperty::Load, {".sqw"}),
      "File of type SQW format");
  declareProperty(new PropertyWithValue<bool>("MetadataOnly", false),
                  "Load Metadata without events.");
  declareProperty(new API::FileProperty("OutputFilename", "",
                                        FileProperty::OptionalSave, {".nxs"}),
                  "If specified, the output workspace will be a file-backed "
                  "MDEventWorkspace");
  std::vector<std::string> allowed = {"Q_sample", "HKL"};
  declareProperty("Q3DFrames", allowed[0],
                  boost::make_shared<StringListValidator>(allowed),
                  "The required frame for the output workspace");

  // Outputs
  declareProperty(new WorkspaceProperty<IMDEventWorkspace>(
                      "OutputWorkspace", "", Kernel::Direction::Output),
                  "Output IMDEventWorkspace reflecting SQW data");
}

/// Execute the algorithm.
void LoadSQW2::exec() {
  cacheInputs();
  initFileReader();
  auto sqwType = readMainHeader();
  throwIfUnsupportedFileType(sqwType);
  createOutputWorkspace();
  readAllSPEHeadersToWorkspace();
  skipDetectorSection();
  readDataSection();
  finalize();
}

/// Cache any user input to avoid repeated lookups
void LoadSQW2::cacheInputs() { m_outputFrame = getPropertyValue("Q3DFrames"); }

/// Opens the file given to the algorithm and initializes the reader
void LoadSQW2::initFileReader() {
  using API::Progress;

  m_file = Kernel::make_unique<std::ifstream>(getPropertyValue("Filename"),
                                              std::ios_base::binary);
  m_reader = Kernel::make_unique<BinaryStreamReader>(*m_file);
  // steps are reset once we know what we are reading
  m_progress = Kernel::make_unique<Progress>(this, 0.0, 1.0, 100);
}

/**
 * Reads the initial header section. Skips specifically the
 * following: app_name, app_version, sqw_type, ndims, filename, filepath,
 * title. Caches the number of contributing files.
 * @return An integer describing the SQW type stored: 0 = DND, 1 = SQW
 */
int32_t LoadSQW2::readMainHeader() {
  std::string appName, filename, filepath, title;
  double appVersion(0.0);
  int32_t sqwType(-1), numDims(-1), nspe(-1);
  *m_reader >> appName >> appVersion >> sqwType >> numDims >> filename >>
      filepath >> title >> nspe;
  m_nspe = static_cast<uint16_t>(nspe);
  if (g_log.is(Logger::Priority::PRIO_DEBUG)) {
    std::ostringstream os;
    os << "Main header:\n"
       << "    app_name: " << appName << "\n"
       << "    app_version: " << appVersion << "\n"
       << "    sqw_type: " << sqwType << "\n"
       << "    ndims: " << numDims << "\n"
       << "    filename: " << filename << "\n"
       << "    filepath: " << filepath << "\n"
       << "    title: " << title << "\n"
       << "    nfiles: " << m_nspe << "\n";
    g_log.debug(os.str());
  }
  return sqwType;
}

/**
 * Throw std::runtime_error if the sqw type of the file is unsupported
 * @param sqwType 0 = DND, 1 = SQW
 */
void LoadSQW2::throwIfUnsupportedFileType(int32_t sqwType) {
  if (sqwType != 1) {
    throw std::runtime_error("Unsupported SQW type: " +
                             std::to_string(sqwType) +
                             "\nOnly files containing the full pixel "
                             "information are currently supported");
  }
}

/// Create the output workspace object
void LoadSQW2::createOutputWorkspace() {
  m_outputWS = boost::make_shared<SQWWorkspace>();
}

/**
 * Read all of the SPE headers and fill in the experiment details on the
 * output workspace
 */
void LoadSQW2::readAllSPEHeadersToWorkspace() {
  m_outputTransforms.reserve(m_nspe);
  for (uint16_t i = 0; i < m_nspe; ++i) {
    auto expt = readSingleSPEHeader();
    m_outputWS->addExperimentInfo(expt);
    m_outputTransforms.emplace_back(
        calculateOutputTransform(expt->run().getGoniometerMatrix(),
                                 expt->sample().getOrientedLattice()));
  }
}

/**
 * Read single SPE header from the file. It assumes the file stream
 * points at the start of a header section. It is left pointing at the end of
 * this section
 * @return A new ExperimentInfo object storing the data
 */
boost::shared_ptr<API::ExperimentInfo> LoadSQW2::readSingleSPEHeader() {
  auto experiment = boost::make_shared<ExperimentInfo>();
  auto &sample = experiment->mutableSample();
  auto &run = experiment->mutableRun();

  std::string chars;
  // skip filename, filepath
  *m_reader >> chars >> chars;
  float efix(1.0f);
  int32_t emode(0);
  // add ei as log but skip emode
  *m_reader >> efix >> emode;
  run.addProperty("Ei", static_cast<double>(efix), true);

  // lattice - alatt, angdeg, cu, cv = 12 values
  std::vector<float> floats;
  m_reader->read(floats, 12);
  auto lattice = new OrientedLattice(floats[0], floats[1], floats[2], floats[3],
                                     floats[4], floats[5]);
  V3D uVec(floats[6], floats[7], floats[8]),
      vVec(floats[9], floats[10], floats[11]);
  lattice->setUFromVectors(uVec, vVec);
  sample.setOrientedLattice(lattice);
  if (g_log.is(Logger::Priority::PRIO_DEBUG)) {
    std::stringstream os;
    os << "Lattice:"
       << "    alatt: " << lattice->a1() << " " << lattice->a2() << " "
       << lattice->a3() << "\n"
       << "    angdeg: " << lattice->alpha() << " " << lattice->beta() << " "
       << lattice->gamma() << "\n"
       << "    cu: " << floats[6] << " " << floats[7] << " " << floats[8]
       << "\n"
       << "    cv: " << floats[9] << " " << floats[10] << " " << floats[11]
       << "\n"
       << "B matrix (calculated): " << lattice->getB() << "\n"
       << "Inverse B matrix (calculated): " << lattice->getBinv() << "\n";
    g_log.debug(os.str());
  }

  // goniometer angles
  float psi(0.0f), omega(0.0f), dpsi(0.0f), gl(0.0f), gs(0.0f);
  *m_reader >> psi >> omega >> dpsi >> gl >> gs;
  V3D uvCross = uVec.cross_prod(vVec);
  Goniometer goniometer;
  goniometer.pushAxis("psi", uvCross[0], uvCross[1], uvCross[2], psi);
  goniometer.pushAxis("omega", uvCross[0], uvCross[1], uvCross[2], omega);
  goniometer.pushAxis("gl", 1.0, 0.0, 0.0, gl);
  goniometer.pushAxis("gs", 0.0, 0.0, 1.0, gs);
  goniometer.pushAxis("dpsi", 0.0, 1.0, 0.0, dpsi);
  run.setGoniometer(goniometer, false);
  if (g_log.is(Logger::Priority::PRIO_DEBUG)) {
    std::stringstream os;
    os << "Goniometer angles:\n"
       << "    psi: " << psi << "\n"
       << "    omega: " << omega << "\n"
       << "    gl: " << gl << "\n"
       << "    gs: " << gs << "\n"
       << "    dpsi: " << dpsi << "\n"
       << "    goniometer matrix: " << goniometer.getR() << "\n";
    g_log.debug(os.str());
  }
  // energy bins
  int32_t nbounds(0);
  *m_reader >> nbounds;
  std::vector<float> enBins(nbounds);
  m_reader->read(enBins, nbounds);
  run.storeHistogramBinBoundaries(
      std::vector<double>(enBins.begin(), enBins.end()));

  // Skip the per-spe file projection information. We only use the
  // information from the data section
  m_file->seekg(96, std::ios_base::cur);
  std::vector<int32_t> ulabel_shape(2);
  m_reader->read(ulabel_shape, 2);
  // shape[0]*shape[1]*sizeof(char)
  m_file->seekg(ulabel_shape[0] * ulabel_shape[1], std::ios_base::cur);

  return experiment;
}

/**
 * Return the required transformation to move from the coordinates in the file
 * to the selected output frame
 * @param gonR The goniometer matrix
 * @param lattice A reference to the lattice object
 */
Kernel::DblMatrix
LoadSQW2::calculateOutputTransform(const Kernel::DblMatrix &gonR,
                                   const Geometry::OrientedLattice &lattice) {
  // File coordinates are in the crystal coordinate system
  if (m_outputFrame == "Q_sample")
    return DblMatrix(3, 3, true);
  else if (m_outputFrame == "HKL")
    return lattice.getBinv() * INV_TWO_PI;
  else
    throw std::logic_error(
        "LoadSQW2::calculateOutputTransform - Unknown output frame: " +
        m_outputFrame);
}

/**
 * Skip the data in the detector section. The size is based on the number
 * of contribution detector parameters
 */
void LoadSQW2::skipDetectorSection() {
  std::string filename, filepath;
  *m_reader >> filename >> filepath;
  int32_t ndet(0);
  *m_reader >> ndet;
  if (g_log.is(Logger::Priority::PRIO_DEBUG)) {
    std::stringstream os;
    os << "Skipping " << ndet << " detector parameters from '" << filename
       << "'\n";
    g_log.debug(os.str());
  }
  // 6 float fields all ndet long - group, x2, phi, azim, width, height
  m_file->seekg(6 * 4 * ndet, std::ios_base::cur);
}

void LoadSQW2::readDataSection() {
  skipDataSectionMetadata();
  readSQWDimensions();
  bool metadataOnly = getProperty("MetadataOnly");
  if (!metadataOnly)
    readPixelData();
}

/**
 * Skip metadata in data section: filename, filepath, title, lattice,
 * projection information.
 * On exit the file pointer will be positioned after the last
 * ulen entry
 */
void LoadSQW2::skipDataSectionMetadata() {
  std::string filename, filepath, title;
  *m_reader >> filename >> filepath >> title;
  // skip alatt, angdeg, uoffset, u_to_rlu, ulen
  m_file->seekg(120, std::ios_base::cur);
}

/**
 * Read and create the SQW dimensions on the output
 * On exit the file pointer will be positioned after the last
 * ulimit entry
 */
void LoadSQW2::readSQWDimensions() {
  // Reads the information in the plot axes (pax) and integration axes (iax)
  // fields to construct the SQW dimensions. The dimension limits are taken
  // as the first/last entries of the pax/iax fields as required and the
  // bin boundary values themselves are ignored as we are creating a full
  // MDEventWorkspace

  // skip dimension labels
  std::vector<int32_t> ulabelShape(2);
  m_reader->read(ulabelShape, 2);
  m_file->seekg(ulabelShape[0] * ulabelShape[1], std::ios_base::cur);

  // projection information
  int32_t nProjAxes(0);
  *m_reader >> nProjAxes;
  int32_t nIntAxes(4 - nProjAxes);
  std::vector<float> dimLimits(8, 0.f);
  if (nIntAxes > 0) {
    // indices of integrated axes (1-based)
    std::vector<int32_t> indices(nIntAxes, 0);
    m_reader->read(indices, nIntAxes);
    // range of integrated axes
    std::vector<float> ranges(2 * nIntAxes, 0.f);
    m_reader->read(ranges, 2 * nIntAxes);
    for (int32_t i = 0; i < nIntAxes; ++i) {
      auto zeroBasedIdx(indices[i] - 1);
      dimLimits[2 * zeroBasedIdx] = ranges[2 * i];
      dimLimits[2 * zeroBasedIdx + 1] = ranges[2 * i + 1];
    }
  }
  std::vector<int32_t> nbins(4, 1);
  int32_t signalLength(1);
  if (nProjAxes > 0) {
    // indices of projection axes (1-based)
    std::vector<int32_t> indices(nProjAxes);
    m_reader->read(indices, nProjAxes);
    // limits & nbins for each projection axis
    for (int32_t i = 0; i < nProjAxes; ++i) {
      int32_t nBinBounds(0);
      *m_reader >> nBinBounds;
      auto zeroBasedIdx(indices[i] - 1);
      nbins[zeroBasedIdx] = nBinBounds - 1;
      signalLength *= nBinBounds - 1;
      *m_reader >> dimLimits[2 * zeroBasedIdx];
      m_file->seekg((nBinBounds - 2) * sizeof(float), std::ios_base::cur);
      *m_reader >> dimLimits[2 * zeroBasedIdx + 1];
    }
    // skip display axes
    m_file->seekg(nProjAxes * sizeof(int32_t), std::ios_base::cur);
  }
  // skip signal(float), error data(float), npix(int64)
  m_file->seekg(2 * signalLength * sizeof(float) +
                    signalLength * sizeof(int64_t),
                std::ios_base::cur);
  // skip data limits
  m_file->seekg(8 * sizeof(int32_t), std::ios_base::cur);

  if (g_log.is(Logger::Priority::PRIO_DEBUG)) {
    std::stringstream os;
    os << "Data:\n"
       << "    ranges (file): ";
    for (size_t i = 0; i < 4; ++i) {
      os << "(" << dimLimits[2 * i] << "," << dimLimits[2 * i + 1] << ") ";
    }
    os << "\n";
    os << "    nbins: (";
    for (const auto &val : nbins) {
      os << val << ",";
    }
    os << ")";
    g_log.debug(os.str());
  }

  transformLimitsToOutputFrame(dimLimits);
  if (g_log.is(Logger::Priority::PRIO_DEBUG)) {
    std::stringstream os;
    os << "    ranges (transformed): ";
    for (size_t i = 0; i < 4; ++i) {
      os << "(" << dimLimits[2 * i] << "," << dimLimits[2 * i + 1] << ") ";
    }
  }
  // The lattice is assumed to be the same in all contributing files so use
  // the first B matrix to create the axis information (only needed in HKL
  // frame)
  const auto &bmat0 =
      m_outputWS->getExperimentInfo(0)->sample().getOrientedLattice().getB();
  for (size_t i = 0; i < 4; ++i) {
    float umin(dimLimits[2 * i]), umax(dimLimits[2 * i + 1]);
    assert(umin <= umax);
    if (i < 3) {
      m_outputWS->addDimension(createQDimension(
          i, umin, umax, static_cast<size_t>(nbins[i]), bmat0));
    } else {
      m_outputWS->addDimension(
          createEnDimension(umin, umax, static_cast<size_t>(nbins[i])));
    }
  }
  setupBoxController();
}

#ifdef __clang__
// The missing braces warning is a false positive -
// https://llvm.org/bugs/show_bug.cgi?id=21629
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
/**
 * Transform the dimension limits to the output frame. We define Q_sample as the
 * same projection as in the file so no transformation is applied in this case.
 * For HKL we simply apply the B^1 transformation.
 * @param urange Limits from the projection/integration fields in the file
 */
void LoadSQW2::transformLimitsToOutputFrame(std::vector<float> &urange) {
  V3D fileMin(urange[0], urange[2], urange[4]);
  V3D transMin = m_outputTransforms[0] * fileMin;
  V3D fileMax(urange[1], urange[3], urange[5]);
  V3D transMax = m_outputTransforms[0] * fileMax;
  for(size_t i = 0; i < 3; ++i) {
    urange[2*i] = transMin[i];
    urange[2*i + 1] = transMax[i];
  }
}

/**
 * Create the Q MDHistoDimension for the output frame and given information from
 * the file
 * @param index Index of the dimension
 * @param dimMin Dimension minimum in output frame
 * @param dimMax Dimension maximum in output frame
 * @param nbins Number of bins for this dimension
 * @param bmat A reference to the B matrix to create the axis labels for the HKL
 * frame
 * @return A new MDHistoDimension object
 */
Geometry::IMDDimension_sptr
LoadSQW2::createQDimension(size_t index, float dimMin, float dimMax,
                           size_t nbins, const Kernel::DblMatrix &bmat) {
  if (index > 2) {
    throw std::logic_error("LoadSQW2::createQDimension - Expected a dimension "
                           "index between 0 & 2. Found: " +
                           std::to_string(index));
  }
  static std::array<const char *, 3> indexToDim{"x", "y", "z"};
  MDHistoDimensionBuilder builder;
  builder.setId(std::string("q") + indexToDim[index]);
  MDHistoDimensionBuilder::resizeToFitMDBox(dimMin, dimMax);
  builder.setMin(dimMin);
  builder.setMax(dimMax);
  builder.setNumBins(nbins);

  std::string name, unit, frameName;
  if (m_outputFrame == "Q_sample" || m_outputFrame == "Q_lab") {
    name = m_outputFrame + "_" + indexToDim[index];
    unit = "A^-1";
    frameName = (m_outputFrame == "Q_sample") ? "QSample" : "QLab";
  } else if (m_outputFrame == "HKL") {
    static std::array<const char *, 3> indexToHKL{"[H,0,0]", "[0,K,0]",
                                                  "[0,0,L]"};
    name = indexToHKL[index];
    V3D dimDir;
    dimDir[index] = 1;
    const V3D x = bmat * dimDir;
    double length = 2. * M_PI * x.norm();
    unit = "in " + MDAlgorithms::sprintfd(length, 1.e-3) + " A^-1";
    frameName = "HKL";
  } else {
    throw std::logic_error(
        "LoadSQW2::createQDimension - Unknown output frame: " + m_outputFrame);
  }
  builder.setUnits(unit);
  builder.setName(name);
  builder.setFrameName(frameName);

  return builder.create();
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

/**
 * Create an energy dimension
 * @param dimMin Dimension minimum in output frame
 * @param dimMax Dimension maximum in output frame
 * @param nbins Number of bins for this dimension
 * @return A new MDHistoDimension object
 */
Geometry::IMDDimension_sptr
LoadSQW2::createEnDimension(float dimMin, float dimMax, size_t nbins) {
  MDHistoDimensionBuilder builder;
  builder.setId("en");
  builder.setUnits("meV");
  builder.setName("en");
  builder.setFrameName("meV");
  MDHistoDimensionBuilder::resizeToFitMDBox(dimMin, dimMax);
  builder.setMin(dimMin);
  builder.setMax(dimMax);
  builder.setNumBins(nbins);
  return builder.create();
}

/**
 * Setup the box controller based on the bin structure
 */
void LoadSQW2::setupBoxController() {
  using Kernel::Timer;
  Timer timer;

  auto boxController = m_outputWS->getBoxController();
  for (size_t i = 0; i < 4; i++) {
    boxController->setSplitInto(i, m_outputWS->getDimension(i)->getNBins());
  }
  boxController->setMaxDepth(1);
  m_outputWS->initialize();
  // Start with a MDGridBox.
  m_outputWS->splitBox();

  g_log.debug() << "Time to setup box structure: " << timer.elapsed() << "s\n";

  std::string fileback = getProperty("OutputFilename");
  if (!fileback.empty()) {
    setupFileBackend(fileback);
  }
}

/**
 * Setup the filebackend for the output workspace. It assumes that the
 * box controller has already been initialized
 * @param filebackPath Path to the file used for backend storage
 */
void LoadSQW2::setupFileBackend(std::string filebackPath) {
  using DataObjects::BoxControllerNeXusIO;
  auto savemd = this->createChildAlgorithm("SaveMD", 0.01, 0.05, true);
  savemd->setProperty("InputWorkspace", m_outputWS);
  savemd->setPropertyValue("Filename", filebackPath);
  savemd->setProperty("UpdateFileBackEnd", false);
  savemd->setProperty("MakeFileBacked", false);
  savemd->executeAsChildAlg();

  // create file-backed box controller
  auto boxControllerMem = m_outputWS->getBoxController();
  auto boxControllerIO =
      boost::make_shared<BoxControllerNeXusIO>(boxControllerMem.get());
  boxControllerMem->setFileBacked(boxControllerIO, filebackPath);
  m_outputWS->getBox()->setFileBacked();
  boxControllerMem->getFileIO()->setWriteBufferSize(1000000);
}

/**
 * Read the pixel data into the workspace
 */
void LoadSQW2::readPixelData() {
  using Kernel::Timer;
  Timer timer;

  // skip redundant field
  m_file->seekg(sizeof(int32_t), std::ios_base::cur);
  int64_t npixtot(0);
  *m_reader >> npixtot;
  g_log.debug() << "    npixtot: " << npixtot << "\n";
  warnIfMemoryInsufficient(npixtot);
  m_progress->setNumSteps(npixtot);

  // Each pixel has 9 float fields. Do a chunked read to avoid
  // using too much memory for the buffer and also split the
  // boxes regularly to ensure that larger workspaces can be loaded
  // without blowing the memory requirements
  constexpr int64_t bufferSize(FIELDS_PER_PIXEL * NPIX_CHUNK);
  std::vector<float> pixBuffer(bufferSize);
  int64_t pixelsLeftToRead(npixtot), chunksRead(0);
  size_t pixelsAdded(0);
  while (pixelsLeftToRead > 0) {
    int64_t chunkSize(pixelsLeftToRead);
    if (chunkSize > NPIX_CHUNK) {
      chunkSize = NPIX_CHUNK;
    }
    m_reader->read(pixBuffer, FIELDS_PER_PIXEL * chunkSize);
    for (int64_t i = 0; i < chunkSize; ++i) {
      pixelsAdded += addEventFromBuffer(pixBuffer.data() + i * 9);
      m_progress->report();
    }
    pixelsLeftToRead -= chunkSize;
    ++chunksRead;
    if ((chunksRead % NCHUNKS_SPLIT) == 0) {
      splitAllBoxes();
    }
  }
  assert(pixelsLeftToRead == 0);
  if (pixelsAdded == 0) {
    throw std::runtime_error(
        "No pixels could be added from the source file. "
        "Please check the irun fields of all pixels are valid.");
  } else if (pixelsAdded != static_cast<size_t>(npixtot)) {
    g_log.warning("Some pixels within the source file had an invalid irun "
                  "field. They have been ignored.");
  }

  g_log.debug() << "Time to read all pixels: " << timer.elapsed() << "s\n";
}

/**
 * Split boxes in the output workspace if required
 */
void LoadSQW2::splitAllBoxes() {
  using Kernel::ThreadPool;
  using Kernel::ThreadSchedulerFIFO;
  auto *ts = new ThreadSchedulerFIFO();
  ThreadPool tp(ts);
  m_outputWS->splitAllIfNeeded(ts);
  tp.joinAll();
}

/**
 * If the output is not file backed and the machine appears to have
 * insufficient
 * memory to read the data in total then warn the user. We don't stop
 * the algorithm just in case our memory calculation is wrong.
 * @param npixtot The total number of pixels to be read
 */
void LoadSQW2::warnIfMemoryInsufficient(int64_t npixtot) {
  using DataObjects::MDEvent;
  using Kernel::MemoryStats;
  if (m_outputWS->isFileBacked())
    return;
  MemoryStats stat;
  size_t reqdMemory =
      (npixtot * sizeof(MDEvent<4>) + NPIX_CHUNK * FIELDS_PER_PIXEL) / 1024;
  if (reqdMemory > stat.availMem()) {
    g_log.warning()
        << "It looks as if there is insufficient memory to load the "
        << "entire file. It is recommended to cancel the algorithm and "
           "specify "
           "the OutputFilename option to create a file-backed workspace.\n";
  }
}

/**
 * Assume the given pointer points to the start of a full pixel and create
 * an MDEvent based on it iff it has a valid run id.
 * @param pixel A pointer assumed to point to at the start of a single pixel
 * from the data file
 * @return 1 if the event was added, 0 otherwise
 */
size_t LoadSQW2::addEventFromBuffer(const float *pixel) {
  using DataObjects::MDEvent;
  // Is the pixel field valid? Older versions of Horace produced files with
  // an invalid field and we can't use this. It should be between 1 && nfiles
  uint16_t irun = static_cast<uint16_t>(pixel[4]);
  if (irun < 1 || irun > m_nspe) {
    return 0;
  }
  auto u1(pixel[0]), u2(pixel[1]), u3(pixel[2]), en(pixel[3]);
  uint16_t runIndex = static_cast<uint16_t>(irun - 1);
  toOutputFrame(runIndex, u1, u2, u3);
  const auto idet = static_cast<detid_t>(pixel[5]);
  // skip energy bin
  auto signal = pixel[7];
  auto error = pixel[8];
  coord_t centres[4] = {u1, u2, u3, en};
  m_outputWS->addEvent(
      MDEvent<4>(signal, error * error, runIndex, idet, centres));
  return 1;
}

/**
 * Transform the given coordinates from the file frame to the requested output
 * frame using the cached transformation for the given run
 * @param runIndex Index into the experiment list
 * @param u1 Coordinate parallel to the beam axis
 * @param u2 Coordinate perpendicular to u1 and in the horizontal plane
 * @param u3 The cross-product of u1 & u2
 */
void LoadSQW2::toOutputFrame(const uint16_t runIndex, float &u1, float &u2,
                             float &u3) {
  V3D uVec(u1, u2, u3);
  V3D qout = m_outputTransforms[runIndex] * uVec;
  u1 = static_cast<float>(qout[0]);
  u2 = static_cast<float>(qout[1]);
  u3 = static_cast<float>(qout[2]);
}

/**
 * Assumed to be the last step in the algorithm. Performs any steps
 * necessary after everything else has run successfully
 */
void LoadSQW2::finalize() {
  splitAllBoxes();
  m_outputWS->refreshCache();
  if (m_outputWS->isFileBacked()) {
    auto savemd = this->createChildAlgorithm("SaveMD", 0.76, 1.00);
    savemd->setProperty("InputWorkspace", m_outputWS);
    savemd->setProperty("UpdateFileBackEnd", true);
    savemd->executeAsChildAlg();
  }
  setProperty("OutputWorkspace", m_outputWS);
}

} // namespace MDAlgorithms
} // namespace Mantid
