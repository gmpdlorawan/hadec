#include "RoamingNetworkServerApp.h"
#include "RoamingLoRaApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/common/packet/Packet.h"

namespace flora {

Define_Module(RoamingNetworkServerApp);

void RoamingNetworkServerApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        netId = par("netId");
        cStringTokenizer tok(par("serverPaths").stringValue());
        while (tok.hasMoreTokens())
            serverPaths.push_back(tok.nextToken());
        passiveAgreement = par("passiveAgreement");
        activeAgreement = par("activeAgreement");
        reportForeignToEcn = par("reportForeignToEcn");
        receiveDelay1 = par("receiveDelay1").doubleValue();
        localDedup = par("localDedup").doubleValue();
        relayDedup = par("relayDedup").doubleValue();
        dlDecisionDelay = par("dlDecisionDelay").doubleValue();
        destPort = par("destPort");
        frontierX = par("frontierX").doubleValue();
        borderHalfWidth = par("borderHalfWidth").doubleValue();
        sessionCounter = 1000 * (netId + 1);
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        socket.setOutputGate(gate("socketOut"));
        socket.bind(par("localPort").intValue());
        const char *ecnPath = par("ecnModule");
        if (*ecnPath)
            ecn = getModuleByPath(ecnPath);
    }
}

cModule *RoamingNetworkServerApp::server(int net)
{
    if (net < 0 || net >= (int)serverPaths.size())
        return nullptr;
    return getModuleByPath(serverPaths[net].c_str());
}

void RoamingNetworkServerApp::sendBackend(BackendMsg *msg, cModule *target, double delay)
{
    if (target == nullptr) { delete msg; return; }
    if (counting() && msg->getBkType() < 8) bkSent[msg->getBkType()]++;
    sendDirect(msg, delay, 0, target, "backendIn");
}

cMessage *RoamingNetworkServerApp::keyTimer(const char *name, int kind, const Key& key)
{
    auto t = new KeyTimer(name, kind);
    t->setDevId(std::get<0>(key));
    t->setFcnt(std::get<1>(key));
    t->setFrameType(std::get<2>(key));
    return t;
}

void RoamingNetworkServerApp::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
        handleTimer(msg);
    else if (msg->arrivedOn("socketIn"))
        handleUplink(check_and_cast<Packet *>(msg));
    else if (msg->arrivedOn("backendIn"))
        handleBackend(check_and_cast<BackendMsg *>(msg));
    else
        delete msg;
}

void RoamingNetworkServerApp::handleUplink(Packet *pkt)
{
    const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
    const auto &payload = pkt->peekDataAt<RoamingPayload>(frame->getChunkLength());
    const auto &networkHeader = getNetworkProtocolHeader(pkt);
    Reception r;
    r.local = true;
    r.gwAddr = networkHeader->getSourceAddress().str();
    r.gwIndex = frame->getGwId();
    r.fromNet = netId;
    r.rssi = frame->getRSSI();
    r.snir = math::fraction2dB(frame->getSNIR());
    r.rxTime = frame->getRxTime();
    r.sf = frame->getLoRaSF();
    r.cf = frame->getLoRaCF().get();

    Key key(payload->getDevId(), payload->getFcnt(), payload->getFrameType());
    auto it = aggs.find(key);
    if (it == aggs.end()) {
        UplinkAgg a;
        a.devId = payload->getDevId();
        a.mac = (long)frame->getTransmitterAddress().getInt();
        a.fcnt = payload->getFcnt();
        a.frameType = payload->getFrameType();
        a.netId = payload->getNetId();
        a.sessionId = payload->getSessionId();
        a.homeNetId = payload->getHomeNetId();
        a.confirmed = payload->getConfirmed();
        a.linkCheck = payload->getLinkCheck();
        a.x = payload->getXPos();
        a.y = payload->getYPos();
        a.firstRx = r.rxTime;
        a.recs.push_back(r);
        aggs[key] = a;
        scheduleAt(simTime() + localDedup, keyTimer("localCollected", TIMER_LOCAL, key));
    }
    else if (!it->second.processed)
        it->second.recs.push_back(r);
    delete pkt;
}

