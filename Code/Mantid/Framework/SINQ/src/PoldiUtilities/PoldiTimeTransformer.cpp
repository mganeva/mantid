#include "MantidSINQ/PoldiUtilities/PoldiTimeTransformer.h"

namespace Mantid
{
namespace Poldi
{

PoldiTimeTransformer::PoldiTimeTransformer() :
    m_detectorCenter(),
    m_detectorElementData(),
    m_detectorEfficiency(0.0),
    m_chopperSlits(0),
    m_spectrum()
{
}

PoldiTimeTransformer::PoldiTimeTransformer(PoldiInstrumentAdapter_sptr poldiInstrument)
{
    initializeFromPoldiInstrument(poldiInstrument);
}

void PoldiTimeTransformer::initializeFromPoldiInstrument(PoldiInstrumentAdapter_sptr poldiInstrument)
{
    if(!poldiInstrument) {
        throw std::invalid_argument("Cannot initialize PoldiTimeTransformer from null-instrument.");
    }

    PoldiAbstractDetector_sptr detector = poldiInstrument->detector();
    PoldiAbstractChopper_sptr chopper = poldiInstrument->chopper();

    m_spectrum = boost::const_pointer_cast<const PoldiSourceSpectrum>(poldiInstrument->spectrum());

    m_detectorCenter = getDetectorCenterCharacteristics(detector, chopper);
    m_detectorElementData = getDetectorElementData(detector, chopper);
    m_detectorEfficiency = 0.88;

    m_chopperSlits = chopper->slitPositions().size();
}

double PoldiTimeTransformer::dToTOF(double d) const
{
    return m_detectorCenter.tof1A * d;
}

double PoldiTimeTransformer::timeTransformedWidth(double widthD, size_t detectorIndex) const
{
    UNUSED_ARG(detectorIndex);

    return dToTOF(widthD);// + m_detectorElementData[detectorIndex]->widthFactor() * 0.0;
}

double PoldiTimeTransformer::timeTransformedCentre(double centreD, size_t detectorIndex) const
{
    return dToTOF(centreD) * m_detectorElementData[detectorIndex]->timeFactor();
}

double PoldiTimeTransformer::timeTransformedIntensity(double areaD, double centreD, size_t detectorIndex) const
{
    return areaD * detectorElementIntensity(centreD, detectorIndex);
}

double PoldiTimeTransformer::detectorElementIntensity(double centreD, size_t detectorIndex) const
{
    double lambda = dToTOF(centreD) * m_detectorElementData[detectorIndex]->lambdaFactor();
    double intensity = m_spectrum->intensity(lambda) * m_detectorElementData[detectorIndex]->intensityFactor();

    return intensity * (1.0 - exp(-m_detectorEfficiency * lambda));
}

double PoldiTimeTransformer::calculatedTotalIntensity(double centreD) const
{
    double sum = 0.0;
    double chopperSlitFactor = static_cast<double>(m_chopperSlits);

    for(size_t i = 0; i < m_detectorElementData.size(); ++i) {
        sum += chopperSlitFactor * detectorElementIntensity(centreD, i);
    }

    return sum;
}

size_t PoldiTimeTransformer::detectorElementCount() const
{
    return m_detectorElementData.size();
}

std::vector<DetectorElementData_const_sptr> PoldiTimeTransformer::getDetectorElementData(PoldiAbstractDetector_sptr detector, PoldiAbstractChopper_sptr chopper)
{
    std::vector<DetectorElementData_const_sptr> data(detector->elementCount());

    DetectorElementCharacteristics center = getDetectorCenterCharacteristics(detector, chopper);

    for(int i = 0; i < static_cast<int>(detector->elementCount()); ++i) {
        data[i] = DetectorElementData_const_sptr(new DetectorElementData(i, center, detector, chopper));
    }

    return data;
}

DetectorElementCharacteristics PoldiTimeTransformer::getDetectorCenterCharacteristics(PoldiAbstractDetector_sptr detector, PoldiAbstractChopper_sptr chopper)
{
    return DetectorElementCharacteristics(static_cast<int>(detector->centralElement()), detector, chopper);
}


} // namespace Poldi
} // namespace Mantid
