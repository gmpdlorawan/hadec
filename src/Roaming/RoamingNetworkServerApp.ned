package flora.Roaming;

import inet.applications.contract.IApp;

// Network server of one operator. Plays the sNS role for devices whose session NetID is
// its own, and the fNS role for foreign devices heard by its gateways (Backend Interfaces 1.0,
// passive roaming 11.3, handover roaming 11.4). Back-end links are modelled as direct messages
// with random delays.
simple RoamingNetworkServerApp like IApp
{
    parameters:
        @class(RoamingNetworkServerApp);
        int netId;
        int localPort = default(1000);
        int destPort = default(2000);                  // packet forwarder port on gateways
        string serverPaths;                            // module paths of all NS apps, indexed by NetID
        string ecnModule = default("");                // "" : no ECN
        bool passiveAgreement = default(true);
        bool activeAgreement = default(true);
        bool reportForeignToEcn = default(true);       // fNS reports what it hears to the ECN
        double receiveDelay1 @unit(s) = default(5s);
        double localDedup @unit(s) = default(100ms);   // collect copies from own gateways
        double relayDedup @unit(s) = default(600ms);   // wait for relayed copies (PRStartReq)
        double dlDecisionDelay @unit(s) = default(1s); // downlink built 1 s after reception
        volatile double backendDelay @unit(s) = default(uniform(150ms, 200ms));  // one-way NS-NS
        volatile double hrDelay @unit(s) = default(uniform(283ms, 362ms));       // HRStartReq/Ans + JS (Hamnache 2022)
        volatile double ecnDelay @unit(s) = default(uniform(10ms, 30ms));        // one-way NS-ECN
        double downlinkTP @unit(dBm) = default(14dBm);
        double rx2SnrMargin @unit(dB) = default(-999dB);   // uplink SNR margin below which RX2 is targeted directly
        int rx2SF = default(12);
        double rx2Frequency @unit(Hz) = default(869.525MHz);
        double rx2TP @unit(dBm) = default(27dBm);
        double frontierX @unit(m) = default(10000m);   // instrumentation only
        double borderHalfWidth @unit(m) = default(2000m);
        int headerLength @unit(B) = default(13B);
    gates:
        input socketIn;
        output socketOut;
        input backendIn @directIn;
}