const Reception *RoamingNetworkServerApp::bestReception(const UplinkAgg& a, bool localOnly) const
{
    const Reception *best = nullptr;
    for (auto &r : a.recs) {
        if (localOnly && !r.local) continue;
        if (best == nullptr || r.snir > best->snir)
            best = &r;
    }
    return best;
}

void RoamingNetworkServerApp::onLocalCollected(const Key& key)
{
    auto it = aggs.find(key);
    if (it == aggs.end()) return;
    UplinkAgg &a = it->second;
    const Reception *best = bestReception(a, true);

    if (a.frameType == RF_REJOIN_REQ) {
        if (a.netId == netId) {           // the device already has a session with us
            rejoinsIgnored++;
            aggs.erase(it);
            return;
        }
        if (!activeAgreement) {
            foreignDropped++;
            aggs.erase(it);
            return;
        }
        startHandoverIn(key);
        return;
    }

    // observation for the ECN (serving or foreign view of this uplink)
    bool serving = (a.netId == netId);
    if (ecn != nullptr && best != nullptr && (serving || reportForeignToEcn)) {
        auto obs = new BackendMsg("OBS");
        obs->setBkType(BK_OBS);
        obs->setDevId(a.devId);
        obs->setFromNet(netId);
        obs->setToNet(a.netId);
        obs->setFcnt(a.fcnt);
        obs->setRxTime(best->rxTime);
        obs->setSf(best->sf);
        obs->setRssi(best->rssi);
        obs->setSnir(best->snir);
        obs->setGwIndex(best->gwIndex);
        obs->setFromServing(serving);
        obs->setXPos(a.x);
        obs->setYPos(a.y);
        sendBackend(obs, ecn, par("ecnDelay").doubleValue());
    }

    if (serving) {
        double wait = passiveAgreement ? std::max(0.0, a.firstRx + relayDedup - simTime().dbl()) : 0;
        scheduleAt(simTime() + wait, keyTimer("process", TIMER_PROCESS, key));
    }
    else {
        // forwarding NS: passive roaming relay (PRStartReq) or drop (no agreement)
        if (passiveAgreement && best != nullptr) {
            auto pr = new BackendMsg("PRStartReq");
            pr->setBkType(BK_PRSTART);
            pr->setDevId(a.devId);
            pr->setFromNet(netId);
            pr->setToNet(a.netId);
            pr->setFcnt(a.fcnt);
            pr->setConfirmed(a.confirmed);
            pr->setLinkCheck(a.linkCheck);
            pr->setRxTime(best->rxTime);
            pr->setSf(best->sf);
            pr->setCf(best->cf);
            pr->setRssi(best->rssi);
            pr->setSnir(best->snir);
            pr->setGwIndex(best->gwIndex);
            pr->setGwAddr(best->gwAddr.c_str());
            pr->setMacAddr(a.mac);
            pr->setSessionId(a.sessionId);
            pr->setXPos(a.x);
            pr->setYPos(a.y);
            sendBackend(pr, server(a.netId), par("backendDelay").doubleValue());
        }
        else
            foreignDropped++;
        aggs.erase(it);
    }
}

