// grid-gateway.cpp

// use like: gcc -E -P grid-gateway.cpp | click

#include "grid-node-info.h"
#include "grid-gw-info.h"

LocationInfo(0, 0)

// device interface
eth :: FromDevice(GW_NET_DEVICE, 0)
to_eth :: ToDevice(GW_NET_DEVICE)

wvlan :: FromDevice(NET_DEVICE, 1)
to_wvlan :: FixSrcLoc -> SetGridChecksum -> ToDevice(NET_DEVICE)

// IP interfaces on gateway machine
tun1 :: Tun(TUN_DEVICE, GW_IP, GW_NETMASK) // gateway's regular address
to_tun1 :: Queue -> tun1

tun2 :: Tun(TUN_DEVICE, GRID_IP, GRID_NETMASK) // gateway's grid address
to_tun2 :: Queue -> tun2

// get IP for this machine's wired address, its grid address, any grid node, *
ip_cl :: Classifier(GW_IP_HEX, // ip for us as wired node
	            GRID_IP_HEX, // ip for us as grid node 
		    GRID_NET_HEX, // ip for our grid net
		    -) // ip for the wired net or outside world

nb :: Neighbor(NBR_TIMEOUT, MAC_ADDR, GRID_IP)
nb [0] -> to_wvlan
nb [1] -> ip_cl

eth -> eth_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			       12/0806 20/0002, // arp replies 
			       12/0800 GW_IP_HEX_ETHER, // ip for us as on wire
			       12/0800 GW_GRID_IP_HEX_ETHER, // ip for us as grid node
			       12/0800 GW_GRID_NET_HEX_ETHER) // ip for 18.26.7.*

wvlan -> wvlan_demux :: Classifier(GRID_ETH_PROTO, -)

// the ARPResponder docs and example don't seem to agree with the implementation...
eth_demux [0] -> ARPResponder(GW_IP GW_MAC_ADDR,
		              GW_GRID_NET_ADDR GW_MAC_ADDR) -> to_eth
eth_demux [1] -> [1] arpq :: ARPQuerier(GW_IP, GW_MAC_ADDR) -> to_eth
eth_demux [2] -> Strip(14) -> Discard // linux handles 
Idle -> to_tun1
eth_demux [3] -> Strip(14) -> Discard // linux handles -> to_tun2
eth_demux [4] -> Strip(14) -> to_nb_ip :: GetIPAddress(16) -> [1] nb

wvlan_demux [0] -> check_grid :: CheckGridHeader [0]
                -> fr :: FilterByRange(1000) [0] 
                -> [0] nb
check_grid [1]-> Print(bad_grid_hdr) -> Discard
fr [1] -> Print(out_of_range) -> Discard
wvlan_demux [1] -> Discard

ip_cl [0] -> to_tun1
ip_cl [1] -> to_tun2
ip_cl [2] -> to_nb_ip // send 18.26.7.* to Grid processing
ip_cl [3] -> gw_cl :: Classifier(GW_NET_HEX, -) // get local wired IP for 18.26.4.*
gw_cl [0] -> GetIPAddress(16) -> [0] arpq // ARP and send local net traffic
gw_cl [1] -> SetIPAddress(GW_REAL_GW_IP) -> [0] arpq // ARP and send gateway traffic

tun1 -> ip_cl
tun2 -> ip_cl

h :: Hello(HELLO_PERIOD, HELLO_JITTER, MAC_ADDR, GRID_IP) -> to_wvlan




