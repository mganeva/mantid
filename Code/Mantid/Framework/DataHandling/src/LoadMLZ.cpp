/*WIKI*
Loads an MLZ Nexus file into a [[Workspace2D]] with the given name.

This algorithm is under development.

To date this algorithm supports: TOFTOF.

TODO: Enter a full wiki-markup description of your algorithm here. You can then use the Build/wiki_maker.py script to generate your full wiki page.
*WIKI*/

#include "MantidDataHandling/LoadMLZ.h"
#include "MantidAPI/FileProperty.h"
#include "MantidAPI/Progress.h"
#include "MantidAPI/RegisterFileLoader.h"
#include "MantidGeometry/Instrument.h"
#include "MantidKernel/EmptyValues.h"
#include "MantidKernel/UnitFactory.h"
#include "MantidDataHandling/LoadHelper.h"

#include <limits>
#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>


namespace Mantid
{
namespace DataHandling
{

  using namespace Kernel;
  using namespace API;
  using namespace NeXus;

  // Register the algorithm into the AlgorithmFactory
  DECLARE_NEXUS_FILELOADER_ALGORITHM(LoadMLZ)
  
  /**
  * tostring operator to print the contents of NXClassInfo
  *
  * TODO : This has to go somewhere else
  */
//  std::ostream& operator<<(std::ostream &strm, const NXClassInfo &c)
//  {
//    return strm << "NXClassInfo :: nxname: " << c.nxname << " , nxclass: "
//      << c.nxclass;
//  }

  //----------------------------------------------------------------------------------------------
  /** Constructor
   */
  LoadMLZ::LoadMLZ() : API::IFileLoader<Kernel::NexusDescriptor>()
  {
     m_instrumentName = "";
     m_wavelength = 0;
     m_channelWidth = 0.0;
     m_numberOfChannels = 0;
     m_numberOfHistograms = 0;
     m_numberOfTubes = 0;
     m_numberOfPixelsPerTube = 0;
     m_monitorElasticPeakPosition = 0;
     m_monitorCounts = 0;
     m_timeOfFlightDelay = 0;
     m_chopper_speed = 0.0;
     m_chopper_ratio = 0;
     m_l1 = 0;
     m_l2 = 0;
     m_supportedInstruments.push_back("TOFTOF");
     m_supportedInstruments.push_back("DNS");
  }
    
  //---------------------------------------------------------------------------
  /** Destructor
   */
  LoadMLZ::~LoadMLZ()
  {
  }
  

  //---------------------------------------------------------------------------
  /// Algorithm's name for identification. @see Algorithm::name
  const std::string LoadMLZ::name() const { return "LoadMLZ";}
  
  /// Algorithm's version for identification. @see Algorithm::version
  int LoadMLZ::version() const { return 1;}
  
  /// Algorithm's category for identification. @see Algorithm::category
  const std::string LoadMLZ::category() const { return "DataHandling";}


  //---------------------------------------------------------------------------
  /** Initialize the algorithm's properties.
   */
  void LoadMLZ::init()
  {
     std::vector<std::string> exts;
     exts.push_back(".nxs");
     exts.push_back(".hdf");
     exts.push_back(".hd5");

     declareProperty(
          new FileProperty("Filename", "", FileProperty::Load, exts),
          "File path of the Data file to load");

     declareProperty(
          new WorkspaceProperty<>("OutputWorkspace", "", Direction::Output),
          "The name to use for the output workspace");


  }


  //---------------------------------------------------------------------------
  /** Execute the algorithm.
   */
  void LoadMLZ::exec()
  {
     // Retrieve filename
     std::string filenameData = getPropertyValue("Filename");

     // open the root node
     NeXus::NXRoot dataRoot(filenameData);
     NXEntry dataFirstEntry = dataRoot.openFirstEntry();

     loadInstrumentDetails(dataFirstEntry);
     loadTimeDetails(dataFirstEntry);
     initWorkSpace(dataFirstEntry);

     runLoadInstrument(); // just to get IDF contents
     initInstrumentSpecific();

     //Read Elastic Peak Position from Monitor's group - entry "TOF_ChannelOfElasticLine_Guess" of raw data
     loadDataIntoTheWorkSpace(dataFirstEntry);//,m_monitorElasticPeakPosition);

     loadRunDetails(dataFirstEntry);
     loadExperimentDetails(dataFirstEntry);
     maskDetectors(dataFirstEntry);

     // load the instrument from the IDF if it exists
     //runLoadInstrument();

     // Set the output workspace property
     setProperty("OutputWorkspace", m_localWorkspace);
  }                                                 