void RoamingNetworkServerApp::processServed(const Key& key)
{
    auto it = aggs.find(key);
    if (it == aggs.end()) return;
    UplinkAgg &a = it->second;
    a.processed = true;
    bool viaRelayOnly = true;
    for (auto &r : a.recs) if (r.local) viaRelayOnly = false;
    if (counting()) {
        delivered[a.devId]++;
        if (std::fabs(a.x - frontierX) <= borderHalfWidth) deliveredBorder[a.devId]++;
        if (viaRelayOnly) deliveredRelayed[a.devId]++;
    }
    // instrumentation: service-interruption tracking on the device module (whichever network delivers the uplink)
    auto dev = RoamingLoRaApp::registry().find(a.devId);
    if (dev != RoamingLoRaApp::registry().end()) dev->second->notifyDelivered(simTime());
    // first uplink of a session created by a rejoin: the handover is complete (11.4, step 12)
    auto ps = pendingSwitch.find(a.devId);
    if (ps != pendingSwitch.end() && a.sessionId == ps->second.second) {
        int oldNet = ps->second.first;
        pendingSwitch.erase(ps);
        if (counting()) handoversIn++;
        auto done = new BackendMsg("SwitchDone");
        done->setBkType(BK_SWITCH_DONE);
        done->setDevId(a.devId);
        done->setFromNet(oldNet);
        done->setToNet(netId);
        done->setRxTime(a.firstRx);
        sendBackend(done, server(oldNet), par("backendDelay").doubleValue());
        if (ecn) sendBackend(done->dup(), ecn, par("ecnDelay").doubleValue());
    }
    double at = std::max(simTime().dbl(), a.firstRx + dlDecisionDelay);
    scheduleAt(at, keyTimer("downlink", TIMER_DL, key));
}

void RoamingNetworkServerApp::decideDownlink(const Key& key)
{
    auto it = aggs.find(key);
    if (it == aggs.end()) return;
    UplinkAgg a = it->second;
    aggs.erase(it);
    PendingTrigger &trig = triggers[a.devId];
    bool sendTrigger = trig.valid;
    if (!a.confirmed && !sendTrigger && !a.linkCheck)
        return;
    const Reception *best = bestReception(a, false);
    if (best == nullptr)
        return;
    auto payload = makeShared<RoamingPayload>();
    payload->setFrameType(sendTrigger ? RF_FORCE_REJOIN : RF_ACK);
    payload->setDevId(a.devId);
    payload->setNetId(netId);
    payload->setFcnt(a.fcnt);
    payload->setAck(a.confirmed);
    payload->setLinkCheck(a.linkCheck);
    if (counting() && a.linkCheck) linkCheckAnsSent++;
    if (sendTrigger) {
        payload->setRejoinSF(trig.sf);
        payload->setMaxRetries(trig.maxRetries);
        payload->setRetryPeriod(trig.period);
        trig.valid = false;
        if (counting()) forceRejoinSent++;
    }
    if (counting() && a.confirmed) acksSent++;
    double txTime, cf, tp; int sf;
    chooseWindow(best, txTime, sf, cf, tp);
    if (best->local)
        sendDownlinkToGw(best->gwAddr, a.mac, sf, cf, txTime, payload, tp);
    else {
        // passive roaming downlink through the fNS (XmitDataReq, 11.3.2): no retry on failure
        auto x = new BackendMsg("XmitDataReq");
        x->setBkType(BK_XMITDATA);
        x->setDevId(a.devId);
        x->setFromNet(netId);
        x->setToNet(best->fromNet);
        x->setFcnt(a.fcnt);
        x->setGwAddr(best->gwAddr.c_str());
        x->setSf(sf);
        x->setCf(cf);
        x->setTp(tp);
        x->setTxTime(txTime);
        x->setMacAddr(a.mac);
        x->setFrameType(payload->getFrameType());
        x->setAck(payload->getAck());
        x->setLinkCheck(payload->getLinkCheck());
        x->setRejoinSF(payload->getRejoinSF());
        x->setMaxRetries(payload->getMaxRetries());
        x->setRetryPeriod(payload->getRetryPeriod());
        if (counting()) dlViaRelay++;
        sendBackend(x, server(best->fromNet), par("backendDelay").doubleValue());
    }
}

