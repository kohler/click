// grid.click
// to be used with tools/run-grid

elementclass TTLChecker {
  // expects grid packets with MAC headers --- place on output path to
  // dec TTL for next hop and provide traceroute support.
  // push -> push
  // output [0] passes through the Grid MAC packets
  // output [1] produces ICMP error packets to be passed back to IP routing layer
  
  input 
    -> cl :: Classifier(15/GRID_NBR_ENCAP_PROTO !90/GRID_HEX_IP, 
			-);

  cl [0] -> output; // don't dec ttl for packets we originate
  
  cl [1] -> MarkIPHeader(78) // 78 is offset of IP in GRID_NBR_ENCAP
         -> dec :: DecIPTTL;
  dec [0] -> output;
  dec [1] -> ICMPError(GRID_IP, 11, 0) -> [1] output;
};

#ifdef CPP_IS_GATEWAY

	ggi :: GridGatewayInfo (true);

#else

	ggi :: GridGatewayInfo (false);

#endif CPP_IS_GATEWAY

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);
ChatterSocket(tcp, 7776);

li :: GridLocationInfo(POS_LAT, POS_LON);

fq :: FloodingLocQuerier(GRID_MAC_ADDR, GRID_IP);
loc_repl :: LocQueryResponder(GRID_MAC_ADDR, GRID_IP);

nb :: GridRouteTable(NBR_TIMEOUT, 
		     LR_PERIOD, LR_JITTER, 
		     GRID_MAC_ADDR, GRID_IP, 
		     ggi, 
		     NUM_HOPS);
lr :: LookupLocalGridRoute(GRID_MAC_ADDR, GRID_IP, nb);
geo :: LookupGeographicGridRoute(GRID_MAC_ADDR, GRID_IP, nb);

loc_repl -> [0] lr; // forward loc reply packets initiated by us

// device layer els
from_grid_if :: FromDevice(WI_NET_DEVICE, 0);
to_grid_if :: TTLChecker [0] 
  -> FixSrcLoc(li) 
  -> SetGridChecksum 
  -> ToDevice(WI_NET_DEVICE);

connectiontunnel to_ip_cl/in -> to_ip_cl/out;
to_grid_if [1] -> to_ip_cl/in;

// linux ip layer els
tun0 :: KernelTap(GRID_IP/GRID_NETMASK, 18.26.7.254, HEADROOM);

tun0 -> from_tun0 :: Strip(14);
to_tun0 :: EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> tun0;

grid_demux :: Classifier(15/GRID_NBR_ENCAP_PROTO,  // encapsulated packets 
			 15/GRID_LOC_QUERY_PROTO,  // loc query packets
			 15/GRID_LOC_REPLY_PROTO,  // loc reply packets
			 15/GRID_LR_HELLO_PROTO);  // route advertisement packets

grid_demux [0] -> [0] lr;

grid_demux [1] -> query_demux :: Classifier(62/GRID_HEX_IP, // loc query for us
					    -);
grid_demux [2] -> repl_demux :: Classifier(62/GRID_HEX_IP, // loc reply for us
					   -);
grid_demux [3] -> nb;

query_demux [0] 
  -> PrintGrid(qd0) 
  -> loc_repl; // reply to this query
query_demux [1]
//  -> PrintGrid(qd1) 
  -> [1] fq [1] 
  -> to_grid_if; // propagate this loc query, or initiate a new loc query

repl_demux [0] 
  -> [1] fq; // handle reply to our loc query
repl_demux [1]
//  -> PrintGrid(FWD_REPL)
  -> [0] lr; // forward query reply packets like encap packets

// hook it all up
from_grid_if 
  -> Classifier(12/GRID_ETH_PROTO)
  -> HostEtherFilter(GRID_MAC_ADDR, 1)
  -> check_grid :: CheckGridHeader
  -> fr :: FilterByRange(RANGE, li) [0] 
  -> grid_demux;

lr [0] -> to_grid_if;
lr [2] -> [0] fq [0] -> [0] geo; // packets for geo fwding
lr [3] -> Discard; // bad packets

geo [0] -> to_grid_if;
geo [1] -> Discard; // geo route can't handle
geo [2] -> Discard; // bad packet

fr [1] -> Discard; // out of range

check_grid [1] -> Print(bad_grid_hdr) -> Discard;

cl :: Classifier(16/GRID_HEX_IP, // ip for us
		 16/GRID_NET_HEX, // ip for Grid network
		 -); // the rest of the world

iprw :: IPRewriter (pattern GRID_IP - - - 0 1,
		    nochange 2);

iprw [0] -> to_tun0; // outgoing rewritten packets
iprw [1] -> cl; // replies
// iprw [2] -> to_tun0; // unchanged incoming packets
iprw [2] -> Print(NO_MAPPING) -> cl;

cl [0] -> to_tun0;
cl [1]
  -> GetIPAddress(16) 
  -> [1] lr [1] // IP packets getting passed up to kernel
  -> check :: CheckIPHeader [0]
  -> nat_ipcl :: IPClassifier (src net 10.2/24,
			       -);

nat_ipcl [0] -> [0] iprw; // packets we want rewritten
nat_ipcl [1] -> to_tun0; // skip rewrite

from_tun0 -> [1] iprw;

cl [2] 
  -> SetIPAddress(18.26.7.254) // special "any gateway" address
  -> [1] lr; // for grid gateway

check [1] -> Discard;

nb [0] -> to_grid_if; // Routing hello packets

to_ip_cl/out -> cl;
