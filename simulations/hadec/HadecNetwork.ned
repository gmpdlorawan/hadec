package flora.simulations.hadec;

import inet.node.inet.Router;
import inet.node.internetcloud.InternetCloud;
import inet.node.inet.StandardHost;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth1G;
import flora.LoRaPhy.LoRaMedium;
import flora.LoraNode.LoRaNode;
import flora.LoraNode.LoRaGW;
import flora.Roaming.Ecn;

// Two LoRaWAN operators (A = NetID 0, B = NetID 1), their gateways and network servers,
// and the HADEC edge coordination node.
network HadecNetwork
{
    parameters:
        int numberOfNodes = default(100);
        int numberOfGateways = default(4);
        @display("bgb=800,500");
    submodules:
        loRaNodes[numberOfNodes]: LoRaNode;
        loRaGW[numberOfGateways]: LoRaGW;
        LoRaMedium: LoRaMedium;
        networkServer[2]: StandardHost;
        ecn: Ecn;
        configurator: Ipv4NetworkConfigurator {
            parameters:
                assignDisjunctSubnetAddresses = false;
        }
        internetCloud: InternetCloud;
        gwRouter[numberOfGateways]: Router;
        nsRouter[2]: Router;
    connections:
        for i=0..1 {
            networkServer[i].ethg++ <--> Eth1G <--> nsRouter[i].ethg++;
            nsRouter[i].pppg++ <--> Eth1G <--> internetCloud.pppg++;
        }
        for i=0..numberOfGateways-1 {
            internetCloud.pppg++ <--> Eth1G <--> gwRouter[i].pppg++;
            gwRouter[i].ethg++ <--> Eth1G <--> loRaGW[i].ethg++;
        }
}
