//
// HADEC v2 - messages for multi-operator LoRaWAN roaming (passive and handover)
//
import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;

namespace flora;

enum RoamingFrameType
{
    RF_DATA = 1;          // uplink data frame (session of netId)
    RF_ACK = 2;           // downlink acknowledgement
    RF_FORCE_REJOIN = 3;  // downlink ForceRejoinReq (optionally carries an ACK)
    RF_REJOIN_REQ = 4;    // uplink Rejoin-request type 0
    RF_JOIN_ACCEPT = 5;   // downlink join-accept answering a rejoin
}

// LoRaWAN payload as seen by the network (MAC header fields + simulation-only instrumentation)
class RoamingPayload extends inet::FieldsChunk
{
    int frameType @enum(RoamingFrameType);
    long devId;            // stable device identifier (DevEUI); MAC address as integer
    int netId;             // NetID of the session used by the frame
    int sessionId;
    int fcnt;              // uplink counter
    bool confirmed;
    int homeNetId;
    bool ack;              // downlink carries an ACK
    bool linkCheck;        // uplink carries LinkCheckReq / downlink carries LinkCheckAns
    // ForceRejoinReq parameters (LoRaWAN 1.1, 5.13): DR (as SF), retries, period
    int rejoinSF = -1;
    int maxRetries = 0;
    double retryPeriod = 0;
    // join-accept
    int newNetId = -1;
    int newSessionId = -1;
    // instrumentation only (never used by network-side algorithms)
    double xPos;
    double yPos;
}

enum BackendType
{
    BK_PRSTART = 1;       // fNS -> sNS : relayed uplink + ULMetadata (passive roaming)
    BK_XMITDATA = 2;      // sNS -> fNS : downlink to transmit through the fNS gateway
    BK_OBS = 3;           // NS -> ECN : radio observation of an uplink
    BK_TRIGGER = 4;       // ECN -> sNS : send ForceRejoinReq
    BK_SWITCH_DONE = 5;   // new sNS -> old sNS and ECN : session moved
    BK_PREPARE = 6;       // ECN -> target NS : profile exchange (signalling only)
}

message BackendMsg
{
    int bkType @enum(BackendType);
    long devId;
    int fromNet;
    int toNet;
    int fcnt;
    int sessionId;
    bool confirmed;
    double rxTime;
    int sf;
    double cf = 868e6;
    // ULMetadata / observation
    double rssi = -999;
    double snir = -999;
    int gwIndex = -1;        // ULToken: gateway of the sender network
    string gwAddr;           // ULToken: L3 address of that gateway
    bool fromServing;        // observation made by the serving network
    // downlink content (XMITDATA)
    int frameType;
    bool linkCheck;
    bool ack;
    int rejoinSF = -1;
    int maxRetries = 0;
    double retryPeriod = 0;
    double txTime;
    double tp = 14;
    long macAddr;
    // instrumentation
    double xPos;
    double yPos;
}

// internal timer identifying one uplink (device, counter, frame type)
message KeyTimer
{
    long devId;
    int fcnt;
    int frameType;
    string gwAddr;
    long mac;
    int sf;
    double cf;
    double txTime;
    double rxTimeTp = 14;
    int newSession;
}
