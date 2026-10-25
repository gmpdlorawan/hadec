#include "RoamingLoRaApp.h"
#include "../LoRa/LoRaTagInfo_m.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"

namespace flora {

Define_Module(RoamingLoRaApp);

void RoamingLoRaApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        homeNetId = par("homeNetId");
        currentNet = homeNetId;
        lorawan11 = par("lorawan11");
        confirmedProbability = par("confirmedProbability");
        receiveDelay1 = par("receiveDelay1").doubleValue();
        autonomousRejoin = par("autonomousRejoin");
        missLimit = par("missLimit");
        linkCheckPeriod = par("linkCheckPeriod");
        linkCheckMissLimit = par("linkCheckMissLimit");
        periodicRejoinCount = par("periodicRejoinCount");
        frontierX = par("frontierX").doubleValue();
        borderHalfWidth = par("borderHalfWidth").doubleValue();
        dataSF = par("initialLoRaSF");
        networkSwitchSignal = registerSignal("networkSwitch");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        cModule *host = getContainingNode(this);
        mobility = check_and_cast<IMobility *>(host->getSubmodule("mobility"));
        loRaRadio = check_and_cast<LoRaRadio *>(getParentModule()->getSubmodule("LoRaNic")->getSubmodule("radio"));
        loRaRadio->loRaTP = par("initialLoRaTP").doubleValue();
        loRaRadio->loRaCF = units::values::Hz(par("initialLoRaCF").doubleValue());
        loRaRadio->loRaSF = dataSF;
        loRaRadio->loRaBW = units::values::Hz(par("initialLoRaBW").doubleValue());
        loRaRadio->loRaCR = par("initialLoRaCR");
        loRaRadio->loRaUseHeader = true;
        cModule *mac = getParentModule()->getSubmodule("LoRaNic")->getSubmodule("mac");
        devId = (long) MacAddress(mac->par("address").stringValue()).getInt();
        registry()[devId] = this;
        nominalPeriod = par("nominalPeriod").doubleValue();

        sendTimer = new cMessage("sendData");
        rejoinTimer = new cMessage("sendRejoin");
        acceptTimeout = new cMessage("acceptTimeout");
        scheduleAt(simTime() + par("timeToFirstPacket"), sendTimer);
    }
}

Coord RoamingLoRaApp::position() const
{
    return mobility->getCurrentPosition();
}

bool RoamingLoRaApp::inBorder() const
{
    return std::fabs(position().x - frontierX) <= borderHalfWidth;
}

void RoamingLoRaApp::handleMessage(cMessage *msg)
{
    if (msg == sendTimer) {
        sendData();
        scheduleAt(simTime() + par("timeToNextPacket"), sendTimer);
    }
    else if (msg == rejoinTimer) {
        sendRejoin();
    }
    else if (msg == acceptTimeout) {
        // no join-accept: keep the old session (LoRaWAN 1.1), retry if allowed
        if (rejoinRetriesLeft > 0) {
            rejoinRetriesLeft--;
            scheduleAt(simTime() + rejoinRetryPeriod, rejoinTimer);
        }
        else
            rejoinPending = false;
    }
    else {
        handleDownlink(check_and_cast<Packet *>(msg));
        delete msg;
    }
}

void RoamingLoRaApp::sendFrame(const Ptr<RoamingPayload>& payload, int sf, const char *name)
{
    auto pkt = new Packet(name);
    Coord c = position();
    payload->setXPos(c.x);
    payload->setYPos(c.y);
    payload->setChunkLength(B(par("dataSize").intValue()));
    pkt->insertAtBack(payload);
    // EU868: pseudo-random channel hopping over 8 uplink channels; RX1 uses the same channel
    static const double channels[8] = {867.1e6, 867.3e6, 867.5e6, 867.7e6, 867.9e6, 868.1e6, 868.3e6, 868.5e6};
    loRaRadio->loRaCF = units::values::Hz(channels[intuniform(0, 7)]);
    loRaRadio->loRaSF = sf;   // the receive windows listen with the uplink parameters
    auto loraTag = pkt->addTagIfAbsent<LoRaTag>();
    loraTag->setBandwidth(loRaRadio->loRaBW);
    loraTag->setCenterFrequency(loRaRadio->loRaCF);
    loraTag->setSpreadFactor(sf);
    loraTag->setCodeRendundance(loRaRadio->loRaCR);
    loraTag->setPower(mW(math::dBmW2mW(loRaRadio->loRaTP)));
    loraTag->setUseHeader(true);
    send(pkt, "socketOut");
}

void RoamingLoRaApp::checkMissedAck()
{
    if (lastConfirmedFcnt >= 0 && !lastConfirmedAcked) {
        consecutiveMisses++;
        if (autonomousRejoin && lorawan11 && !rejoinPending && consecutiveMisses >= missLimit) {
            consecutiveMisses = 0;
            startAutonomousRejoin();
        }
    }
}

