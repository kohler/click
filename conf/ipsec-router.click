// simple-ipsec.click An IP Router Configuration with IPsec/ESP support
 
// This file is an extended version of a canonical IP router that also supports
// IPsec ESP tunnels between gateways
// This configuration uses network sources (FromDevice and to Device elements) 
 
 
// Kernel configuration for cone as a router between
// 18.26.4 (eth0) and 18.26.7 (eth1). Moreover an 
// IPsec tunnel is configured between this router(18.26.4.24) and another(18.26.4.1) which
// are used for connecting 18.26.7 and 18.26.8. At the end of the day a packet that is
// sent from 18.26.7 and has to arrive somewhere at 18.26.8 enters the IPsec tunnel between
// the aformentioned gateways. 
// To illustrate details:
// (18.26.7)<-->(Router1)<-->(18.26.4)<-->(Router2)<-->(18.26.8)
// To illustrate the IPsec/ESP Tunnel:
// (18.26.7)<-->(Router1)<--IPsec/ESP-->(Router2)<-->(18.26.8) 
 
// Proxy ARPs for 18.26.7 on eth0.
 
// This routers interfaces are configured as follows:
// eth0, 00:50:BF:01:0C:91, 18.26.4.24
// eth1, 00:50:BF:01:0C:5D, 18.26.7.1
 
// 0. ARP queries
// 1. ARP replies
// 2. IP
// 3. Other
// We need separate classifiers for each interface because
// we only want proxy ARP on eth0.
 
c0 :: Classifier(12/0806 20/0001,
                  12/0806 20/0002,
                  12/0800,
                  -);
 
c1 :: Classifier(12/0806 20/0001,
                  12/0806 20/0002,
                  12/0800,
                  -);
  
FromDevice(eth0) -> [0]c0;
FromDevice(eth1) -> [0]c1;
 
 
out0 :: Queue(200) -> ToDevice(eth0);
out1 :: Queue(200) -> ToDevice(eth1);
//This packet goes to linux stack
tol :: ToHost();
  
// An "ARP querier" for each interface.
arpq0 :: ARPQuerier(18.26.4.24, 00:50:BF:01:0C:91);
arpq1 :: ARPQuerier(18.26.7.1, 00:50:BF:01:0C:5D);
 
// Deliver ARP responses to ARP queriers as well as Linux.
t :: Tee(3);
c0[1] -> t;
c1[1] -> t;
t[0] -> tol;
t[1] ->[1]arpq0
t[2] ->[1]arpq1
 
// Connect ARP outputs to the interface queues.
arpq0 -> out0;
arpq1 -> out1;
 
// Proxy ARP on eth0 for 18.26.8, as well as cone's IP address.
ar0 :: ARPResponder(18.26.4.24 00:50:BF:01:0C:91,
                    18.26.7.0/24 00:50:BF:01:0C:91);
c0[0] ->ar0 -> out0;
 
// Ordinary ARP on eth1.
ar1 :: ARPResponder(18.26.7.1 00:50:BF:01:0C:5D);
c1[0] -> ar1 -> out1;
 
// The IP routing table determines which incoming and outgoing packets are comming from or need to enter the IPsec tunnel respectively.
// Incoming packets will have this routers' IP as destination address and in that case the IP protocol number is checked to determine
// whether this packet is an IPsec/ESP packet, in which case it is propagated to the IPsec incoming processing path, or a packet for 
// this router that should be propagated to the linux stack. The packets that exit the IPsec processing path visit IP routing 
// table again to be properly forwarded. 
// Outgoing packets are sent to the outgoing IPsec/ESP processing path, they get encapsulated and therefore they visit the IP routing
// table a second time in order to be properly forwarded.
 
//IP routing table. The 0,1,2 outputs are _reserved_ for the described usage below in RadixIPsecLookup:
// 0: packets for this machine that may belong to an IPsec tunnel (if they don't the RadixIPsecLookup code changes outgoing port to number 2 below)  
// 1: packets for 18.26.8 via corresponding gateway (18.26.4.1) with which there is an IPsec tunnel.
// 2: packets for this machine which cannot belong to IPsec tunnel (because they originate from a 18.26.7).This port must be connected to 
//    linux stack
//
// Apart from these you have to enter canonical IP router paths from output port 3 and onwards:
// 3: packets for 18.26.4.1 which is a router to which we send prepared outgoing ESP packets that visit IP routing table for the second time
//    as regular outgoing packets
// 4: Canonical IP routing for packets that are for any machine at 18.26.7
  
 rt :: RadixIPsecLookup(18.26.4.24/32 0,
		        18.26.4.1/32 3, 
		        18.26.7.1/32 2,
		        18.26.7.0/24 4,  	
		        18.26.8.0/24 18.26.4.1 1 234 ABCDEFFF001DEFD2354550FE40CD708E 112233EE556677888877665544332211 300 64);
 