  /**
  * Return the confidence with with this algorithm can load the file
  * @param descriptor A descriptor for the file
  * @returns An integer specifying the confidence level. 0 indicates it will not be used
  */
  int LoadMLZ::confidence(Kernel::NexusDescriptor & descriptor) const
  {
     // fields existent only at the MLZ
     if (descriptor.pathExists("/Scan/wavelength")
       && descriptor.pathExists("/Scan/title")
       && descriptor.pathExists("/Scan/mode"))
     {
        return 80;
     }
     else
     {
      return 0;
     }
  }


  /**
   * Loads Masked detectors from the /Scan/instrument/Detector/pixel_mask
   */
  void LoadMLZ::maskDetectors(NeXus::NXEntry& entry)
  {
     // path to the pixel_mask
     std::string pmpath = "instrument/detector/pixel_mask";

     NeXus::NXInt pmdata = entry.openNXInt(pmpath);
     // load the counts from the file into memory
     pmdata.load();
     g_log.debug() << "PMdata size: " << pmdata.size() << std::endl;
     std::vector<int> masked_detectors(pmdata(), pmdata()+pmdata.size());

     g_log.debug() << "Number of masked detectors: " << masked_detectors.size() << std::endl;

     for (size_t i=0; i<masked_detectors.size(); i++)
     {
        g_log.debug() << "List of masked detectors: ";
        g_log.debug() << masked_detectors[i];
        g_log.debug() << ", ";
     }
     g_log.debug() << std::endl;

     // Need to get hold of the parameter map
     Geometry::ParameterMap& pmap = m_localWorkspace->instrumentParameters();

     // If explicitly given a list of detectors to mask, just mark those.
     // Otherwise, mask all detectors pointing to the requested spectra in indexlist loop below
     std::vector<detid_t>::const_iterator it;
     Geometry::Instrument_const_sptr instrument = m_localWorkspace->getInstrument();
     if ( ! masked_detectors.empty() )
     {
        for (it = masked_detectors.begin(); it != masked_detectors.end(); ++it)
        {
           try
           {
              if ( const Geometry::ComponentID det = instrument->getDetector(*it)->getComponentID() )
              {
                 pmap.addBool(det,"masked",true);
              }
           }
           catch(Kernel::Exception::NotFoundError &e)
           {
              g_log.warning() << e.what() << " Found while running MaskDetectors" << std::endl;
           }
        }
     }
  }


  /**
   * Set the instrument name along with its path on the nexus file
  */
  void LoadMLZ::loadInstrumentDetails(NeXus::NXEntry& firstEntry)
  {

     m_instrumentPath = m_mlzloader.findInstrumentNexusPath(firstEntry);

     if (m_instrumentPath == "")
     {
        throw std::runtime_error("Cannot set the instrument name from the Nexus file!");
     }

     m_instrumentName = m_mlzloader.getStringFromNexusPath(firstEntry, m_instrumentPath + "/name");

     if (std::find(m_supportedInstruments.begin(), m_supportedInstruments.end(),
             m_instrumentName) == m_supportedInstruments.end())
     {
          std::string message = "The instrument " + m_instrumentName + " is not valid for this loader!";
          throw std::runtime_error(message);
     }

     g_log.debug() << "Instrument name set to: " + m_instrumentName << std::endl;

   }


  /**
   * Creates the workspace and initialises member variables with
   * the corresponding values
   *
   * @param entry :: The Nexus entry
   *
   */
  void LoadMLZ::initWorkSpace(NeXus::NXEntry& entry)
  {
     // read in the data
     NXData dataGroup = entry.openNXData("data");
     NXInt data = dataGroup.openIntData();

     m_numberOfTubes = static_cast<size_t>(data.dim0());
     m_numberOfPixelsPerTube = static_cast<size_t>(data.dim1());
     m_numberOfChannels = static_cast<size_t>(data.dim2());

     m_numberOfHistograms = m_numberOfTubes * m_numberOfPixelsPerTube;

     g_log.debug() << "NumberOfTubes: " << m_numberOfTubes << std::endl;
     g_log.debug() << "NumberOfPixelsPerTube: " << m_numberOfPixelsPerTube << std::endl;
     g_log.debug() << "NumberOfChannels: " << m_numberOfChannels << std::endl;

      // Now create the output workspace
     m_localWorkspace = WorkspaceFactory::Instance().create("Workspace2D",
              m_numberOfHistograms, m_numberOfChannels + 1, m_numberOfChannels);
     m_localWorkspace->getAxis(0)->unit() = UnitFactory::Instance().create("TOF");
     m_localWorkspace->setYUnitLabel("Counts");

  }


