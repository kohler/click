// grid.click
// to be used with tools/run-grid-node

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);

li :: GridLocationInfo(POS_LAT, POS_LON);

// protocol els
nb :: GridRouteTable(NBR_TIMEOUT, LR_PERIOD, LR_JITTER, MAC_ADDR, GRID_IP, NUM_HOPS);
lr :: LookupLocalGridRoute(MAC_ADDR, GRID_IP, nb);
geo :: LookupGeographicGridRoute(MAC_ADDR, GRID_IP, nb);
fq :: FloodingLocQuerier(MAC_ADDR, GRID_IP);
loc_repl :: LocQueryResponder(MAC_ADDR, GRID_IP);

elementclass TTLChecker {
  // expects grid packets with MAC headers --- place on output path to
  // dec TTL for next hop and provide traceroute support.
  
  // push -> push

  // output [0] passes through the Grid MAC packets
  // output [1] produces ICMP error packets to be passed back to 
  // IP routing layer
  
  input 
    -> cl :: Classifier(15/GRID_NBR_ENCAP_PROTO !90/GRID_HEX_IP, -) [0] // don't dec ttl for packets we originate
    -> ckip :: MarkIPHeader(78) // 78 is offset of IP in GRID_NBR_ENCAP
    -> dec :: DecIPTTL
    -> output;

  cl [1] -> output;
  dec [1] -> ICMPError(GRID_IP, 11, 0) -> [1] output;
};


// device layer els
from_wvlan :: FromDevice(NET_DEVICE, 0);
to_wvlan :: TTLChecker [0] -> FixSrcLoc(li) -> SetGridChecksum -> ToDevice(NET_DEVICE);
connectiontunnel to_ip_cl/in -> to_ip_cl/out;
to_wvlan [1] -> to_ip_cl/in;

// linux ip layer els
linux :: KernelTap(GRID_IP/GRID_NETMASK, GRID_GW, HEADROOM) -> from_linux :: Strip(14);
to_linux :: EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> linux;

grid_demux :: Classifier(15/GRID_NBR_ENCAP_PROTO,  // encapsulated packets 
			 15/GRID_LOC_QUERY_PROTO,  // loc query packets
			 15/GRID_LOC_REPLY_PROTO,  // loc reply packets
			 15/GRID_LR_HELLO_PROTO);  // route advertisement packets



// hook it all up
from_wvlan -> Classifier(12/GRID_ETH_PROTO) 
  -> check_grid :: CheckGridHeader
  -> fr :: FilterByRange(RANGE, li) [0] 
  -> grid_demux [0] 
  -> [0] lr [0] 
  -> to_wvlan;

query_demux :: Classifier(62/GRID_HEX_IP, // loc query for us
			  -);

repl_demux :: Classifier(62/GRID_HEX_IP, // loc reply for us
			  -);

grid_demux [1] -> query_demux;
grid_demux [2] -> repl_demux;
grid_demux [3] -> nb;

repl_demux [0] -> [1] fq; // handle reply to our loc query
repl_demux [1] -> PrintGrid(FWD_REPL) -> [0] lr; // forward query reply packets like encap packets

loc_repl -> [0] lr; // forward loc reply packets initiated by us

query_demux [0] -> PrintGrid(qd0) -> loc_repl; // reply to this query
query_demux [1] -> PrintGrid(qd1) -> [1] fq [1] -> to_wvlan; // propagate this loc query, or initiate a new loc query



lr [2] -> [0] fq [0] -> [0] geo; // packets for geo fwding
lr [3] -> Discard; // bad packets

geo [0] -> to_wvlan;
geo [1] -> Discard; // geo route can't handle
geo [2] -> Discard; // bad packet

fr [1] -> Discard; // out of range

check_grid [1] -> Print(bad_grid_hdr) -> Discard;

from_linux -> cl :: Classifier(16/GRID_HEX_IP, // ip for us
			       16/GRID_NET_HEX, // ip for Grid network
			       -); // the rest of the world
to_ip_cl/out -> cl;
cl [0] -> to_linux;
cl [1] -> GetIPAddress(16) -> [1] lr [1] -> check :: CheckIPHeader [0] -> to_linux;
check [1] -> Discard;
cl [2] -> SetIPAddress(GRID_GW) -> [1] lr; // for grid gateway
nb [0] -> to_wvlan; // Routing hello packets

// SendGridHello(HELLO_PERIOD, HELLO_JITTER, MAC_ADDR, GRID_IP) -> to_wvlan;
