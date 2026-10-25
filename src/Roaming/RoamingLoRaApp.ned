package flora.Roaming;

import inet.applications.contract.IApp;

// End-device application for HADEC experiments: periodic (optionally confirmed) uplinks,
// LoRaWAN 1.1 handover roaming (ForceRejoinReq -> Rejoin-request type 0 -> join-accept).
simple RoamingLoRaApp like IApp
{
    parameters:
        @class(RoamingLoRaApp);
        int homeNetId = default(0);
        bool lorawan11 = default(true);             // 1.0.x devices ignore ForceRejoinReq and never rejoin
        volatile double timeToFirstPacket @unit(s) = default(uniform(5s, 60s));
        volatile double timeToNextPacket @unit(s) = default(60s);
        double confirmedProbability = default(1.0);
        double initialLoRaTP @unit(dBm) = default(14dBm);
        double initialLoRaCF @unit(Hz) = default(868MHz);
        int initialLoRaSF = default(10);
        double initialLoRaBW @unit(Hz) = default(125kHz);
        int initialLoRaCR = default(4);
        int dataSize @unit(B) = default(20B);
        double receiveDelay1 @unit(s) = default(5s);  // must match the MAC
        bool autonomousRejoin = default(false);      // reactive baseline (device-initiated)
        double nominalPeriod @unit(s) = default(60s);  // mean uplink period, reference for service interruption
        int missLimit = default(3);                  // unanswered confirmed uplinks before an autonomous rejoin
        int linkCheckPeriod = default(0);             // LinkCheckReq every N uplinks (0 = off)
        int linkCheckMissLimit = default(2);          // unanswered LinkCheckReq before an autonomous rejoin
        int periodicRejoinCount = default(0);         // RejoinParamSetupReq MaxCountN: rejoin type 0 every N uplinks (0 = off)
        int autonomousMaxRetries = default(0);
        double autonomousRetryPeriod @unit(s) = default(32s);
        double frontierX @unit(m) = default(10000m);  // instrumentation only
        double borderHalfWidth @unit(m) = default(2000m);
        @signal[uplinkSent](type=double);
        @signal[networkSwitch](type=double);
        @statistic[networkSwitch](source=networkSwitch; record=vector);
    gates:
        input socketIn;
        output socketOut;
}
