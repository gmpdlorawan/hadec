package flora.Roaming;

// Edge Coordination Node (HADEC). Receives radio observations from every network server,
// decides whether and when a device should move to handover roaming, and asks the
// serving NS to send ForceRejoinReq. The strategy parameter selects the compared rules.
simple Ecn
{
    parameters:
        @class(Ecn);
        // none | naiveHear | naiveLose | hysteresis | hadec1 | hadec2 | fall | oracle
        string strategy = default("hadec2");
        string serverPaths;                      // NS app module paths indexed by NetID
        string gwNets;                           // NetID of each gateway, e.g. "0 0 1 1"
        string nodesPath = default("loRaNodes"); // for the oracle (trace replay)
        string traceFile = default("");          // BonnMotion trace used by the nodes (oracle)
        volatile double ecnDelay @unit(s) = default(uniform(10ms, 30ms));
        double evalDelay @unit(s) = default(200ms);
        double pendingHold @unit(s) = default(150s); // no new trigger while a rejoin is in progress
        double maxHold @unit(s) = default(1200s);    // the hold doubles after each failed attempt (common to all strategies)
        double tBloc @unit(s) = default(0s);          // no return trigger after a switch
        bool sendPrepare = default(true);
        bool commonRejoinSF = default(false);    // fairness: all rules choose the rejoin SF from network o's mean RSSI
        int maxRetries = default(1);
        double retryPeriod @unit(s) = default(32s);
        int dataSF = default(10);
        // path-loss model used by fall / oracle (mean, no shadowing)
        double plB = default(128.95);
        double plN = default(2.32);
        double plD0 @unit(m) = default(1000m);
        // naive rules
        double rssiThreshold @unit(dBm) = default(-125dBm);
        double hystMargin @unit(dB) = default(6dB);
        int hystCount = default(2);
        // first HADEC version (not used by default)
        double v1Alpha = default(0.3);
        double v1TRssi @unit(dBm) = default(-80dBm);
        double v1Gamma @unit(dB) = default(-2dB);
        double v1Beta = default(0.65);
        double v1K = default(0.1);
        double v1Sigma @unit(dB) = default(7.5dB);
        double v1W1 = default(0.5);
        double v1W2 = default(0.3);
        double v1RssiMin @unit(dBm) = default(-137dBm);
        double v1RssiMax @unit(dBm) = default(-80dBm);
        double v1SnrMin @unit(dB) = default(-20dB);
        double v1SnrMax @unit(dB) = default(10dB);
        double v1History @unit(s) = default(600s);
        // HADEC v2
        double sigmaShadow @unit(dB) = default(7.8dB);
        double margin @unit(dB) = default(3dB);        // H
        double beta = default(0.5);
        double gamma = default(0.5);
        double lossMargin @unit(dB) = default(3dB);    // m
        double tauW @unit(s) = default(600s);
        int minPoints = default(3);
        bool dwellTest = default(false);
        double dStar @unit(s) = default(600s);
        bool adaptRejoinSF = default(true);
        double drMargin @unit(dB) = default(5dB);
        double reachMargin @unit(dB) = default(3dB);  // m_B : margin above sensitivity for the rejoin
        double sfConfidence = default(0.9);           // rejoin SF: lowest SF reached with this probability
        double minReach = default(0.5);               // last-chance branch: minimum probability that B decodes the rejoin
        double censorOffset @unit(dB) = default(4dB);
        string calibFile = default("");                 // instrumentation : prédictions et écarts observés (CSV)  // missed frame -> pseudo-observation S - offset
        // Fall et al. adaptation
        double sigmaPos @unit(m) = default(0m);
        // oracle
        double oracleStep @unit(s) = default(10s);
        double oracleMargin @unit(dB) = default(3dB);
    gates:
        input backendIn @directIn;
}
