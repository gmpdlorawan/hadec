#ifndef __FLORA_ROAMINGLORAAPP_H_
#define __FLORA_ROAMINGLORAAPP_H_

#include <omnetpp.h>
#include "inet/common/INETDefs.h"
#include "inet/common/lifecycle/ILifecycle.h"
#include "inet/mobility/contract/IMobility.h"
#include "../LoRa/LoRaRadio.h"
#include "RoamingMessages_m.h"

using namespace omnetpp;
using namespace inet;

namespace flora {

class RoamingLoRaApp : public cSimpleModule, public ILifecycle
{
  protected:
    // configuration
    int homeNetId = 0;
    bool lorawan11 = true;
    double confirmedProbability = 1;
    double receiveDelay1 = 5;
    bool autonomousRejoin = false;
    int missLimit = 3;
    int linkCheckPeriod = 0;          // LinkCheckReq every N uplinks (0 = off)
    int linkCheckMissLimit = 2;       // unanswered LinkCheckReq before an autonomous rejoin
    int uplinksSinceLinkCheck = 0, lastLinkCheckFcnt = -1, linkCheckMisses = 0;
    bool lastLinkCheckAnswered = true;
    long linkChecksSent = 0, linkCheckAnswers = 0;
    int periodicRejoinCount = 0;
    int uplinksSinceRejoin = 0;
    long periodicRejoins = 0;
    double frontierX = 10000, borderHalfWidth = 2000;
    int dataSF = 10;

    // session state
    int currentNet = 0;
    int sessionId = 0;
    int fcnt = 0;
    long devId = 0;

    // rejoin state
    bool rejoinPending = false;
    int rejoinRetriesLeft = 0;
    double rejoinRetryPeriod = 32;
    int rejoinSF = -1;
    cMessage *sendTimer = nullptr;
    cMessage *rejoinTimer = nullptr;       // next rejoin transmission (first or retry)
    cMessage *acceptTimeout = nullptr;
    int consecutiveMisses = 0;
    void startAutonomousRejoin();
    int lastConfirmedFcnt = -1;
    bool lastConfirmedAcked = true;

    LoRaRadio *loRaRadio = nullptr;
    IMobility *mobility = nullptr;

    // statistics (after warm-up)
    long sent = 0, sentConfirmed = 0, acks = 0;
    long sentBorder = 0, sentConfirmedBorder = 0, acksBorder = 0;
    long rejoinsSent = 0, forceRejoinsReceived = 0, joinAccepts = 0, autonomousRejoins = 0;
    long switches = 0;
    double firstSwitchX = NAN, lastSwitchX = NAN, firstSwitchTime = NAN;
    simsignal_t networkSwitchSignal;
    // service interruption (instrumentation: deliveries reported by whichever network server delivers the uplink)
    double nominalPeriod = 60, firstSentTime = -1, lastDeliveredTime = -1;
    double interruptionTotal = 0, interruptionMax = 0;
    long interruptions = 0;
    double handoverGap = -1;          // gap between the last delivery before the first switch and the first delivery after it
    void addGap(double gap);

  public:
    static std::map<long, RoamingLoRaApp *>& registry();
    void notifyDelivered(simtime_t t);
    virtual ~RoamingLoRaApp();

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override { return true; }

    void sendData();
    void sendRejoin();
    void sendFrame(const Ptr<RoamingPayload>& payload, int sf, const char *name);
    void handleDownlink(Packet *pkt);
    void checkMissedAck();
    bool inBorder() const;
    Coord position() const;
    bool counting() const { return simTime() >= getSimulation()->getWarmupPeriod(); }
};

} // namespace flora
#endif
