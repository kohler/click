// udpgen.click

// This file is a simple, fast UDP/IP load generator, meant to be used in
// the Linux kernel module. It sends 64-byte UDP/IP packets from this
// machine to another machine at a given rate. It can generate rates up to
// about 147,000 packets/s on 733 MHz Pentium III machines using our
// polling drivers.

// The relevant address and rate arguments are specified as parameters to a
// compound element UDPGen.

// UDPGen($device, $rate, $time, $saddr, $sport, $saddr_ethernet,
//	$daddr, $dport, $daddr_ethernet);
//
//	$device		name of device to generate traffic on
//	$rate		rate to generate traffic (packets/s)
//	$limit		total number of packets to send
//	$saddr		this machine's IP address
//	$sport		source port for generated traffic
//	$saddr_ethernet	the Ethernet address of $device
//	$daddr		destination machine's IP address
//	$dport		destination port for generated traffic
//	$daddr_ethernet the next-hop Ethernet address

// After creating a UDPGen named `u', the actual packet count and generated
// rate are found in the files `/proc/click/u/counter/count' and
// `/proc/click/u/counter/rate'.


elementclass UDPGen {
  $device, $rate, $limit,
  $saddr, $sport, $saddr_ethernet, 
  $daddr, $dport, $daddr_ethernet |

  source :: RatedSource(\<00000000111111112222222233333333444444445555>,
	$rate, $limit);
  out :: Queue(8192);
  counter :: Counter;
  class :: Classifier(12/0806 20/0001, -);

  pd :: PollDevice($device)		// may be need to be FromDevice
	-> class;
  class[0]				// ARP queries
	-> ARPResponder($saddr $saddr_ethernet)
	-> out;
  class[1]				// all other packets
	-> ToLinux;

  source
	-> UDPIPEncap($saddr, $sport, $daddr, $dport, true)
	-> EtherEncap(0x0800, $saddr_ethernet, $daddr_ethernet)
	-> out;

  out
	-> counter
	-> td :: ToDevice($device);

  // ticket for RatedSource must be smaller so it won't overflow the Queue
  ScheduleInfo(td 1, pd .1, source .1);
}


// create a UDPGen

u :: UDPGen(eth0, 10, 100,
	1.0.0.2, 1234, 00:e0:29:05:e2:d4,
	2.0.0.2, 1234, 00:c0:95:e2:09:14);
