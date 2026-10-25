#ifndef __FLORA_ROAMINGNETWORKSERVERAPP_H_
#define __FLORA_ROAMINGNETWORKSERVERAPP_H_

#include <map>
#include <vector>
#include <tuple>
#include <omnetpp.h>
#include "inet/common/INETDefs.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "../LoRa/LoRaMacFrame_m.h"
#include "RoamingMessages_m.h"

using namespace omnetpp;
using namespace inet;

namespace flora {

struct Reception {
    bool local = true;
    std::string gwAddr;   // L3 address of the gateway (ULToken)
    int gwIndex = -1;
    int fromNet = -1;     // network owning the gateway
    double rssi = -999, snir = -999, rxTime = 0;
    int sf = 10;
    double cf = 868e6;
};

struct UplinkAgg {
    long devId = 0;
    long mac = 0;
    int fcnt = 0, frameType = 0, netId = 0, sessionId = 0, homeNetId = 0;
    bool confirmed = false;
    bool linkCheck = false;
    double x = 0, y = 0;
    double firstRx = -1;
    std::vector<Reception> recs;
    bool processed = false;
};

struct PendingTrigger {
    bool valid = false;
    int sf = -1, maxRetries = 0;
    double period = 32;
};

class RoamingNetworkServerApp : public cSimpleModule
{
  protected:
    int netId = 0;
    std::vector<std::string> serverPaths;
    cModule *ecn = nullptr;
    bool passiveAgreement = true, activeAgreement = true, reportForeignToEcn = true;
    double receiveDelay1 = 5, localDedup = 0.1, relayDedup = 0.6, dlDecisionDelay = 1;
    double frontierX = 10000, borderHalfWidth = 2000;
    UdpSocket socket;
    int destPort = 2000;
    int sessionCounter = 1000;

    typedef std::tuple<long,int,int> Key;   // devId, fcnt, frameType
    std::map<Key, UplinkAgg> aggs;
    std::map<long, PendingTrigger> triggers;
    std::map<long, std::pair<int,int>> pendingSwitch;  // devId -> (old net, new session)

    // statistics
    std::map<long, long> delivered, deliveredBorder, deliveredRelayed;
    long bkSent[8] = {0};
    long forceRejoinSent = 0, acksSent = 0, joinAcceptsSent = 0, dlViaRelay = 0, rejoinsIgnored = 0;
    long linkCheckAnsSent = 0;
    long relayedLate = 0, foreignDropped = 0, handoversIn = 0, handoversOut = 0;

    enum { TIMER_LOCAL = 100, TIMER_PROCESS, TIMER_DL, TIMER_ACCEPT };

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    void handleUplink(Packet *pkt);
    void handleBackend(BackendMsg *msg);
    void handleTimer(cMessage *msg);
    void onLocalCollected(const Key& key);
    void processServed(const Key& key);
    void decideDownlink(const Key& key);
    void startHandoverIn(const Key& key);
    void sendDownlinkToGw(const std::string& gwAddr, long mac, int sf, double cf, double txTime, const Ptr<RoamingPayload>& payload, double tpDbm = 14);
    void chooseWindow(const Reception *best, double &txTime, int &sf, double &cf, double &tp);
    void sendBackend(BackendMsg *msg, cModule *target, double delay);
    cModule *server(int net);
    const Reception *bestReception(const UplinkAgg& a, bool localOnly) const;
    cMessage *keyTimer(const char *name, int kind, const Key& key);
    bool counting() const { return simTime() >= getSimulation()->getWarmupPeriod(); }
};

} // namespace flora
#endif
