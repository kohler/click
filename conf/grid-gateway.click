// grid-gateway.click
// to be used with tools/run-grid-gateway

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);

li :: GridLocationInfo(POS_LAT, POS_LON);

fq :: FloodingLocQuerier(GRID_MAC_ADDR, GRID_IP);
loc_repl :: LocQueryResponder(GRID_MAC_ADDR, GRID_IP);

elementclass GWTTLChecker {
  // expects grid packets with MAC headers --- place on output path to
  // dec TTL for next hop and provide traceroute support.
  
  // push -> push

  // output [0] passes through the Grid MAC packets
  // output [1] produces ICMP error packets to be passed back to 
  // IP routing layer
  
  input 
    -> cl :: Classifier(15/GRID_NBR_ENCAP_PROTO !90/GRID_HEX_IP !90/GW_HEX_IP, -) [0] // don't dec ttl on packets we originate
    -> ckip :: MarkIPHeader(78) // 78 is offset of IP in GRID_NBR_ENCAP
    -> dec :: DecIPTTL
    -> output;

  cl [1] -> output;
  dec [1] -> ICMPError(GRID_IP, 11, 0) -> [1] output;
};


// device interface
eth :: FromDevice(GW_NET_DEVICE, 0);
to_eth :: ToDevice(GW_NET_DEVICE);


wvlan :: FromDevice(WI_NET_DEVICE, 0);
to_wvlan :: GWTTLChecker [0] -> FixSrcLoc(li) -> SetGridChecksum -> ToDevice(WI_NET_DEVICE);
connectiontunnel to_ip_cl/in -> to_ip_cl/out;
to_wvlan [1] -> to_ip_cl/in;


// IP interfaces on gateway machine
tun1 :: KernelTap(GW_IP/GW_NETMASK, GW_REAL_IP, HEADROOM); // gateway's regular address
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
to_ip_cl/out -> ip_cl;


nb :: UpdateGridRoutes(NBR_TIMEOUT, LR_PERIOD, LR_JITTER, GRID_MAC_ADDR, GRID_IP, NUM_HOPS);
lr :: LookupLocalGridRoute(GRID_MAC_ADDR, GRID_IP, nb);
geo :: LookupGeographicGridRoute(GRID_MAC_ADDR, GRID_IP, nb);

grid_demux :: Classifier(15/GRID_NBR_ENCAP_PROTO,  // encapsulated packets 
			 15/GRID_LOC_QUERY_PROTO,  // loc query packets
			 15/GRID_LOC_REPLY_PROTO); // loc reply packets


lr [0] -> to_wvlan;
lr [1] -> ip_cl; // decrement TTL on grid packets that we forward to wired net

lr [2] -> fq -> [0] geo; // for geographic forwarding
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
-> HostEtherFilter(GRID_MAC_ADDR, true)
-> check_grid :: CheckGridHeader [0]
-> fr :: FilterByRange(RANGE, li) [0]
-> [0] nb [0]
-> grid_demux [0]
-> [0] lr;


check_grid [1]-> Print(BAD_GRID_HDR) -> Discard;
 fr [1] -> Discard; // out of range
wvlan_demux [1] -> Discard; // not a grid packet

query_demux :: Classifier(62/GRID_HEX_IP, // loc query for us
			  -);
reply_demux :: Classifier(62/GRID_HEX_IP, // loc reply for us
			  -);

grid_demux [1] -> PrintGrid(QUERY) -> query_demux;
grid_demux [2] -> PrintGrid(REPLY) -> reply_demux; 

reply_demux [0] -> PrintGrid(REPLY_FOR_US) -> [1] fq; // handle reply to our loc query
reply_demux [1] -> [0] lr; // forward query reply packets like encap packets

loc_repl 
-> PrintGrid(REPLY_FROM_US) 
-> [0] lr; // forward loc reply packets initiated by us

query_demux [0] 
//-> PrintGrid(qd0) 
-> loc_repl; // reply to this query
query_demux [1] 
//-> PrintGrid(qd1) 
-> [1] fq [1] 
//-> PrintGrid(fq1) 
-> to_wvlan; // propagate this loc query, or initiate a new loc query


ip_cl [0] -> to_tun1;
ip_cl [1] -> to_tun2;
ip_cl [2] -> to_nb_ip; // send grid net packets to Grid processing
ip_cl [3] -> MarkIPHeader -> dec :: DecIPTTL -> gw_cl :: Classifier(16/GW_HEX_NET, -); // get local wired IP net traffic; decrement TTL
dec [1] -> ICMPError(GRID_IP, 11, 0) -> ip_cl;

gw_cl [0] -> GetIPAddress(16) -> [0] arpq; // ARP and send local net traffic
gw_cl [1] -> SetIPAddress(GW_REAL_IP) -> [0] arpq; // ARP and send gateway traffic

from_tun1 -> ip_cl;
from_tun2 -> ip_cl;

nb [1] -> to_wvlan; // routing Hello packets

// h :: SendGridHello(HELLO_PERIOD, HELLO_JITTER, GRID_MAC_ADDR, GRID_IP) -> to_wvlan;




