// grid.click 
//
// NOTE: (12 August 2003) This configuration is very out of date, and
// is kept here only for historical purposes, and to show the details
// of how some of the Grid elements can be put together.
//
// More recent information about running Grid can be found at
// http://pdos.lcs.mit.edu/grid/software.html.
//
// This configuration was designed to me pre-processed by the m4 macro
// processor, which would define the neccessary parameters for each of
// the elements.

elementclass TTLChecker {
  /* expects grid packets with MAC headers --- place on output path to
   * decrement the IP TTL for next hop and provide traceroute support.  
   *
   * push -> push 
   *
   * output [0] passes through the Grid MAC packets 
   *
   * output [1] produces ICMP error packets to be passed back to IP
   * routing layer 
   */
  
  input -> cl :: Classifier(OFFSET_GRID_PROTO/GRID_PROTO_NBR_ENCAP, -);
  cl [1] -> output; // don't try to dec ttl for non-IP packets...

  cl [0] 
    -> MarkIPHeader(OFFSET_ENCAP_IP) 
    -> cl2 :: IPClassifier(src host != GRID_IP, -);

  cl2 [0]-> dec :: DecIPTTL; // only decrement ttl for packets we don't originate
  cl2 [1] -> output; 

  dec [0] -> output;
  dec [1] -> ICMPError(GRID_IP, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS) -> [1] output;
};


elementclass PrintGrid2 {
  $tag |  // print all packets except GRID_LR_HELLO messages
  input -> cl :: Classifier(OFFSET_GRID_PROTO/GRID_PROTO_LR_HELLO, -);
  cl [0] -> output;
  cl [1] -> PrintGrid($tag) -> output;
};


ifdef(`IS_GATEWAY',
   ggi :: GridGatewayInfo(true);
, dnl else
   ggi :: GridGatewayInfo(false);
)

ghi :: GridHeaderInfo;
gl :: GridLogger(SHORT_IP true);

ControlSocket(tcp, CONTROL_PORT, READONLY CONTROL_RO);
ControlSocket(tcp, CONTROL2_PORT, READONLY CONTROL2_RO);

// general router chatter output
ChatterSocket(tcp, CHATTER_PORT);

// routelog output
ChatterSocket(tcp, ROUTELOG_PORT, CHANNEL ROUTELOG_CHANNEL);

// route probe reply messages
ChatterSocket(tcp, PROBE_PORT, CHANNEL PROBE_CHANNEL);

li :: GridLocationInfo(POS_LAT, POS_LON, LOC_GOOD ARG_LOC_GOOD, ERR_RADIUS ARG_LOC_ERR, TAG ARG_LOC_TAG);

fq :: FloodingLocQuerier(GRID_MAC_ADDR, GRID_IP);
loc_repl :: LocQueryResponder(GRID_MAC_ADDR, GRID_IP);

rps :: GridProbeSender(GRID_MAC_ADDR, GRID_IP);
rph :: GridProbeHandler(GRID_MAC_ADDR, GRID_IP, lr, geo, fq);
rpr :: GridProbeReplyReceiver(PROBE_CHANNEL);

ai :: AiroInfo(GRID_NET_DEVICE);
lt :: LinkTracker(LINK_TRACKER_TAU);

nb :: DSDVRouteTable(ROUTE_TIMEOUT,
		     BROADCAST_PERIOD, BROADCAST_JITTER, BROADCAST_MIN_PERIOD,
		     GRID_MAC_ADDR, GRID_IP, 
		     GW ggi, 
                     LT lt,
                     LS ls,
		     MAX_HOPS NUM_HOPS,
                     METRIC est_tx_count,
                     LOG gl);

lr :: LookupLocalGridRoute(GRID_MAC_ADDR, GRID_IP, nb, ggi, lt, LOG gl);
geo :: LookupGeographicGridRoute(GRID_MAC_ADDR, GRID_IP, nb, li);

grid_demux :: Classifier(OFFSET_GRID_PROTO/GRID_PROTO_NBR_ENCAP,    // encapsulated (data) packets
			 OFFSET_GRID_PROTO/GRID_PROTO_LOC_QUERY,    // loc query packets
			 OFFSET_GRID_PROTO/GRID_PROTO_LOC_REPLY,    // loc reply packets
			 OFFSET_GRID_PROTO/GRID_PROTO_LR_HELLO,     // route advertisement packets
		         OFFSET_GRID_PROTO/GRID_PROTO_ROUTE_PROBE,  // route probe packets
			 OFFSET_GRID_PROTO/GRID_PROTO_ROUTE_REPLY); // probe reply packets




loc_repl -> [0] lr; // forward loc reply packets initiated by us

rps [0] -> PrintGrid("rps0 ") -> grid_demux; // insert originated route probes into grid demux so we can reply to our own probe
rph [0] -> PrintGrid("rph0 ") -> [0] lr;     // forward probes that need to continue
rph [1] -> PrintGrid("rph1 ") -> grid_demux; // insert probe replies into grid demux so we can get our own reply


// device layer els

from_grid_if :: FromDevice(GRID_NET_DEVICE, 0);
to_grid_if :: TTLChecker;

to_grid_if [0] -> FixSrcLoc(li)
               -> PingPong(ls)
               -> SetGridChecksum
	       -> ToDevice(GRID_NET_DEVICE, SET_ERROR_ANNO true) 
               -> GridTxError(LOG gl);

// linux ip layer els
tun0 :: KernelTap(GRID_IP/GRID_NETMASK, 1.2.3.4, TUN_INPUT_HEADROOM)
        -> Strip(14) -> from_tun0 :: MarkIPHeader(0);
to_tun0 :: EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) 
        -> tun0;

from_grid_if 
  -> Classifier(12/GRID_ETH_PROTO)
  -> HostEtherFilter(GRID_MAC_ADDR, 1)
  -> check_grid :: CheckGridHeader
//  -> PrintGrid("XXX")
  -> fr :: FilterByRange(MAX_RANGE_FILTER, li) [0]
//  -> PrintGrid("post_fr")
  -> ls :: LinkStat(ai, LINK_STAT_WINDOW)
//  -> PrintGrid("post_ls")
  -> lt
//  -> PrintGrid("post_lt")
//  -> Print("post_lt", 80)
  -> grid_demux;

grid_demux [0]
//	       -> PrintGrid("gd0 ")
//               -> Print("gd0", 80)
               -> Align(4, 2)
//               -> Print("after_gd0_align")
	       -> [0] lr;

grid_demux [1]
//	       -> Print("gd1 ")
ifdef(`DISABLE_GF',
               -> Discard;
               Idle
)
               -> query_demux :: Classifier(OFFSET_LOC_QUERY_DST/GRID_HEX_IP, 
					    OFFSET_LOC_QUERY_DST/GRID_ANY_GATEWAY_HEX_IP,
					    -);
