#ifndef __FLORA_ECN_H_
#define __FLORA_ECN_H_

#include <map>
#include <deque>
#include <vector>
#include <omnetpp.h>
#include "RoamingMessages_m.h"

using namespace omnetpp;

namespace flora {

struct EcnFrame {
    int fcnt = 0;
    double t = 0;
    bool rx[2] = {false, false};
    double rssi[2] = {-999, -999};
    double snir[2] = {-999, -999};
    int sf = 10;
    double x = 0, y = 0;
    int serving = 0;
};

struct EcnDevice {
    int serving = 0;
    std::map<int, EcnFrame> open;
    std::deque<EcnFrame> hist;
    double blockedUntil = -1, pendingUntil = -1;
    int hyst = 0;
    bool hasEma = false;
    double ema = 0;
    bool prepared[2] = {false, false};
    long triggers = 0;
    int failures = 0;      // consecutive triggers without a completed switch
};

struct Waypoint { double t, x, y; };

class Ecn : public cSimpleModule
{
  protected:
    std::string strategy;
    std::vector<std::string> serverPaths;
    std::vector<int> gwNet;
    std::vector<std::pair<double,double>> gwPos;
    std::map<long, EcnDevice> devs;
    std::map<long, std::vector<Waypoint>> traces;   // oracle: devId -> trajectory
    long obsReceived = 0, triggersSent = 0, preparesSent = 0, switchesSeen = 0;
    long triggerReasonB = 0, triggerReasonLose = 0;
    // instrumentation (charge de l'ECN)
    long nDecisions = 0, nPts = 0, nCensored = 0; double cpuDecide = 0; double nearSum = 0; long nearMax = 0, nearSamples = 0;
    std::map<long,double> lastHeardByOther;
    struct Pred { double conf, H; int serving; int n; };
    std::map<const void*, Pred> preds;
    FILE *calib = nullptr;


  protected:
    virtual int numInitStages() const override { return 2; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void evaluate(long devId, int fcnt);
    bool decide(EcnDevice &d, const EcnFrame &f, int &rejoinSF);
    void trigger(long devId, EcnDevice &d, int rejoinSF);
    bool counting() const { return simTime() >= getSimulation()->getWarmupPeriod(); }

    // helpers
    static double phi(double z);
    double meanRx(int net, double x, double y) const;   // best mean received power (dBm, 14 dBm TX)
    int bestNetAt(double x, double y) const;
    bool positionAt(long devId, double t, double &x, double &y) const;
    void loadTraces();
    static double requiredSnr(int sf);
    static double sensitivity(int sf);
    int commonRejoinSF(const EcnDevice &d, const EcnFrame &f) const;
    bool regress(const std::vector<std::pair<double,double>> &pts, double tStar, double sigma, double &pred, double &sPred, double &slope) const;
};

} // namespace flora
#endif
