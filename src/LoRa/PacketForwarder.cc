//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "PacketForwarder.h"
#include "LoRaGWMac.h"
//#include "inet/networklayer/ipv4/IPv4Datagram.h"
//#include "inet/networklayer/contract/ipv4/IPv4ControlInfo.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/ModuleAccess.h"
#include "inet/applications/base/ApplicationPacket_m.h"
#include "../LoRaPhy/LoRaRadioControlInfo_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"


namespace flora {

Define_Module(PacketForwarder);


void PacketForwarder::initialize(int stage)
{
    if (stage == 0) {
        LoRa_GWPacketReceived = registerSignal("LoRa_GWPacketReceived");
        localPort = par("localPort");
        destPort = par("destPort");
        rx2Offset = par("rx2Offset").doubleValue();
        rx2Fallback = par("rx2Fallback");
    } else if (stage == INITSTAGE_APPLICATION_LAYER) {
        startUDP();
        getSimulation()->getSystemModule()->subscribe("LoRa_AppPacketSent", this);
    }
}


void PacketForwarder::startUDP()
{
    EV << "Wywalamy sie tutaj" << endl;
    socket.setOutputGate(gate("socketOut"));
    const char *localAddress = par("localAddress");
    socket.bind(*localAddress ? L3AddressResolver().resolve(localAddress) : L3Address(), localPort);
    EV << "Dojechalismy za pierwszy resolv" << endl;
    // TODO: is this required?
    //setSocketOptions();

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;

    // Create UDP sockets to multiple destination addresses (network servers)
    while ((token = tokenizer.nextToken()) != nullptr) {
        EV << "Wchodze w petle" << endl;
        EV << token << endl;
        L3Address result;
        L3AddressResolver().tryResolve(token, result);
        EV << "Wychodze z petli" << endl;
        if (result.isUnspecified())
            EV_ERROR << "cannot resolve destination address: " << token << endl;
        else
            EV << "Got destination address: " << token << endl;
        destAddresses.push_back(result);
    }
    EV << "Dojechalismy do konca" << endl;
}


void PacketForwarder::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) { // held downlink whose transmit time has come
        transmitHeld(check_and_cast<Packet *>(msg));
        return;
    }
    EV << msg->getArrivalGate() << endl;
    if (msg->arrivedOn("lowerLayerIn")) {
        EV << "Received LoRaMAC frame" << endl;
        auto pkt = check_and_cast<Packet*>(msg);
        const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
        if(frame->getReceiverAddress() == MacAddress::BROADCAST_ADDRESS)
            processLoraMACPacket(pkt);
        //send(msg, "upperLayerOut");
        //sendPacket();
    } else if (msg->arrivedOn("socketIn")) {
        // FIXME : debug for now to see if LoRaMAC frame received correctly from network server
        EV << "Received UDP packet" << endl;
        auto pkt = check_and_cast<Packet*>(msg);
        const auto &frame = pkt->peekAtFront<LoRaMacFrame>();

        if (frame == nullptr)
            throw cRuntimeError("Packet type error");
        //EV << frame->getLoRaTP() << endl;
        //delete frame;

       /* auto loraTag = pkt->addTagIfAbsent<LoRaTag>();
        pkt->setBandwidth(loRaBW);
        pkt->setCarrierFrequency(loRaCF);
        pkt->setSpreadFactor(loRaSF);
        pkt->setCodeRendundance(loRaCR);
        pkt->setPower(W(loRaTP));*/

        scheduleDownlink(pkt);
    }
}

void PacketForwarder::toRx2(Packet *pkt)
{
    pkt->trimFront();
    auto frame = pkt->removeAtFront<LoRaMacFrame>();
    frame->setLoRaSF(par("rx2SF").intValue());
    frame->setLoRaCF(Hz(par("rx2Frequency").doubleValue()));
    frame->setLoRaTP(math::dBmW2mW(par("rx2TP").doubleValue()));
    pkt->insertAtFront(frame);
}