// IPsec incoming packet IP table visit order 
// rt[0]->rt[4]
// IPsec outgoing packet IP table visit order
// rt[1]->rt[3]
 
// Hand incoming IP packets to the routing table.
// CheckIPHeader checks all the lengths and length fields
// for sanity.
ip ::   Strip(14)
     -> CheckIPHeader(INTERFACES 18.26.4.24/24 18.26.7.1/24)
     -> [0]rt;
c0[2] -> Paint(1) -> ip;
c1[2] -> Paint(2) -> ip;
 
// IP packets for this machine.
// ToHost expects ethernet packets, so cook up a fake header.
rt[2] -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> tol;
 
//1 entering the ipsec tunnel...
//ESP Encapsulate -> Authenticate -> Encrypt -> IP Encapsulate -> send back to IP routing table 
rt[1]  	-> espen :: IPsecESPEncap()
        -> cauth :: IPsecAuthHMACSHA1(0)
        -> encr :: IPsecAES(1)
        -> ipencap :: IPsecEncap(50)
        -> [0]rt;
 
//0 packets arriving from a tunnel...
//Strip IP header -> Decrypt -> Authenticate -> Decapsulate ESP -> send back to IP routing table
rt[0] -> StripIPHeader()
      -> decr :: IPsecAES(0)
      -> vauth :: IPsecAuthHMACSHA1(1)
      -> espuncap :: IPsecESPUnencap()
      -> CheckIPHeader()
      -> [0]rt;
 
rt[3] -> DropBroadcasts
      -> cp1 :: PaintTee(1)
      -> gio1 :: IPGWOptions(18.26.4.24)
      -> FixIPSrc(18.26.4.24)
      -> dt1 :: DecIPTTL
      -> fr1 :: IPFragmenter(1500)
      -> [0]arpq0
 
// Canonical IP router processing paths
// we've committed to a
// particular output device.
// Check paint to see if a redirect is required.
// Process record route and timestamp IP options.
// Fill in missing ip_src fields.
// Discard packets that arrived over link-level broadcast or multicast.
// Decrement and check the TTL after deciding to forward.
// Fragment.
// Send outgoing packets through ARP to the interfaces.
 
rt[4] -> DropBroadcasts
      -> cp2 :: PaintTee(2)
      -> gio2 :: IPGWOptions(18.26.7.1)
      -> FixIPSrc(18.26.7.1)
      -> dt2 :: DecIPTTL
      -> fr2 :: IPFragmenter(1500)
      -> [0]arpq1;
 
// DecIPTTL[1] emits packets with expired TTLs.
// Reply with ICMPs. Rate-limit them?
dt1[1] -> ICMPError(18.26.4.24, timeexceeded) -> [0]rt;
dt2[1] -> ICMPError(18.26.4.24, timeexceeded) -> [0]rt;
 
// Send back ICMP UNREACH/NEEDFRAG messages on big packets with DF set.
// This makes path mtu discovery work.
fr1[1] -> ICMPError(18.26.7.1, unreachable, needfrag) -> [0]rt;
fr2[1] -> ICMPError(18.26.7.1, unreachable, needfrag) -> [0]rt;
 
// Send back ICMP Parameter Problem messages for badly formed
// IP options. Should set the code to point to the
// bad byte, but that's too hard.
gio1[1] -> ICMPError(18.26.4.24, parameterproblem) -> [0]rt;
gio2[1] -> ICMPError(18.26.4.24, parameterproblem) -> [0]rt;
 
// Send back an ICMP redirect if required.
cp1[1] -> ICMPError(18.26.4.24, redirect, host) -> [0]rt;
cp2[1] -> ICMPError(18.26.7.1, redirect, host) -> [0]rt;
 
// Unknown ethernet type numbers.
c0[3] -> Discard;
c1[3] -> Discard;
