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

//based on Ieee802154NarrowbandMac

package flora.LoRa;

import inet.linklayer.csmaca.CsmaCaMac;

module LoRaMac extends CsmaCaMac
{
    parameters:
        bitrate = 250bps;
        double receiveDelay1 @unit(s) = default(1s); // RECEIVE_DELAY1 (5 s for roaming)
        double receiveWindow @unit(s) = default(1s); // listening duration of each window
        bool useRx2Params = default(false);          // HADEC: RX2 listens on SF12 / 869.525 MHz (EU868)
        int rx2SF = default(12);
        double rx2Frequency @unit(Hz) = default(869.525MHz);
        @class(LoRaMac);
    gates:
        input upperMgmtIn;
        output upperMgmtOut;
}