  /**
   * Function to do specific instrument stuff
   *
   */
  void LoadMLZ::initInstrumentSpecific()
  {
     // Read data from IDF: distance source-sample and distance sample-detectors
     m_l1 = m_mlzloader.getL1(m_localWorkspace);
     m_l2 = m_mlzloader.getL2(m_localWorkspace);

     g_log.debug() << "L1: " << m_l1 << ", L2: " << m_l2 << std::endl;
  }


  /**
   * Load the time details from the nexus file.
   * @param entry :: The Nexus entry
   */
  void LoadMLZ::loadTimeDetails(NeXus::NXEntry& entry)
  {

     m_wavelength = entry.getFloat("wavelength");

     // Monitor can be monitor or Monitor
     std::string monitorName;
     if (entry.containsGroup("monitor"))
        monitorName = "monitor";
     else if (entry.containsGroup("Monitor"))
        monitorName = "Monitor";
     else
        {
           std::string message("Cannot find monitor/Monitor in the Nexus file!");
           g_log.error(message);
           throw std::runtime_error(message);
        }

     m_monitorCounts = entry.getInt(monitorName + "/integral");//monitor_counts");

     m_monitorElasticPeakPosition = entry.getInt(monitorName + "/elastic_peak");

     NXFloat time_of_flight_data = entry.openNXFloat(monitorName + "/time_of_flight");
     time_of_flight_data.load();

     // The entry "monitor/time_of_flight", has 3 fields:
     // channel width [microseconds], number of channels, Time of flight delay
     m_channelWidth = time_of_flight_data[0]*50.e-3;
     m_timeOfFlightDelay = time_of_flight_data[2]*50.e-3;

     g_log.debug("Nexus Data:");
     g_log.debug() << " MonitorCounts: " << m_monitorCounts << std::endl;
     g_log.debug() << " ChannelWidth (microseconds): " << m_channelWidth << std::endl;
     g_log.debug() << " Wavelength (angstroems): " << m_wavelength << std::endl;
     g_log.debug() << " ElasticPeakPosition: " << m_monitorElasticPeakPosition << std::endl;
     g_log.debug() << " TimeOfFlightDelay (microseconds): " <<  m_timeOfFlightDelay<< std::endl;

     m_chopper_speed = entry.getFloat("instrument/chopper/rotation_speed");

     m_chopper_ratio = entry.getInt("instrument/chopper/ratio");

     g_log.debug() << " ChopperSpeed: " << m_chopper_speed << std::endl;
     g_log.debug() << " ChopperRatio: " <<  m_chopper_ratio << std::endl;


   }


  /**
  * Load information about the run.
  * People from ISIS have this...
  * TODO: They also have a lot of info in XML format!
  *
  * @param entry :: The Nexus entry
  */
   void LoadMLZ::loadRunDetails(NXEntry & entry)
   {

     API::Run & runDetails = m_localWorkspace->mutableRun();

     int runNum = entry.getInt("entry_identifier");//run_number");
     std::string run_num = boost::lexical_cast<std::string>(runNum);
     runDetails.addProperty("run_number", run_num);

     std::string start_time = entry.getString("start_time");
     start_time = m_mlzloader.dateTimeInIsoFormat(start_time);
     runDetails.addProperty("run_start", start_time);

     std::string end_time = entry.getString("end_time");
     end_time = m_mlzloader.dateTimeInIsoFormat(end_time);
     runDetails.addProperty("run_end", end_time);

     std::string wavelength = boost::lexical_cast<std::string>(m_wavelength);
     runDetails.addProperty("wavelength", wavelength);

     double ei = m_mlzloader.calculateEnergy(m_wavelength);
     runDetails.addProperty<double>("Ei", ei, true); //overwrite

     std::string duration = boost::lexical_cast<std::string>(
                                   entry.getFloat("duration"));
     runDetails.addProperty("duration", duration);

     std::string mode = entry.getString("mode");
     runDetails.addProperty("mode", mode);

     std::string title = entry.getString("title");
     runDetails.addProperty("title", title);
     m_localWorkspace->setTitle(title);

     // This should belong to sample ???
     std::string experiment_identifier = entry.getString("experiment_identifier");
     runDetails.addProperty("experiment_title", experiment_identifier);
     m_localWorkspace->mutableSample().setName(experiment_identifier);

     // Check if temperature is defined
     NXClass sample = entry.openNXGroup("sample");
     if ( sample.containsDataSet("temperature") )
     {
     std::string temperature = boost::lexical_cast<std::string>(
     entry.getFloat("sample/temperature"));
     runDetails.addProperty("temperature", temperature);
     }


     std::string monitorCounts = boost::lexical_cast<std::string>(m_monitorCounts);
     runDetails.addProperty("monitor_counts", monitorCounts);

     std::string chopper_speed = boost::lexical_cast<std::string>(m_chopper_speed);
     runDetails.addProperty("chopper_speed", chopper_speed);

     std::string chopper_ratio = boost::lexical_cast<std::string>(m_chopper_ratio);
     runDetails.addProperty("chopper_ratio", chopper_ratio);

     std::string channel_width = boost::lexical_cast<std::string>(m_channelWidth);
     runDetails.addProperty("channel_width", channel_width);

     //Calculate number of full time channels - use to crop workspace - S. Busch's method
     double full_channels = floor(30.*m_chopper_ratio/(m_chopper_speed)*1.e6/m_channelWidth);//channelWidth in microsec.
     runDetails.addProperty("full_channels", full_channels);

  }


