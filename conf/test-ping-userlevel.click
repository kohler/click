// test-ping-userlevel.click

// This configuration tests the FromDevice and ToDevice elements by sending
// pings to host 131.179.80.139 (read.cs.ucla.edu) via 'eth0'.  Change the
// 'define' statement to use another device or address, or run e.g. "click
// test-ping.click DEV=eth1" to change a parameter at the command line.
// You will need to run the configuration as root.
//
// FromDevice's "SNIFFER false" option tells Click to install kernel
// firewall rules that prevent the host kernel from processing received
// packets.  Thus, all other network traffic on $DEV will be ignored until
// the configuration stops.  This will only work on a host that supports
// iptables.
//
// You should see, printed to standard error, a sequence of "icmp echo"
// printouts intermixed with "ping :: ICMPPingSource" receive reports.

define($DEV eth0, $DADDR 131.179.80.139, $GW 131.179.33.1)

FromDevice($DEV, SNIFFER false)
	-> c :: Classifier(12/0800, 12/0806 20/0002)
	-> CheckIPHeader(14)
	-> ip :: IPClassifier(icmp echo-reply)
	-> ping :: ICMPPingSource($DEV, $DADDR)
	-> SetIPAddress($GW)
	-> arpq :: ARPQuerier($DEV)
	-> IPPrint
	-> q :: Queue
	-> ToDevice($DEV);
arpq[1]	-> q;
c[1]	-> [1] arpq;
