package flora.LoRaPhy;

import inet.physicallayer.wireless.common.pathloss.FreeSpacePathLoss;

// Log-distance path loss of Oulu (Petajajarvi et al., 2015) with RECIPROCAL, spatially
// correlated shadowing: the shadowing term depends on the pair of locations (quantised on
// a grid of cellSize), so an uplink and the downlink answering it see the same shadowing,
// and shadowing decorrelates when the device moves by more than cellSize. A small
// independent per-packet term models fast fading.
module LoRaPathLossCorrelated extends FreeSpacePathLoss
{
    parameters:
        double d0 = default(1000m) @unit(m);
        double n = default(2.32);
        double B = default(128.95);
        double sigma = default(7.8);        // shadowing (dB), correlated
        double sigmaFast = default(1.0);    // per-packet term (dB)
        double cellSize = default(100m) @unit(m);
        double rhoLinks = default(0);       // correlation of shadowing between the links of one device position to different gateways (0: independent)
        double antennaGain = default(2);
        @class(LoRaPathLossCorrelated);
}