void RoamingNetworkServerApp::startHandoverIn(const Key& key)
{
    auto it = aggs.find(key);
    UplinkAgg a = it->second;
    aggs.erase(it);
    const Reception *best = bestReception(a, true);
    if (best == nullptr) return;
    // HRStartReq to the hNS / Join Server and HRStartAns back (2 back-end messages)
    if (counting()) { bkSent[0] += 2; }
    auto t = new KeyTimer("joinAcceptReady", TIMER_ACCEPT);
    t->setDevId(a.devId);
    t->setFcnt(a.netId);              // old network
    t->setGwAddr(best->gwAddr.c_str());
    t->setMac(a.mac);
    double txTime, cf, tp; int sf;
    chooseWindow(best, txTime, sf, cf, tp);         // JOIN_ACCEPT_DELAY1 = RECEIVE_DELAY1 here
    t->setSf(sf);
    t->setCf(cf);
    t->setTxTime(txTime);
    t->setRxTimeTp(tp);
    t->setNewSession(++sessionCounter);
    scheduleAt(simTime() + par("hrDelay").doubleValue(), t);
}

void RoamingNetworkServerApp::handleTimer(cMessage *msg)
{
    auto t = check_and_cast<KeyTimer *>(msg);
    Key key(t->getDevId(), t->getFcnt(), t->getFrameType());
    switch (msg->getKind()) {
        case TIMER_LOCAL: onLocalCollected(key); break;
        case TIMER_PROCESS: processServed(key); break;
        case TIMER_DL: decideDownlink(key); break;
        case TIMER_ACCEPT: {
            auto payload = makeShared<RoamingPayload>();
            payload->setFrameType(RF_JOIN_ACCEPT);
            payload->setDevId(t->getDevId());
            payload->setNetId(netId);
            payload->setNewNetId(netId);
            payload->setNewSessionId(t->getNewSession());
            pendingSwitch[t->getDevId()] = std::make_pair(t->getFcnt(), t->getNewSession());
            if (counting()) joinAcceptsSent++;
            sendDownlinkToGw(t->getGwAddr(), t->getMac(), t->getSf(), t->getCf(), t->getTxTime(), payload, t->getRxTimeTp());
            break;
        }
    }
    delete msg;
}

void RoamingNetworkServerApp::handleBackend(BackendMsg *msg)
{
    switch (msg->getBkType()) {
        case BK_PRSTART: {
            Key key(msg->getDevId(), msg->getFcnt(), RF_DATA);
            Reception r;
            r.local = false;
            r.gwAddr = msg->getGwAddr();
            r.gwIndex = msg->getGwIndex();
            r.fromNet = msg->getFromNet();
            r.rssi = msg->getRssi();
            r.snir = msg->getSnir();
            r.rxTime = msg->getRxTime();
            r.sf = msg->getSf();
            r.cf = msg->getCf();
            auto it = aggs.find(key);
            if (it == aggs.end()) {
                UplinkAgg a;
                a.devId = msg->getDevId();
                a.mac = msg->getMacAddr();
                a.fcnt = msg->getFcnt();
                a.frameType = RF_DATA;
                a.netId = netId;
                a.sessionId = msg->getSessionId();
                a.confirmed = msg->getConfirmed();
                a.linkCheck = msg->getLinkCheck();
                a.x = msg->getXPos();
                a.y = msg->getYPos();
                a.firstRx = r.rxTime;
                a.recs.push_back(r);
                aggs[key] = a;
                // only relayed copies: process once the relay window is over
                double wait = std::max(0.0, a.firstRx + relayDedup - simTime().dbl());
                scheduleAt(simTime() + wait, keyTimer("process", TIMER_PROCESS, key));
            }
            else if (!it->second.processed)
                it->second.recs.push_back(r);
            else if (counting())
                relayedLate++;
            break;
        }
        case BK_XMITDATA: {
            auto payload = makeShared<RoamingPayload>();
            payload->setFrameType(msg->getFrameType());
            payload->setDevId(msg->getDevId());
            payload->setNetId(msg->getFromNet());
            payload->setFcnt(msg->getFcnt());
            payload->setAck(msg->getAck());
            payload->setLinkCheck(msg->getLinkCheck());
            payload->setRejoinSF(msg->getRejoinSF());
            payload->setMaxRetries(msg->getMaxRetries());
            payload->setRetryPeriod(msg->getRetryPeriod());
            sendDownlinkToGw(msg->getGwAddr(), msg->getMacAddr(), msg->getSf(), msg->getCf(), msg->getTxTime(), payload, msg->getTp());
            break;
        }
        case BK_TRIGGER: {
            PendingTrigger &t = triggers[msg->getDevId()];
            t.valid = true;
            t.sf = msg->getRejoinSF();
            t.maxRetries = msg->getMaxRetries();
            t.period = msg->getRetryPeriod();
            break;
        }
        case BK_SWITCH_DONE: {
            if (counting()) handoversOut++;
            triggers.erase(msg->getDevId());
            break;
        }
        default: break;
    }
    delete msg;
}