void RoamingLoRaApp::startAutonomousRejoin()
{
    // reactive baseline: the device itself concludes that its network is lost
    rejoinPending = true;
    rejoinSF = dataSF;
    rejoinRetriesLeft = par("autonomousMaxRetries");
    rejoinRetryPeriod = par("autonomousRetryPeriod").doubleValue();
    if (counting()) autonomousRejoins++;
    cancelEvent(rejoinTimer);
    scheduleAt(simTime() + 0.001, rejoinTimer);
}

std::map<long, RoamingLoRaApp *>& RoamingLoRaApp::registry()
{
    static std::map<long, RoamingLoRaApp *> reg;
    return reg;
}

RoamingLoRaApp::~RoamingLoRaApp()
{
    auto it = registry().find(devId);
    if (it != registry().end() && it->second == this) registry().erase(it);
}

void RoamingLoRaApp::addGap(double gap)
{
    // a gap longer than 1.5 nominal periods counts as an interruption; its length beyond one period is lost service time
    if (gap > 1.5 * nominalPeriod) {
        double excess = gap - nominalPeriod;
        interruptionTotal += excess;
        interruptionMax = std::max(interruptionMax, excess);
        interruptions++;
    }
}

void RoamingLoRaApp::notifyDelivered(simtime_t t)
{
    if (!counting() || firstSentTime < 0) return;
    double ref = lastDeliveredTime >= 0 ? lastDeliveredTime : firstSentTime;
    addGap(t.dbl() - ref);
    if (handoverGap < 0 && !std::isnan(firstSwitchTime) && t.dbl() >= firstSwitchTime)
        handoverGap = t.dbl() - std::min(ref, firstSwitchTime);
    lastDeliveredTime = t.dbl();
}

void RoamingLoRaApp::sendData()
{
    checkMissedAck();
    // periodic Rejoin-request type 0 (RejoinParamSetupReq, LoRaWAN 1.1): standard-conformant device-side baseline
    if (lorawan11 && periodicRejoinCount > 0 && ++uplinksSinceRejoin >= periodicRejoinCount && !rejoinPending) {
        uplinksSinceRejoin = 0;
        rejoinPending = true;
        rejoinSF = dataSF;
        rejoinRetriesLeft = 0;
        if (counting()) periodicRejoins++;
        cancelEvent(rejoinTimer);
        scheduleAt(simTime() + 0.001, rejoinTimer);
    }
    auto payload = makeShared<RoamingPayload>();
    payload->setFrameType(RF_DATA);
    payload->setDevId(devId);
    payload->setNetId(currentNet);
    payload->setSessionId(sessionId);
    payload->setFcnt(fcnt++);
    payload->setHomeNetId(homeNetId);
    bool confirmed = uniform(0, 1) < confirmedProbability;
    payload->setConfirmed(confirmed);
    // LinkCheckReq (LoRaWAN MAC command): the device checks that a network still answers it
    if (linkCheckPeriod > 0 && ++uplinksSinceLinkCheck >= linkCheckPeriod) {
        uplinksSinceLinkCheck = 0;
        if (lastLinkCheckFcnt >= 0 && !lastLinkCheckAnswered) linkCheckMisses++;
        if (autonomousRejoin && lorawan11 && !rejoinPending && linkCheckMisses >= linkCheckMissLimit) {
            linkCheckMisses = 0;
            startAutonomousRejoin();
        }
        payload->setLinkCheck(true);
        lastLinkCheckFcnt = payload->getFcnt();
        lastLinkCheckAnswered = false;
        if (counting()) linkChecksSent++;
    }
    if (confirmed) {
        lastConfirmedFcnt = payload->getFcnt();
        lastConfirmedAcked = false;
    }
    if (counting() && firstSentTime < 0) firstSentTime = simTime().dbl();
    if (counting()) {
        sent++;
        bool border = inBorder();
        if (border) sentBorder++;
        if (confirmed) { sentConfirmed++; if (border) sentConfirmedBorder++; }
    }
    sendFrame(payload, dataSF, "DataFrame");
}

void RoamingLoRaApp::sendRejoin()
{
    auto payload = makeShared<RoamingPayload>();
    payload->setFrameType(RF_REJOIN_REQ);
    payload->setDevId(devId);
    payload->setNetId(currentNet);
    payload->setSessionId(sessionId);
    payload->setFcnt(fcnt);   // RJcount0 is separate in LoRaWAN; fcnt only used for de-duplication
    payload->setHomeNetId(homeNetId);
    payload->setConfirmed(false);
    if (counting()) rejoinsSent++;
    sendFrame(payload, rejoinSF > 0 ? rejoinSF : dataSF, "RejoinRequest");
    // join-accept expected in RX1 (JOIN_ACCEPT_DELAY1) or RX2; keep listening for both windows
    cancelEvent(acceptTimeout);
    // the request may wait in the MAC queue until the receive windows of a previous uplink are over
    scheduleAt(simTime() + 2 * receiveDelay1 + 7.0, acceptTimeout);
}