void PacketForwarder::transmitHeld(Packet *pkt)
{
    auto mac = check_and_cast<LoRaGWMac *>(getParentModule()->getSubmodule("LoRaGWNic")->getSubmodule("mac"));
    const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
    if (mac->bandAvailable(frame->getLoRaCF().get(), simTime())) {
        send(pkt, "lowerLayerOut");
        return;
    }
    if (rx2Fallback && pkt->getKind() == 1) {   // RX1 band busy: retry in RX2 one second later
        toRx2(pkt);
        pkt->setKind(2);
        downlinkRx2Fallback++;
        scheduleAt(simTime() + rx2Offset, pkt);
        return;
    }
    downlinkDroppedBand++;
    delete pkt;
}

void PacketForwarder::scheduleDownlink(Packet *pkt)
{
    const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
    double txTime = frame->getTxTime();
    if (txTime < 0) { // legacy behaviour: transmit immediately
        send(pkt, "lowerLayerOut");
        return;
    }
    if (pkt->getControlInfo())
        delete pkt->removeControlInfo();
    simtime_t now = simTime();
    bool isRx2 = std::fabs(frame->getLoRaCF().get() - par("rx2Frequency").doubleValue()) < 1e3;
    if (now <= txTime) {
        downlinkOnTime++;
        pkt->setKind(isRx2 ? 2 : 1);
        scheduleAt(txTime, pkt);
    }
    else if (now <= txTime + rx2Offset && !isRx2) {
        downlinkRx2++;
        if (rx2Fallback) toRx2(pkt);
        pkt->setKind(2);
        scheduleAt(txTime + rx2Offset, pkt);
    }
    else {
        downlinkTooLate++;
        delete pkt;
    }
}

void PacketForwarder::processLoraMACPacket(Packet *pk)
{
    // FIXME: Change based on new implementation of MAC frame.
    emit(LoRa_GWPacketReceived, 42);
    if (simTime() >= getSimulation()->getWarmupPeriod())
        counterOfReceivedPackets++;
    pk->trimFront();
    auto frame = pk->removeAtFront<LoRaMacFrame>();

    auto snirInd = pk->getTag<SnirInd>();

    auto signalPowerInd = pk->getTag<SignalPowerInd>();

    W w_rssi = signalPowerInd->getPower();
    double rssi = w_rssi.get()*1000;
    frame->setRSSI(math::mW2dBmW(rssi));
    frame->setSNIR(snirInd->getMinimumSnir());
    frame->setRxTime(simTime().dbl());
    frame->setGwId(getParentModule()->getIndex());
    pk->insertAtFront(frame);

    //bool exist = false;
    EV << frame->getTransmitterAddress() << endl;
    //for (std::vector<nodeEntry>::iterator it = knownNodes.begin() ; it != knownNodes.end(); ++it)

    // FIXME : Identify network server message is destined for.
    L3Address destAddr = destAddresses[0];
    if (pk->getControlInfo())
       delete pk->removeControlInfo();

    socket.sendTo(pk, destAddr, destPort);
}

void PacketForwarder::sendPacket()
{
//    LoRaAppPacket *mgmtCommand = new LoRaAppPacket("mgmtCommand");
//    mgmtCommand->setMsgType(TXCONFIG);
//    LoRaOptions newOptions;
//    newOptions.setLoRaTP(uniform(0.1, 1));
//    mgmtCommand->setOptions(newOptions);
//
//    LoRaMacFrame *response = new LoRaMacFrame("mgmtCommand");
//    response->encapsulate(mgmtCommand);
//    response->setLoRaTP(pk->getLoRaTP());
//    response->setLoRaCF(pk->getLoRaCF());
//    response->setLoRaSF(pk->getLoRaSF());
//    response->setLoRaBW(pk->getLoRaBW());
//    response->setReceiverAddress(pk->getTransmitterAddress());
//    send(response, "lowerLayerOut");

}

void PacketForwarder::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    if (simTime() >= getSimulation()->getWarmupPeriod())
        counterOfSentPacketsFromNodes++;
}

void PacketForwarder::finish()
{
    recordScalar("LoRa_GW_DER", counterOfSentPacketsFromNodes > 0 ? double(counterOfReceivedPackets)/counterOfSentPacketsFromNodes : 0);
    recordScalar("downlinkOnTime", downlinkOnTime);
    recordScalar("downlinkRx2", downlinkRx2);
    recordScalar("downlinkTooLate", downlinkTooLate);
    recordScalar("downlinkRx2Fallback", downlinkRx2Fallback);
    recordScalar("downlinkDroppedBand", downlinkDroppedBand);
}



} //namespace inet