  /**
   * Load data about the Experiment.
   *
   * TODO: This is very incomplete. In ISIS they much more info in the nexus file than ILL.
   *
   * @param entry :: The Nexus entry
   */
  void LoadMLZ::loadExperimentDetails(NXEntry & entry)
  {
     // TODO: Do the rest
     // Pick out the geometry information

     std::string description = boost::lexical_cast<std::string>(
        entry.getFloat("sample/description"));

     m_localWorkspace->mutableSample().setName(description);

  }

  /**
   * Loads all the spectra into the workspace, including that from the monitor
   *
   * @param entry :: The Nexus entry
   * @param ElasticPeakPosition :: If -1 uses this value as the elastic peak position at the detector.
   */
  void LoadMLZ::loadDataIntoTheWorkSpace(NeXus::NXEntry& entry)//, int ElasticPeakPosition)
  {
     // read in the data
     NXData dataGroup = entry.openNXData("data");
     NXInt data = dataGroup.openIntData();
     // load the counts from the file into memory
     data.load();

     //set it as a Property
     API::Run & runDetails = m_localWorkspace->mutableRun();
     runDetails.addProperty("EPP", m_monitorElasticPeakPosition);


     // Calculate the real tof (t1+t2) put it in tof array
     std::vector<double> detectorTofBins(m_numberOfChannels + 1);
     for (size_t i = 0; i < m_numberOfChannels + 1; ++i)
        {
           // From Lamp's t2e: m_channelWidth*FLOAT(channel_number - ElasticPeakPosition) + L2/Vi
           detectorTofBins[i] = m_channelWidth
                   * static_cast<double>(static_cast<int>(i) + 1);/*theoreticalElasticTOF
                                + m_channelWidth
                                * static_cast<double>(static_cast<int>(i) - ElasticPeakPosition);*/
                               // - m_channelWidth / 2; // to make sure the bin is in the middle of the elastic peak
        }

     //g_log.information() << "T1+T2 : Theoretical = " << theoreticalElasticTOF;
     //g_log.information() << " ::  Calculated bin = ["
     //                    << detectorTofBins[ElasticPeakPosition] << ","
     //                   << detectorTofBins[ElasticPeakPosition + 1] << "]"
     //                   << std::endl;

     // Assign calculated bins to first X axis
     m_localWorkspace->dataX(0).assign(detectorTofBins.begin(), detectorTofBins.end());

     Progress progress(this, 0, 1, m_numberOfTubes * m_numberOfPixelsPerTube);
     size_t spec = 0;
     for (size_t i = 0; i < m_numberOfTubes; ++i)
     {
        for (size_t j = 0; j < m_numberOfPixelsPerTube; ++j)
        {
           if (spec > 0)
           {
              // just copy the time binning axis to every spectra
              m_localWorkspace->dataX(spec) = m_localWorkspace->readX(0);
           }
           // Assign Y
           int* data_p = &data(static_cast<int>(i), static_cast<int>(j), 0);
           m_localWorkspace->dataY(spec).assign(data_p,
           data_p + m_numberOfChannels);

           // Assign Error
           MantidVec& E = m_localWorkspace->dataE(spec);
           std::transform(data_p, data_p + m_numberOfChannels, E.begin(),LoadMLZ::calculateError);

           ++spec;
           progress.report();
        }
     }
  }


  /**
   * Run the Child Algorithm LoadInstrument.
   */
  void LoadMLZ::runLoadInstrument()
  {

     IAlgorithm_sptr loadInst = createChildAlgorithm("LoadInstrument");

     // Now execute the Child Algorithm. Catch and log any error, but don't stop.
     try
     {
        // TODO: depending on the m_numberOfPixelsPerTube we might need to load a different IDF

        loadInst->setPropertyValue("InstrumentName", m_instrumentName);
        g_log.debug() << "InstrumentName" << m_instrumentName << std::endl;
        loadInst->setProperty<MatrixWorkspace_sptr>("Workspace", m_localWorkspace);
        loadInst->execute();
     }
     catch (...)
     {
        g_log.information("Cannot load the instrument definition.");
      }
   }


} // namespace DataHandling
} // namespace Mantid
