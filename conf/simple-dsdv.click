// simple-dsdv.click

// bare-bones kernel-compatible DSDV configuration for a single interface

// to be used with tools/build-grid-config.sh

// Note: This configuration file will not work unless you supplied
// the "--enable-grid" option when configuring Click.



elementclass ToGridDev {
  // push, no output

  // Send packets to the output device, fixing location and checksum
  // header fields; maintain separate queues for protocol and data
  // packets; send a copy of the packet to sniffers such as tcpdump.
  $dev |
  input -> cl :: Classifier(OFFSET_GRID_PROTO/GRID_PROTO_LR_HELLO,
			    OFFSET_GRID_PROTO/GRID_PROTO_NBR_ENCAP);
  prio :: PrioSched;
  cl [0] -> route_counter :: Counter -> route_q :: Queue(5) -> [0] prio;
  cl [1] ->  data_counter :: Counter ->  data_q :: Queue(5) -> [1] prio;
  prio
    -> FixSrcLoc(li)
    -> SetGridChecksum
    -> t :: PullTee 
    -> ToDevice($dev);
  t [1] -> SetTimestamp -> ToHostSniffers($dev);
};

elementclass FromGridDev {
  // push, no input

  // Get packets from the input device; ignore non-grid packets, or
  // packets that are corrupt; send a copy of any packet we process to
  // sniffers such as tcpdump.
  $dev, $mac |
  FromDevice($dev, PROMISC false) 
    -> t :: Tee 
    -> Classifier(12/GRID_ETH_PROTO)
    -> HostEtherFilter($mac, DROP_OWN true)
    -> ck :: CheckGridHeader
    -> output;
  t [1] -> ToHostSniffers($dev);
  ck [1] -> Print("Bad Grid header received", TIMESTAMP true) -> Discard;
};

li :: GridLocationInfo2(0, 0, LOC_GOOD false);

nb :: DSDVRouteTable(ROUTE_TIMEOUT,
		     BROADCAST_PERIOD, BROADCAST_JITTER, BROADCAST_MIN_PERIOD,
		     GRID_MAC_ADDR, GRID_IP, 
		     MAX_HOPS NUM_HOPS,
                     METRIC hopcount);

lr :: LookupLocalGridRoute2(GRID_MAC_ADDR, GRID_IP, nb);

grid_demux :: Classifier(OFFSET_GRID_PROTO/GRID_PROTO_NBR_ENCAP,    // encapsulated (data) packets
			 OFFSET_GRID_PROTO/GRID_PROTO_LR_HELLO)     // route advertisement packets

arp_demux :: Classifier(12/0806 20/0001, // arp queries
			12/0800);        // IP packets

// handles IP packets with no extra encapsulation
ip_demux :: IPClassifier(dst host GRID_IP,                      // ip for us
			 dst net GRID_NET1/GRID_NET1_NETMASK);  // ip for Grid network

// handles IP packets with Grid data encapsulation
grid_data_demux :: IPClassifier(dst host GRID_IP,                      // ip for us
				dst net GRID_NET1/GRID_NET1_NETMASK);  // ip for Grid network

// dev0
FromGridDev(REAL_NET_DEVICE, GRID_MAC_ADDR) -> Paint(0) -> grid_demux
dev0 :: ToGridDev(REAL_NET_DEVICE);


grid_demux [0] -> Align(4, 2) -> CheckIPHeader(OFFSET_ENCAP_IP) -> grid_data_demux;
grid_demux [1] -> nb -> dev0;

to_host :: ToHost(grid0);
to_host_encap :: EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> to_host; 
from_host :: FromHost(grid0, GRID_IP/GRID_NETMASK) -> arp_demux -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> to_host;
arp_demux [1] -> Strip(14) -> CheckIPHeader -> GetIPAddress(16) -> ip_demux;

ip_demux [0] -> IPPrint("ip_loopback") -> to_host_encap;  // loopback packet sent by us
ip_demux [1] -> GridEncap(GRID_MAC_ADDR, GRID_IP) -> lr;  // forward packet sent by us

grid_data_demux [0] -> Strip(OFFSET_ENCAP_IP) -> to_host_encap;  // receive packet from net for us  
grid_data_demux [1] -> lr;                                       // forward packet from net for someone else

lr -> dev0;









