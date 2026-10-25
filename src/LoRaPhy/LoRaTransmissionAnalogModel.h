#ifndef LORAPHY_LORATRANSMISSIONANALOGMODEL_H_
#define LORAPHY_LORATRANSMISSIONANALOGMODEL_H_

#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarTransmissionAnalogModel.h"

using namespace inet;
using namespace inet::physicallayer;

namespace flora {

class LoRaTransmissionAnalogModel : public ScalarTransmissionAnalogModel
{
  protected:
    const int LoRaSF;
    const int LoRaCR;

  public:
    LoRaTransmissionAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, W power, int LoRaSF, int LoRaCR);

    int getLoRaSF() const { return LoRaSF; }
    int getLoRaCR() const { return LoRaCR; }
    Hz getLoRaCF() const { return getCenterFrequency(); }
    Hz getLoRaBW() const { return getBandwidth(); }
};

} // namespace flora

#endif
