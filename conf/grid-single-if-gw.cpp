// -*- c++ -*-
// grid-single-if-gw.cpp

// like grid-single-if-gw.click, but designed to be run through cpp

// use like: gcc -E -P grid2.cpp | click

#include "grid-node-info.h"
#include "grid-gw-info.h"

// device interface
eth :: PacketSocket(NET_DEVICE, 0)
to_eth :: Queue -> eth

// IP interfaces on gateway machine
tun1 :: Tun(TUN_DEVICE, GW_IP, GW_NETMASK) // gateway's regular address
to_tun1 :: Queue -> tun1

tun2 :: Tun(TUN_DEVICE, GRID_IP, GRID_NETMASK) // gateway's grid address
to_tun2 :: Queue -> tun2

// get IP for 18.26.4.25, 18.26.7.1, 18.26.7.*, *
ip_cl :: Classifier(GW_IP_HEX, GW_GRID_IP_HEX, GRID_NET_HEX, -) 

nb :: Neighbor(NBR_TIMEOUT, MAC_ADDR, GRID_IP)
nb [0] -> to_eth
nb [1] -> ip_cl

eth -> eth_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			       12/0806 20/0002, // arp replies 
			       12/0800 GW_IP_HEX_ETHER, // ip for us as 18.26.4.25
			       12/0800 GW_GRID_IP_HEX_ETHER, // ip for us as 18.26.7.1
			       12/0800 GW_GRID_NET_HEX_ETHER, // ip for 18.26.7.*
			       GRID_ETH_PROTO) // grid protocol	

eth_demux [0] -> ARPResponder(GW_GRID_NET_ADDR GW_IP MAC_ADDR) -> to_eth
eth_demux [1] -> [1] arpq :: ARPQuerier(GW_IP, MAC_ADDR) -> to_eth
eth_demux [2] -> Strip(14) -> Discard // linux handles 
Idle -> to_tun1
eth_demux [3] -> Strip(14) -> Discard // linux handles -> to_tun2
eth_demux [4] -> Strip(14) -> to_nb_ip :: GetIPAddress(16) -> [1] nb
eth_demux [5] -> [0] nb


ip_cl [0] -> to_tun1
ip_cl [1] -> to_tun2
ip_cl [2] -> to_nb_ip // send 18.26.7.* to Grid processing
ip_cl [3] -> gw_cl :: Classifier(GW_NET_HEX, -) // get local wired IP for 18.26.4.*
gw_cl [0] -> GetIPAddress(16) -> [0] arpq // ARP and send local net traffic
gw_cl [1] -> SetIPAddress(GW_REAL_GW_IP) -> [0] arpq // ARP and send gateway traffic

tun1 -> ip_cl
tun2 -> ip_cl

h :: Hello(HELLO_PERIOD, HELLO_JITTER, MAC_ADDR, GRID_IP) -> to_eth
