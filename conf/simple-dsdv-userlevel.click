// simple-dsdv-userlevel.click

// bare-bones userlevel DSDV configuration for a single interface

// to be used with tools/build-grid-config.sh

// Note: This configuration file will not work unless you supplied
// the "--enable-grid" option when configuring Click.


ControlSocket(tcp, CONTROL_PORT, READONLY CONTROL_RO);

elementclass ToGridDev {
  // push, no output

  // Send packets to the output device, fixing location and checksum
  // header fields.
  $dev |
    input 
    -> FixSrcLoc(li)
    -> SetGridChecksum
    -> ToDevice($dev);
};

elementclass FromGridDev {
  // push, no input

  // Get packets from the input device; ignore non-grid packets, or
  // packets that are corrupt; send a copy of any packet we process to
  // sniffers such as tcpdump.
  $dev, $mac |
  FromDevice($dev, PROMISC false) 
    -> Classifier(12/GRID_ETH_PROTO)
    -> HostEtherFilter($mac, DROP_OWN true)
    -> ck :: CheckGridHeader
    -> output;
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

host_tun :: KernelTun(GRID_IP/GRID_NETMASK, HEADROOM TUN_INPUT_HEADROOM) -> CheckIPHeader -> GetIPAddress(16) -> ip_demux;

ip_demux [0] -> IPPrint("ip_loopback") -> host_tun;       // loopback packet sent by us
ip_demux [1] -> GridEncap(GRID_MAC_ADDR, GRID_IP) -> lr;  // forward packet sent by us

grid_data_demux [0] -> Strip(OFFSET_ENCAP_IP) -> host_tun;  // receive packet from net for us  
grid_data_demux [1] -> lr;                                  // forward packet from net for someone else

lr -> dev0;