void RoamingNetworkServerApp::chooseWindow(const Reception *best, double &txTime, int &sf, double &cf, double &tp)
{
    static const double req[6] = {-7.5, -10, -12.5, -15, -17.5, -20};
    txTime = best->rxTime + receiveDelay1;
    sf = best->sf;
    cf = best->cf;
    tp = par("downlinkTP").doubleValue();
    double margin = best->snir - req[std::min(5, std::max(0, best->sf - 7))];
    if (margin < par("rx2SnrMargin").doubleValue()) {   // weak link: RX2 (SF12, 869.525 MHz, higher power)
        txTime += 1.0;
        sf = par("rx2SF");
        cf = par("rx2Frequency").doubleValue();
        tp = par("rx2TP").doubleValue();
    }
}

void RoamingNetworkServerApp::sendDownlinkToGw(const std::string& gwAddr, long mac, int sf, double cf, double txTime, const Ptr<RoamingPayload>& payload, double tpDbm)
{
    auto frame = makeShared<LoRaMacFrame>();
    frame->setChunkLength(B(par("headerLength").intValue()));
    frame->setReceiverAddress(MacAddress((uint64_t)mac));
    frame->setLoRaTP(math::dBmW2mW(tpDbm));
    frame->setLoRaCF(Hz(cf));
    frame->setLoRaSF(sf);
    frame->setLoRaBW(Hz(125e3));
    frame->setLoRaCR(4);
    frame->setLoRaUseHeader(true);
    frame->setTxTime(txTime);
    payload->setChunkLength(B(12));
    char pname[64]; snprintf(pname, sizeof(pname), "DL-%ld-%d-%d", mac, payload->getFcnt(), payload->getFrameType());
    auto pkt = new Packet(pname);
    pkt->insertAtFront(payload);
    pkt->insertAtFront(frame);
    L3Address dest;
    dest.tryParse(gwAddr.c_str());
    socket.sendTo(pkt, dest, destPort);
}

void RoamingNetworkServerApp::finish()
{
    for (auto &d : delivered) {
        recordScalar(("delivered " + std::to_string(d.first)).c_str(), d.second);
        recordScalar(("deliveredBorder " + std::to_string(d.first)).c_str(), deliveredBorder[d.first]);
        recordScalar(("deliveredRelayed " + std::to_string(d.first)).c_str(), deliveredRelayed[d.first]);
    }
    const char *names[8] = {"bkHRStart", "bkPRStart", "bkXmitData", "bkObs", "bkTrigger", "bkSwitchDone", "bkPrepare", "bk7"};
    for (int i = 0; i < 7; i++) recordScalar(names[i], bkSent[i]);
    recordScalar("forceRejoinSent", forceRejoinSent);
    recordScalar("acksSent", acksSent);
    recordScalar("linkCheckAnsSent", linkCheckAnsSent);
    recordScalar("joinAcceptsSent", joinAcceptsSent);
    recordScalar("dlViaRelay", dlViaRelay);
    recordScalar("rejoinsIgnored", rejoinsIgnored);
    recordScalar("relayedLate", relayedLate);
    recordScalar("foreignDropped", foreignDropped);
    recordScalar("handoversIn", handoversIn);
    recordScalar("handoversOut", handoversOut);
}

} // namespace flora
