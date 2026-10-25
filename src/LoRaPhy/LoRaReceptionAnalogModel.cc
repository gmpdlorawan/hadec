#include "LoRaReceptionAnalogModel.h"

namespace flora {

LoRaReceptionAnalogModel::LoRaReceptionAnalogModel(simtime_t preambleDuration, simtime_t headerDuration, simtime_t dataDuration, Hz centerFrequency, Hz bandwidth, W power, int LoRaSF, int LoRaCR) :
    ScalarReceptionAnalogModel(preambleDuration, headerDuration, dataDuration, centerFrequency, bandwidth, power),
    LoRaSF(LoRaSF),
    LoRaCR(LoRaCR)
{
}

} // namespace flora
