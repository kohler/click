// grid-gateway.click
// to be used with tools/run-grid-gateway

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);

li :: LocationInfo(POS_LAT, POS_LON);

ls :: SimpleLocQuerier(LOC_DB);

// device interface
eth :: FromDevice(GW_NET_DEVICE, 0);
to_eth :: ToDevice(GW_NET_DEVICE);

wvlan :: FromDevice(WI_NET_DEVICE, 0);
to_wvlan :: FixSrcLoc(li) -> SetGridChecksum -> ToDevice(WI_NET_DEVICE);

// IP interfaces on gateway machine
tun1 :: KernelTap(GW_IP/GW_NETMASK, GW_REAL_IP); // gateway's regular address
to_tun1 :: Queue -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> tun1;
tun1 -> from_tun1 :: Strip(14);

tun2 :: KernelTap(GRID_IP/GRID_NETMASK, 0.0.0.0, HEADROOM); // gateway's grid address
to_tun2 :: Queue -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> tun2;
tun2 -> from_tun2 :: Strip(14);

// get IP for this machine's wired address, its grid address, any grid node, *
ip_cl :: Classifier(16/GW_HEX_IP, // ip for us as wired node
	            16/GRID_HEX_IP, // ip for us as grid node 
		    16/GRID_NET_HEX, // ip for our grid net
		    -); // ip for the wired net or outside world

nb :: UpdateGridRoutes(NBR_TIMEOUT, LR_PERIOD, LR_JITTER, GRID_MAC_ADDR, GRID_IP, NUM_HOPS);
lr :: LookupLocalGridRoute(GRID_MAC_ADDR, GRID_IP, nb);
geo :: LookupGeographicGridRoute(GRID_MAC_ADDR, GRID_IP, nb);

lr [0] -> to_wvlan;
lr [1] -> ip_cl;

lr [2] -> ls -> [0] geo; // for geographic forwarding
lr [3] -> Discard; // too many hops, or bad protocol 

geo [0] -> to_wvlan;
geo [1] -> Discard; // can't handle
geo [2] -> Discard; // bad packet

eth -> eth_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			       12/0806 20/0002, // arp replies 
			       12/0800 30/GW_HEX_IP, // ip for us as on wire
			       12/0800 30/GRID_HEX_IP, // ip for us as grid node
			       12/0800 30/GRID_NET_HEX); // ip for Grid net

wvlan -> wvlan_demux :: Classifier(12/GRID_ETH_PROTO, -);

eth_demux [0] -> ARPResponder(GW_IP GW_MAC_ADDR,
		              GRID_NET/24 GW_MAC_ADDR) -> to_eth;
eth_demux [1] -> [1] arpq :: ARPQuerier(GW_IP, GW_MAC_ADDR) -> to_eth;
eth_demux [2] -> Strip(14) -> Discard; // linux picks up for us XXX but BSD?
Idle -> to_tun1;
eth_demux [3] -> Strip(14) -> Discard; // linux picks up for us XXX but BSD?
eth_demux [4] -> Strip(14) -> to_nb_ip :: GetIPAddress(16) -> [1] lr;

wvlan_demux [0] 
-> check_grid :: CheckGridHeader [0]
// -> fr :: FilterByRange(RANGE, li) [0]
-> nb 
-> Classifier(15/GRID_NBR_ENCAP_PROTO)
-> [0] lr;

check_grid [1]-> Print(bad_grid_hdr) -> Discard;
// fr [1] -> Discard; // out of range
wvlan_demux [1] -> Discard; // not a grid packet

ip_cl [0] -> to_tun1;
ip_cl [1] -> to_tun2;
ip_cl [2] -> to_nb_ip; // send grid net packets to Grid processing
ip_cl [3] -> gw_cl :: Classifier(16/GW_HEX_NET, -); // get local wired IP net traffic
gw_cl [0] -> GetIPAddress(16) -> [0] arpq; // ARP and send local net traffic
gw_cl [1] -> SetIPAddress(GW_REAL_IP) -> [0] arpq; // ARP and send gateway traffic

from_tun1 -> ip_cl;
from_tun2 -> ip_cl;

nb [1] -> to_wvlan; // routing Hello packets

// h :: SendGridHello(HELLO_PERIOD, HELLO_JITTER, GRID_MAC_ADDR, GRID_IP) -> to_wvlan;