grid_demux [2]
//	       -> Print("gd2 ")
ifdef(`DISABLE_GF',
               -> Discard;
               Idle
)
               -> repl_demux :: Classifier(OFFSET_LOC_REPLY_DST/GRID_HEX_IP, 
					   OFFSET_LOC_REPLY_DST/GRID_ANY_GATEWAY_HEX_IP,
					   -); 

grid_demux [3]
//	       -> Print("gd3 ")
               -> nb;

grid_demux [4]
//             -> Print("gd4 ") 
               -> rph;

grid_demux [5]
//             -> Print("gd5 ") 
               -> probe_repl_demux :: Classifier(OFFSET_ROUTE_PROBE_DST/GRID_HEX_IP, 
                                                 OFFSET_ROUTE_PROBE_DST/GRID_ANY_GATEWAY_HEX_IP,
                                                 -); 

query_demux [0] 
//	       -> Print("qd0 ")
               -> loc_repl; // reply to this query

ifdef(`IS_GATEWAY',
  query_demux [1] 
//	       -> Print("qd1 ")
               -> loc_repl;
, dnl else
  query_demux [1] 
//	       -> Print("qd1 ")
               -> [1] fq;
)

query_demux [2] 
//	       -> Print("qd2 ")
               -> [1] fq;

repl_demux [0]
//	       -> Print("rd0 ")
               -> [1] fq; // handle reply to our loc query

ifdef(`IS_GATEWAY',
  repl_demux [1] 
//	       -> Print("rd1 ")
               -> [1] fq;
, dnl else
  repl_demux [1] 
//	       -> Print("rd1 ")
               -> [0] lr;
)

repl_demux [2] 
//	       -> Print("qd2 ")
               -> [0] lr; // forward query reply packets like encap packets

probe_repl_demux [0]
               -> PrintGrid("prd0 ")
               -> rpr; // handle probe replies for us

ifdef(`IS_GATEWAY',
probe_repl_demux [1]
               -> PrintGrid("prd1 ")
               -> rpr; // handle probe replies for us
, dnl else
probe_repl_demux [1]
               -> PrintGrid("prd1 ")
               -> [0] lr; // fwd replies for someone else
)

