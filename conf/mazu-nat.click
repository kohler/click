// mazu-nat.click

// This configuration is the lion's share of a firewalling NAT gateway. A
// version of this configuration was in daily use at Mazu Networks, Inc.
//
// Mazu was hooked up to the Internet via a single Ethernet connection (a
// cable modem). This configuration ran on a gateway machine hooked up
// to that cable modem via one Ethernet card. A second Ethernet card was
// hooked up to our internal network. Machines inside the internal network
// were given internal IP addresses in net 10.
//
// Here is a network diagram. Names in starred boxes must have addresses
// specified in AddressInfo. (No bare IP addresses occur in this
// configuration; everything has been specified through AddressInfo.)
//
//     +---------+
//    /           \                                              +-------
//   |             |       +-----------+           +-------+    /        
//   |  internal   |   ********     ********   **********  |   |         
//   |  network    |===*intern*     *extern*===*extern_ *  |===| outside 
//   |             |===*      *     *      *===*next_hop*  |===|  world  
//   |  *********  |   ********     ********   **********  |   |         
//   |  *intern_*  |       |  GATEWAY  |           | MODEM |    \        
//   |  *server *  |       +-----------+           +-------+     +-------
//    \ ********* /
//     +---------+
//
// The gateway supported the following functions:
//
// - Forwards arbitrary connections from the internal network to the outside
//   world.
// - Allows arbitrary FTP connections from the internal network to the outside
//   world. This requires application-level packet rewriting to support FTP's
//   PASV command. See FTPPortMapper, below.
// - New HTTP, HTTPS, and SSH connections from the outside world are allowed,
//   but they are forwarded to the internal machine `intern_server'.
// - All other packets from the outside world are sent to the gateway's Linux
//   stack, where they are handled appropriately.
//
// The heart of this configuration is the IPRewriter element and associated
// TCPRewriter and IPRewriterPatterns elements. You should probably look at
// the documentation for IPRewriter before trying to understand the
// configuration in depth.
//
// Note that the configuration will only forward TCP and UDP through the
// firewall. ICMP is not passed. A nice exercise: Add ICMP support to the
// configuration using the ICMPRewriter and ICMPPingRewriter elements.
//
// See also thomer-nat.click


// ADDRESS INFORMATION

AddressInfo(
  intern 	10.0.0.1	10.0.0.0/8	00:50:ba:85:84:a9,
  extern	209.6.198.213	209.6.198.0/24	00:e0:98:09:ab:af,
  extern_next_hop				02:00:0a:11:22:1f,
  intern_server	10.0.0.10
);


// DEVICE SETUP

elementclass GatewayDevice {
  $device |
  from :: FromDevice($device)
	-> output;
  input -> q :: Queue(1024)
	-> to :: ToDevice($device);
  ScheduleInfo(from .1, to 1);
}

// The following version of GatewayDevice sends a copy of every packet to
// ToHostSniffers, so that you can use tcpdump(1) to debug the gateway.

elementclass SniffGatewayDevice {
  $device |
  from :: FromDevice($device)
	-> t1 :: Tee
	-> output;
  input -> q :: Queue(1024)
	-> t2 :: PullTee
	-> to :: ToDevice($device);
  t1[1] -> ToHostSniffers;
  t2[1] -> ToHostSniffers($device);
  ScheduleInfo(from .1, to 1);
}

extern_dev :: SniffGatewayDevice(extern:eth);
intern_dev :: SniffGatewayDevice(intern:eth);

ip_to_host :: EtherEncap(0x0800, 1:1:1:1:1:1, intern)
	-> ToHost;


// ARP MACHINERY

extern_arp_class, intern_arp_class
	:: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
intern_arpq :: ARPQuerier(intern);

extern_dev -> extern_arp_class;
extern_arp_class[0] -> ARPResponder(extern)	// ARP queries
	-> extern_dev;
extern_arp_class[1] -> ToHost;			// ARP responses
extern_arp_class[3] -> Discard;

intern_dev -> intern_arp_class;
intern_arp_class[0] -> ARPResponder(intern)	// ARP queries
	-> intern_dev;
intern_arp_class[1] -> intern_arpr_t :: Tee;
	intern_arpr_t[0] -> ToHost;
	intern_arpr_t[1] -> [1]intern_arpq;
