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

#include "LoRaGWMac.h"
#include "inet/common/ModuleAccess.h"
#include "../LoRaPhy/LoRaPhyPreamble_m.h"
#include "inet/common/ProtocolTag_m.h"


#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"


namespace flora {

Define_Module(LoRaGWMac);

void LoRaGWMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        // subscribe for the information of the carrier sense
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        //radioModule->subscribe(IRadio::radioModeChangedSignal, this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radio = check_and_cast<IRadio *>(radioModule);
        waitingForDC = false;
        dutyCycleTimer = new cMessage("Duty Cycle Timer");
        const char *addressString = par("address");
        GW_forwardedDown = 0;
        GW_droppedDC = 0;
        euDutyCycle = par("euDutyCycle");
        dcRx1Band = par("dutyCycleRx1Band");
        dcRx2Band = par("dutyCycleRx2Band");
        rx2Frequency = par("rx2Frequency").doubleValue();
        if (!strcmp(addressString, "auto")) {
            // assign automatic address
            address = MacAddress::generateAutoAddress();
            // change module parameter from "auto" to concrete address
            par("address").setStringValue(address.str().c_str());
        }
        else
            address.setAddress(addressString);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        radio->setRadioMode(IRadio::RADIO_MODE_TRANSCEIVER);
    }
}

void LoRaGWMac::finish()
{
    recordScalar("GW_forwardedDown", GW_forwardedDown);
    recordScalar("GW_droppedDC", GW_droppedDC);
    recordScalar("GW_sentRx2", GW_sentRx2);
    cancelAndDelete(dutyCycleTimer);
}


void LoRaGWMac::configureNetworkInterface()
{
    //NetworkInterface *e = new NetworkInterface(this);

    // data rate
    //e->setDatarate(bitrate);

    // generate a link-layer address to be used as interface token for IPv6
    //e->setMACAddress(address);
    //e->setInterfaceToken(address.formInterfaceIdentifier());

    // capabilities
    //e->setMtu(par("mtu"));
    //e->setMulticast(true);
    //e->setBroadcast(true);
    //e->setPointToPoint(false);
    MacAddress address = parseMacAddressParameter(par("address"));

    // generate a link-layer address to be used as interface token for IPv6
    networkInterface->setMacAddress(address);
    // data rate
    //interfaceEntry->setDatarate(bitrate);

    // capabilities
    networkInterface->setMtu(par("mtu"));
    networkInterface->setMulticast(true);
    networkInterface->setBroadcast(true);
    networkInterface->setPointToPoint(false);
}

void LoRaGWMac::handleSelfMessage(cMessage *msg)
{
    if(msg == dutyCycleTimer) waitingForDC = false;
}

double LoRaGWMac::airtime(int sf, double bwHz, int cr, int payloadBytes)
{
    // Semtech AN1200.13, explicit header, CRC on (downlinks have no CRC, kept for simplicity)
    double ts = std::pow(2.0, sf) / bwHz;
    int de = (sf >= 11 && bwHz <= 125e3) ? 1 : 0;
    double num = 8.0 * payloadBytes - 4.0 * sf + 28 + 16;
    double n = 8 + std::max(std::ceil(num / (4.0 * (sf - 2 * de))) * (cr + 4), 0.0);
    return (8 + 4.25) * ts + n * ts;
}

bool LoRaGWMac::bandAvailable(double cfHz, simtime_t at) const
{
    if (!euDutyCycle) return !waitingForDC;
    bool rx2Band = std::fabs(cfHz - rx2Frequency) < 1e3;
    if (rx2Band) return at >= busyUntilRx2Band;
    return at >= (cfHz >= 868.0e6 ? busyUntilRx1BandHigh : busyUntilRx1Band);
}

