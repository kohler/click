// grid-gateway2.click

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);

li :: LocationInfo(POS_LAT, POS_LON)

rh :: ReadHandlerCaller(1) 

// device interface
eth :: FromDevice(GW_NET_DEVICE, 0)
to_eth :: ToDevice(GW_NET_DEVICE)

wvlan :: FromDevice(WI_NET_DEVICE, 0)
to_wvlan :: FixSrcLoc(li) -> SetGridChecksum -> ToDevice(WI_NET_DEVICE)

// IP interfaces on gateway machine
tun1 :: Tun(TUN_DEVICE_PREFIX, GW_IP, GW_NETMASK, GW_REAL_IP) // gateway's regular address
to_tun1 :: Queue -> tun1

tun2 :: Tun(TUN_DEVICE_PREFIX, GRID_IP, GRID_NETMASK) // gateway's grid address
to_tun2 :: Queue -> tun2

// get IP for this machine's wired address, its grid address, any grid node, *
ip_cl :: Classifier(16/GW_HEX_IP, // ip for us as wired node
	            16/GRID_HEX_IP, // ip for us as grid node 
		    16/GRID_NET_HEX, // ip for our grid net
		    -) // ip for the wired net or outside world

nb :: Neighbor(NBR_TIMEOUT, LR_PERIOD, LR_JITTER, GRID_MAC_ADDR, GRID_IP)
lr :: LocalRoute(GRID_MAC_ADDR, GRID_IP, nb)
lr [0] -> to_wvlan
lr [1] -> ip_cl

lr [2] -> Discard // don't know where to route these
lr [3] -> Discard // too many hops, or bad protocol 

eth -> eth_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			       12/0806 20/0002, // arp replies 
			       12/0800 30/GW_HEX_IP, // ip for us as on wire
			       12/0800 30/GRID_HEX_IP, // ip for us as grid node
			       12/0800 30/GRID_NET_HEX) // ip for 18.26.7.*

wvlan -> wvlan_demux :: Classifier(12/GRID_ETH_PROTO, -)

eth_demux [0] -> ARPResponder(GW_IP GW_MAC_ADDR,
		              GRID_NET/24 GW_MAC_ADDR) -> to_eth
eth_demux [1] -> [1] arpq :: ARPQuerier(GW_IP, GW_MAC_ADDR) -> to_eth
eth_demux [2] -> Strip(14) -> Discard // linux handles 
Idle -> to_tun1
eth_demux [3] -> Strip(14) -> Discard // linux handles -> to_tun2
eth_demux [4] -> Strip(14) -> to_nb_ip :: GetIPAddress(16) -> [1] lr

wvlan_demux [0] -> check_grid :: CheckGridHeader [0]
                -> fr :: FilterByRange(RANGE, li) [0] 
                -> nb
                -> Classifier(15/GRID_NBR_ENCAP_PROTO)
                -> [0] lr
check_grid [1]-> Print(bad_grid_hdr) -> Discard
fr [1] -> Discard // out of range
wvlan_demux [1] -> Discard // not a grid packet

ip_cl [0] -> to_tun1
ip_cl [1] -> to_tun2
ip_cl [2] -> to_nb_ip // send 18.26.7.* to Grid processing
ip_cl [3] -> gw_cl :: Classifier(16/GW_HEX_NET, -) // get local wired IP for 18.26.4.*
gw_cl [0] -> Print(for_this_net) -> GetIPAddress(16) -> [0] arpq // ARP and send local net traffic
gw_cl [1] -> Print(for_gw) -> SetIPAddress(GW_REAL_IP) -> [0] arpq // ARP and send gateway traffic

tun1 -> ip_cl
tun2 -> ip_cl

nb [1] -> to_wvlan // routing Hello packets

h :: Hello(HELLO_PERIOD, HELLO_JITTER, GRID_MAC_ADDR, GRID_IP) -> to_wvlan