intern_arp_class[3] -> Discard;


// REWRITERS

IPRewriterPatterns(to_world_pat extern 50000-65535 - -,
		to_server_pat intern 50000-65535 intern_server -);

rw :: IPRewriter(// internal traffic to outside world
		 pattern to_world_pat 0 1,
		 // external traffic redirected to 'intern_server'
		 pattern to_server_pat 1 0,
		 // internal traffic redirected to 'intern_server'
		 pattern to_server_pat 1 1,
		 // virtual wire to output 0 if no mapping
		 pass 0,
		 // virtual wire to output 2 if no mapping
		 pass 2);

tcp_rw :: TCPRewriter(// internal traffic to outside world
		pattern to_world_pat 0 1,
		// everything else is dropped
		drop);


// OUTPUT PATH

ip_to_extern :: GetIPAddress(16)
      -> CheckIPHeader
      -> EtherEncap(0x0800, extern:eth, extern_next_hop:eth)
      -> extern_dev;
ip_to_intern :: GetIPAddress(16)
      -> CheckIPHeader
      -> [0]intern_arpq
      -> intern_dev;


// to outside world or gateway from inside network
rw[0] -> ip_to_extern_class :: IPClassifier(dst host intern, -);
  ip_to_extern_class[0] -> ip_to_host;
  ip_to_extern_class[1] -> ip_to_extern;
// to server
rw[1] -> ip_to_intern;
// only accept packets from outside world to gateway
rw[2] -> IPClassifier(dst host extern)
	-> ip_to_host;

// tcp_rw is used only for FTP control traffic
tcp_rw[0] -> ip_to_extern;
tcp_rw[1] -> ip_to_intern;


// FILTER & REWRITE IP PACKETS FROM OUTSIDE

ip_from_extern :: IPClassifier(dst host extern,
			-);
my_ip_from_extern :: IPClassifier(dst tcp ssh,
			dst tcp www or https,
			src tcp port ftp,
			tcp or udp,
			-);

extern_arp_class[2] -> Strip(14)
  	-> CheckIPHeader
	-> ip_from_extern;
ip_from_extern[0] -> my_ip_from_extern;
  my_ip_from_extern[0] -> [1]rw; // SSH traffic (rewrite to server)
  my_ip_from_extern[1] -> [1]rw; // HTTP(S) traffic (rewrite to server)
  my_ip_from_extern[2] -> [1]tcp_rw; // FTP control traffic, rewrite w/tcp_rw
  my_ip_from_extern[3] -> [4]rw; // other TCP or UDP traffic, rewrite or to gw
  my_ip_from_extern[4] -> Discard; // non TCP or UDP traffic is dropped
ip_from_extern[1] -> Discard;	// stuff for other people


// FILTER & REWRITE IP PACKETS FROM INSIDE

ip_from_intern :: IPClassifier(dst host intern,
			dst net intern,
			dst tcp port ftp,
			-);
my_ip_from_intern :: IPClassifier(dst tcp ssh,
			dst tcp www or https,
			src or dst port dns,
			dst tcp port auth,
			tcp or udp,
			-);

intern_arp_class[2] -> Strip(14)
  	-> CheckIPHeader
	-> ip_from_intern;
ip_from_intern[0] -> my_ip_from_intern; // stuff for 10.0.0.1 from inside
  my_ip_from_intern[0] -> ip_to_host; // SSH traffic to gw
  my_ip_from_intern[1] -> [2]rw; // HTTP(S) traffic, redirect to server instead
  my_ip_from_intern[2] -> Discard;  // DNS (no DNS allowed yet)
  my_ip_from_intern[3] -> ip_to_host; // auth traffic, gw will reject it
  my_ip_from_intern[4] -> [3]rw; // other TCP or UDP traffic, send to linux
                             	// but pass it thru rw in case it is the
				// returning redirect HTTP traffic from server
  my_ip_from_intern[5] -> ip_to_host; // non TCP or UDP traffic, to linux
ip_from_intern[1] -> ip_to_host; // other net 10 stuff, like broadcasts
ip_from_intern[2] -> FTPPortMapper(tcp_rw, rw, 0)
		-> [0]tcp_rw;	// FTP traffic for outside needs special
				// treatment
ip_from_intern[3] -> [0]rw;	// stuff for outside
