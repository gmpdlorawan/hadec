#ifndef LORAPHY_LORAPATHLOSSCORRELATED_H_
#define LORAPHY_LORAPATHLOSSCORRELATED_H_

#include <vector>
#include "inet/physicallayer/wireless/common/pathloss/FreeSpacePathLoss.h"

using namespace inet;
using namespace inet::physicallayer;
namespace flora {

class LoRaPathLossCorrelated : public FreeSpacePathLoss
{
  protected:
    double d0 = 1000, n = 2.32, B = 128.95, sigma = 7.8, sigmaFast = 1, cellSize = 100, antennaGain = 2, rhoLinks = 0;
    uint64_t salt = 0;
    mutable bool gwScanned = false;
    mutable std::vector<Coord> gwPos;

  protected:
    virtual void initialize(int stage) override;
    double shadowing(const Coord& a, const Coord& b) const;
    bool isGateway(const Coord& p) const;

  public:
    virtual double computePathLoss(const ITransmission *transmission, const IArrival *arrival) const override;
    virtual double computePathLoss(mps propagationSpeed, Hz frequency, m distance) const override;
};

} // namespace flora
#endif
