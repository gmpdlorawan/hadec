#include "LoRaPathLossCorrelated.h"
#include <random>
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/ITransmission.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IArrival.h"

namespace flora {

Define_Module(LoRaPathLossCorrelated);

void LoRaPathLossCorrelated::initialize(int stage)
{
    FreeSpacePathLoss::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        d0 = m(par("d0")).get();
        n = par("n");
        B = par("B");
        sigma = par("sigma");
        sigmaFast = par("sigmaFast");
        cellSize = m(par("cellSize")).get();
        rhoLinks = par("rhoLinks");
        antennaGain = par("antennaGain");
        salt = ((uint64_t)intrand(0x7fffffff) << 31) ^ (uint64_t)intrand(0x7fffffff);   // differs between runs
    }
}

static uint64_t mix(uint64_t h, int64_t v)
{
    h ^= (uint64_t)v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h * 0xff51afd7ed558ccdULL;
}

bool LoRaPathLossCorrelated::isGateway(const Coord& p) const
{
    if (!gwScanned) {
        cModule *net = getSimulation()->getSystemModule();
        for (int i = 0; net->getSubmodule("loRaGW", i); i++) {
            auto mob = dynamic_cast<inet::IMobility *>(net->getSubmodule("loRaGW", i)->getSubmodule("mobility"));
            if (mob) gwPos.push_back(mob->getCurrentPosition());
        }
        gwScanned = true;
    }
    for (auto &g : gwPos) if (g.distance(p) < 1.0) return true;
    return false;
}

double LoRaPathLossCorrelated::shadowing(const Coord& a, const Coord& b) const
{
    if (sigma <= 0) return 0;
    int64_t ax = (int64_t)std::floor(a.x / cellSize), ay = (int64_t)std::floor(a.y / cellSize);
    int64_t bx = (int64_t)std::floor(b.x / cellSize), by = (int64_t)std::floor(b.y / cellSize);
    // order the two ends so that the value is reciprocal
    if (std::make_pair(ax, ay) > std::make_pair(bx, by)) { std::swap(ax, bx); std::swap(ay, by); }
    uint64_t h = mix(mix(mix(mix(salt, ax), ay), bx), by);
    std::mt19937_64 gen(h);
    std::normal_distribution<double> nd(0.0, sigma);
    double link = nd(gen);
    if (rhoLinks <= 0) return link;          // unchanged behaviour when links are independent
    // common term attached to the cell of the device end (the end that is not a gateway), so that the links
    // from one device position to all gateways share a fraction rhoLinks of their shadowing variance
    const Coord &dev = isGateway(a) ? b : a;
    int64_t dx = (int64_t)std::floor(dev.x / cellSize), dy = (int64_t)std::floor(dev.y / cellSize);
    std::mt19937_64 g2(mix(mix(mix(salt, 0x5bd1e995), dx), dy));
    std::normal_distribution<double> n2(0.0, sigma);
    double c = n2(g2);
    return std::sqrt(1 - rhoLinks) * link + std::sqrt(rhoLinks) * c;
}

double LoRaPathLossCorrelated::computePathLoss(const ITransmission *transmission, const IArrival *arrival) const
{
    const Coord& tx = transmission->getStartPosition();
    const Coord& rx = arrival->getStartPosition();
    double d = std::max(1.0, tx.distance(rx));
    double plDb = B + 10 * n * std::log10(d / d0) - antennaGain + shadowing(tx, rx);
    if (sigmaFast > 0)
        plDb += const_cast<LoRaPathLossCorrelated *>(this)->normal(0.0, sigmaFast);
    return math::dB2fraction(-plDb);
}

double LoRaPathLossCorrelated::computePathLoss(mps propagationSpeed, Hz frequency, m distance) const
{
    // mean path loss (used for range estimates only)
    double d = std::max(1.0, distance.get());
    return math::dB2fraction(-(B + 10 * n * std::log10(d / d0) - antennaGain));
}

} // namespace flora
