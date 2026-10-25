#ifndef LORAPHY_LORARECEPTIONANALOGMODEL_H_
#define LORAPHY_LORARECEPTIONANALOGMODEL_H_

#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarReceptionAnalogModel.h"

using namespace inet;
using namespace inet::physicallayer;

namespace flora {

class LoRaReceptionAnalogModel : public ScalarReceptionAnalogModel
{
  protected:
    const int LoRaSF;
    const int LoRaCR;

  public:
    LoRaReceptionAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, W power, int LoRaSF, int LoRaCR);

    int getLoRaSF() const { return LoRaSF; }
    int getLoRaCR() const { return LoRaCR; }
    Hz getLoRaCF() const { return getCenterFrequency(); }
    Hz getLoRaBW() const { return getBandwidth(); }
    W getLoRaPower() const { return getPower(); }

    virtual W computeMinPower(simtime_t startTime, simtime_t endTime) const override { return getPower(); }
};

} // namespace flora

#endif
