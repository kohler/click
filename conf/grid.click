// grid.click
// to be used with tools/run-grid

elementclass TTLChecker {
  // expects grid packets with MAC headers --- place on output path to
  // dec TTL for next hop and provide traceroute support.  push -> push
  // output [0] passes through the Grid MAC packets
  // output [1] produces ICMP error packets to be passed back to IP routing layer
  
  input 
    -> cl :: Classifier(15/GRID_NBR_ENCAP_PROTO !90/GRID_HEX_IP, 
			-);

  cl [0] -> MarkIPHeader(78) // 78 is offset of IP in GRID_NBR_ENCAP
         -> dec :: DecIPTTL;
  cl [1] -> output; // don't dec ttl for packets we originate

  dec [0] -> output;
  dec [1] -> ICMPError(GRID_IP, 11, 0) -> [1] output;
};

#if CPP_IS_GATEWAY
   ggi :: GridGatewayInfo (true);
#else
   ggi :: GridGatewayInfo (false);
#endif // CPP_IS_GATEWAY

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);
ChatterSocket(tcp, 7776);
ChatterSocket(tcp, 7766, CHANNEL routelog);

li :: GridLocationInfo(POS_LAT, POS_LON);

fq :: FloodingLocQuerier(GRID_MAC_ADDR, GRID_IP);
loc_repl :: LocQueryResponder(GRID_MAC_ADDR, GRID_IP);

nb :: GridRouteTable(NBR_TIMEOUT, 
		     LR_PERIOD, LR_JITTER, 
		     GRID_MAC_ADDR, GRID_IP, 
		     ggi,
		     NUM_HOPS);
lr :: LookupLocalGridRoute(GRID_MAC_ADDR, GRID_IP, nb, ggi);
geo :: LookupGeographicGridRoute(GRID_MAC_ADDR, GRID_IP, nb);

loc_repl -> [0] lr; // forward loc reply packets initiated by us

// device layer els

from_grid_if :: FromDevice(WI_NET_DEVICE, 0);
to_grid_if :: TTLChecker;

to_grid_if [0] -> FixSrcLoc(li)
               -> SetGridChecksum
	       -> ToDevice(WI_NET_DEVICE);

// linux ip layer els
tun0 :: KernelTap(GRID_IP/GRID_NETMASK, 1.2.3.4, HEADROOM)
        -> from_tun0 :: Strip(14);
to_tun0 :: EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) 
        -> tun0;

from_grid_if 
  -> Classifier(12/GRID_ETH_PROTO)
  //  -> Print("grid")
  -> HostEtherFilter(GRID_MAC_ADDR, 1)
  -> check_grid :: CheckGridHeader
  -> fr :: FilterByRange(RANGE, li) [0] 
  -> grid_demux :: Classifier(19/GRID_NBR_ENCAP_PROTO,  // encapsulated (data) packets
			      19/GRID_LOC_QUERY_PROTO,  // loc query packets
			      19/GRID_LOC_REPLY_PROTO,  // loc reply packets
			      19/GRID_LR_HELLO_PROTO);  // route advertisement packets

grid_demux [0]
//	       -> Print("gd0 ")
	       -> [0] lr;
grid_demux [1]
//	       -> Print("gd1 ")
               -> query_demux :: Classifier(62/GRID_HEX_IP, 
					    62/GRID_HEX_ANY_GATEWAY_IP,
					    -); // loc query for us
grid_demux [2]
//	       -> Print("gd2 ")
               -> repl_demux :: Classifier(62/GRID_HEX_IP, 
					   62/GRID_HEX_ANY_GATEWAY_IP,
					   -); // loc reply for us
grid_demux [3]
//	       -> Print("gd3 ")
               -> nb;

query_demux [0] 
//	       -> Print("qd0 ")
               -> loc_repl; // reply to this query

#if CPP_IS_GATEWAY==1
  query_demux [1] 
//	       -> Print("qd1 ")
               -> loc_repl;
#else
  query_demux [1] 
//	       -> Print("qd1 ")
               -> [1] fq;
#endif

query_demux [2] 
//	       -> Print("qd2 ")
               -> [1] fq;

repl_demux [0]
//	       -> Print("rd0 ")
               -> [1] fq; // handle reply to our loc query

#if CPP_IS_GATEWAY==1
  repl_demux [1] 
//	       -> Print("rd1 ")
               -> [1] fq;
#else
  repl_demux [1] 
//	       -> Print("rd1 ")
               -> [0] lr;
#endif

repl_demux [2] 
//	       -> Print("qd2 ")
               -> [0] lr; // forward query reply packets like encap packets

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
//       -> Print ("lr1 ")
       -> check :: CheckIPHeader; // IP packets getting passed up to kernel
lr [2]
//       -> Print ("lr2 ") // packets for GF
       -> [0] fq;
lr [3]
//       -> Print ("lr3 ")
       -> Discard; // bad packets

geo [0] -> to_grid_if;
geo [1] -> Discard; // geo route can't handle
geo [2] -> Discard; // bad packet

fr [1] -> Discard; // out of range

check_grid [1] -> Discard;

cl :: Classifier(16/GRID_HEX_IP, // ip for us
		 16/GRID_NET1_HEX,  // ip for Grid network
		 16/GRID_NET2_HEX, // ...or the rest of the Grid network
		 -); // the rest of the world

cl [0] 
//     -> Print("cl0 ")
       -> to_tun0;

get_ip :: GetIPAddress(16) -> [1] lr;

cl [1] 
//     -> Print ("cl1 ")
       -> get_ip;
		  
cl [2]
//     -> Print ("cl2 ")
       -> get_ip;

#if CPP_IS_GATEWAY==0
     check [0] -> to_tun0;
     from_tun0 -> cl;
#else
     iprw :: IPRewriter (pattern GW_IP - - - 0 1,
			 nochange 2);
     nat_ipcl :: IPClassifier (src net 10.2.0.0/16,
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
              -> wired_cl :: Classifier(12/0800,
					12/0806 20/0001);

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

#endif

cl [3] 
//  -> Print ("cl3 ") 
  -> SetIPAddress(GRID_ANY_GATEWAY_IP) // special "any gateway" address
  -> [1] lr; // for grid gateway

check [1] -> Discard;

nb [0] -> to_grid_if; // Routing hello packets

to_grid_if [1] -> cl; // ICMP TTL expired messages