probe_repl_demux [2]
               -> PrintGrid("prd2 ")
               -> [0] lr; // fwd replies for someone else


fq [0] 
//	       -> Print("fq0 ")
               -> [0] geo; // packets for geo fwding
fq [1] 
//	       -> Print("fq1 ")
               -> to_grid_if; // propagate loc query, or initiate a new loc query

lr [0]
//       -> Print ("lr0 ")
       -> to_grid_if; // grid packets being local routed
lr [1]
//       -> IPPrint ("lr1_out")
       -> check :: CheckIPHeader; // IP packets getting passed up to kernel
lr [2]
//       -> Print ("lr2 ") // packets for GF
ifdef(`DISABLE_GF',
       -> Discard;
       Idle 
)
       -> [0] fq;


lr [3]
//       -> Print ("lr3 ")
       -> Discard; // bad packets

geo [0] -> to_grid_if;

geo [1] -> Discard; // geo route can't handle

fr [1] -> Discard; // out of range

check_grid [1] -> Discard;

cl :: IPClassifier(dst host GRID_IP,                      // ip for us
		   dst net GRID_NET1/GRID_NET1_NETMASK,   // ip for Grid network
		   dst net GRID_NET2/GRID_NET2_NETMASK,   // ...or the rest of the Grid network
		   -); // the rest of the world

cl [0] 
//     -> Print("cl0 ")
       -> to_tun0;

get_ip :: GetIPAddress(16) 
  // -> IPPrint("lr1_in") 
-> [1] lr;

cl [1] 
//     -> Print ("cl1 ")
       -> get_ip;
		  
cl [2]
//     -> Print ("cl2 ")
       -> get_ip;

ifdef(`IS_GATEWAY',
     iprw :: IPRewriter (pattern GW_IP - - - 0 1,
			 pass 2);
     nat_ipcl :: IPClassifier (src net GRID_NET2/GRID_NET2_NETMASK,
			       -);
     check [0] -> nat_ipcl;

     nat_ipcl [0] 
//                -> Print ("nc0 ")
//                -> IPPrint("rw in0")
                  -> [0] iprw; // packets we want rewritten
     nat_ipcl [1] 
//                -> Print ("nc1 ") 
                  -> to_tun0;  // skip rewrite on non-10.2* packets

     from_tun0    -> nat_ch :: CheckIPHeader;

     nat_ch [0] 
//              -> IPPrint ("rw in1") 
                -> [1] iprw; // reverse map on packets from the kernel

     nat_ch [1] -> Discard;

     iprw [0] 
//            -> IPPrint ("rw rew")
              -> to_tun0; // outgoing rewritten packets
     iprw [1] 
//            -> IPPrint ("rw rep")
              -> cl; // (reverse) rewritten replies 
     iprw [2]
//            -> IPPrint ("rw nom")
              -> cl; // unchanged incoming packets

     from_wired_if :: FromDevice (GW_NET_DEVICE, 0)
       -> HostEtherFilter (GW_MAC_ADDR, 1)
       -> wired_cl :: Classifier(12/0800,          // IP packets
			         12/0806 20/0001); // ARP broadcasts

     wired_cl [0] -> Strip(14)
                  -> CheckIPHeader
                  -> ICMPPingResponder
                  -> to_wired_dev :: ToDevice(GW_NET_DEVICE);

     wired_cl [1] // arp requests
              -> ARPResponder(GW_IP GW_MAC_ADDR)
              -> Print (arpresp)
              -> to_wired_dev

     Idle
              -> wired_tun :: KernelTap(BOGUS_IP/BOGUS_NETMASK)
              -> Strip (14)
              -> CheckIPHeader
//            -> IPPrint ("1 rw")
              -> [1] iprw; // packets for 18.26.7/24 should show up on tun0's doorstep anyway
, dnl else
     check [0] -> to_tun0;
     from_tun0 -> cl;
)

cl [3] 
//  -> Print ("cl3 ") 
  -> SetIPAddress(GRID_ANY_GATEWAY_IP) // special "any gateway" address
  -> [1] lr; // for grid gateway

check [1] -> Discard;

nb [0] -> to_grid_if; // Routing hello packets

to_grid_if [1] -> cl; // ICMP TTL expired messages

// End of file