void RoamingLoRaApp::handleDownlink(Packet *pkt)
{
    const auto &payload = pkt->peekAtFront<RoamingPayload>();
    EV_INFO << "downlink type=" << payload->getFrameType() << " dev=" << payload->getDevId() << " me=" << devId
            << " rejoinPending=" << rejoinPending << " newNet=" << payload->getNewNetId() << endl;
    if (payload->getDevId() != devId)
        return;
    if (payload->getAck() && payload->getFcnt() == lastConfirmedFcnt && !lastConfirmedAcked) {
        lastConfirmedAcked = true;
        consecutiveMisses = 0;
        if (counting()) {
            acks++;
            if (inBorder()) acksBorder++;
        }
    }
    if (payload->getLinkCheck() && payload->getFcnt() == lastLinkCheckFcnt && !lastLinkCheckAnswered) {
        lastLinkCheckAnswered = true;
        linkCheckMisses = 0;
        if (counting()) linkCheckAnswers++;
    }
    if (payload->getFrameType() == RF_FORCE_REJOIN) {
        if (!lorawan11)
            return;  // LoRaWAN 1.0.x devices do not implement ForceRejoinReq
        if (counting()) forceRejoinsReceived++;
        rejoinPending = true;
        rejoinSF = payload->getRejoinSF();
        rejoinRetriesLeft = payload->getMaxRetries();
        rejoinRetryPeriod = payload->getRetryPeriod();
        cancelEvent(rejoinTimer);
        scheduleAt(simTime() + 0.001, rejoinTimer);
    }
    else if (payload->getFrameType() == RF_JOIN_ACCEPT) {
        if (!rejoinPending)
            return;
        rejoinPending = false;
        cancelEvent(acceptTimeout);
        cancelEvent(rejoinTimer);
        int from = currentNet;
        currentNet = payload->getNewNetId();
        sessionId = payload->getNewSessionId();
        consecutiveMisses = 0;
        lastConfirmedAcked = true;
        linkCheckMisses = 0;
        lastLinkCheckAnswered = true;
        if (currentNet != from) {
            double x = position().x;
            emit(networkSwitchSignal, x);
            if (counting()) {
                joinAccepts++;
                switches++;
                if (std::isnan(firstSwitchX)) { firstSwitchX = x; firstSwitchTime = simTime().dbl(); }
                lastSwitchX = x;
            }
        }
    }
}

void RoamingLoRaApp::finish()
{
    // trailing gap: from the last delivered uplink (or the first sent one) to the end of the simulation
    if (firstSentTime >= 0)
        addGap(simTime().dbl() - (lastDeliveredTime >= 0 ? lastDeliveredTime : firstSentTime));
    recordScalar("interruptionTotal", interruptionTotal);
    recordScalar("interruptionMax", interruptionMax);
    recordScalar("interruptions", interruptions);
    // handover never followed by a delivery: the gap runs to the end of the simulation
    if (handoverGap < 0 && !std::isnan(firstSwitchTime) && firstSentTime >= 0)
        handoverGap = simTime().dbl() - std::min(lastDeliveredTime >= 0 ? lastDeliveredTime : firstSentTime, firstSwitchTime);
    recordScalar("handoverGap", handoverGap < 0 ? NAN : handoverGap);
    recordScalar("devId", devId);
    recordScalar("homeNet", homeNetId);
    recordScalar("finalNet", currentNet);
    recordScalar("lorawan11", lorawan11);
    recordScalar("sent", sent);
    recordScalar("sentConfirmed", sentConfirmed);
    recordScalar("acks", acks);
    recordScalar("sentBorder", sentBorder);
    recordScalar("sentConfirmedBorder", sentConfirmedBorder);
    recordScalar("acksBorder", acksBorder);
    recordScalar("rejoinsSent", rejoinsSent);
    recordScalar("autonomousRejoins", autonomousRejoins);
    recordScalar("linkChecksSent", linkChecksSent);
    recordScalar("linkCheckAnswers", linkCheckAnswers);
    recordScalar("periodicRejoins", periodicRejoins);
    recordScalar("forceRejoinsReceived", forceRejoinsReceived);
    recordScalar("joinAccepts", joinAccepts);
    recordScalar("switches", switches);
    recordScalar("firstSwitchX", firstSwitchX);
    recordScalar("firstSwitchTime", firstSwitchTime);
    recordScalar("lastSwitchX", lastSwitchX);
    cancelAndDelete(sendTimer);
    cancelAndDelete(rejoinTimer);
    cancelAndDelete(acceptTimeout);
    sendTimer = rejoinTimer = acceptTimeout = nullptr;
}

} // namespace flora