void LoRaGWMac::handleUpperMessage(cMessage *msg)
{
    if (euDutyCycle) {
        auto pkt = check_and_cast<Packet *>(msg);
        const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
        double cf = frame->getLoRaCF().get();
        if (!bandAvailable(cf, simTime())) {
            GW_droppedDC++;
            delete msg;
            return;
        }
        if (pkt->getControlInfo())
            delete pkt->removeControlInfo();
        auto tag = pkt->addTagIfAbsent<MacAddressReq>();
        tag->setDestAddress(frame->getReceiverAddress());
        double toa = airtime(frame->getLoRaSF(), frame->getLoRaBW().get(), frame->getLoRaCR(), B(pkt->getTotalLength()).get());
        bool rx2Band = std::fabs(cf - rx2Frequency) < 1e3;
        if (rx2Band) { busyUntilRx2Band = simTime() + toa / dcRx2Band; GW_sentRx2++; }
        else if (cf >= 868.0e6) busyUntilRx1BandHigh = simTime() + toa / dcRx1Band;
        else busyUntilRx1Band = simTime() + toa / dcRx1Band;
        GW_forwardedDown++;
        pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);
        sendDown(pkt);
        return;
    }
    if(waitingForDC == false)
    {
//        LoRaMacFrame *frame = check_and_cast<LoRaMacFrame *>(msg);
//        frame->removeControlInfo();
        auto pkt = check_and_cast<Packet *>(msg);
        const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
        if (pkt->getControlInfo())
            delete pkt->removeControlInfo();

        auto tag = pkt->addTagIfAbsent<MacAddressReq>();
        tag->setDestAddress(frame->getReceiverAddress());
//        LoRaMacControlInfo *ctrl = new LoRaMacControlInfo();
//        ctrl->setSrc(address);
//        ctrl->setDest(frame->getReceiverAddress());
//        frame->setControlInfo(ctrl);
//        sendDown(frame);


        waitingForDC = true;
        double delta;
        if(frame->getLoRaSF() == 7) delta = 0.61696;
        if(frame->getLoRaSF() == 8) delta = 1.23392;
        if(frame->getLoRaSF() == 9) delta = 2.14016;
        if(frame->getLoRaSF() == 10) delta = 4.28032;
        if(frame->getLoRaSF() == 11) delta = 7.24992;
        if(frame->getLoRaSF() == 12) delta = 14.49984;
        scheduleAt(simTime() + delta, dutyCycleTimer);
        GW_forwardedDown++;
        pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);
        sendDown(pkt);
    }
    else
    {
        GW_droppedDC++;
        delete msg;
    }
}

void LoRaGWMac::handleLowerMessage(cMessage *msg)
{
    auto pkt = check_and_cast<Packet *>(msg);
    auto header = pkt->popAtFront<LoRaPhyPreamble>();
    const auto &frame = pkt->peekAtFront<LoRaMacFrame>();
    if(frame->getReceiverAddress() == MacAddress::BROADCAST_ADDRESS)
        sendUp(pkt);
    else
        delete pkt;
}

void LoRaGWMac::sendPacketBack(Packet *receivedFrame)
{
    const auto &frame = receivedFrame->peekAtFront<LoRaMacFrame>();
    EV << "sending Data frame back" << endl;
    auto pktBack = new Packet("LoraPacket");
    auto frameToSend = makeShared<LoRaMacFrame>();
    frameToSend->setChunkLength(B(par("headerLength").intValue()));

    frameToSend->setReceiverAddress(frame->getTransmitterAddress());
    pktBack->insertAtFront(frameToSend);
    sendDown(pktBack);
}

void LoRaGWMac::createFakeLoRaMacFrame()
{

}

void LoRaGWMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method_Silent();
    if (signalID == IRadio::transmissionStateChangedSignal) {
        IRadio::TransmissionState newRadioTransmissionState = (IRadio::TransmissionState)value;
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && newRadioTransmissionState == IRadio::TRANSMISSION_STATE_IDLE) {
            //transmissin is finished
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
        }
        transmissionState = newRadioTransmissionState;
    }
}

MacAddress LoRaGWMac::getAddress()
{
    return address;
}

}
