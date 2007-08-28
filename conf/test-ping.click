// test-ping.click

// This kernel configuration tests the FromDevice and ToDevice elements
// by sending pings to host 131.179.80.139 (read.cs.ucla.edu) via 'eth1'.
// Change the 'define' statement to use another device or address, or run
// e.g. "click test-ping.click DEV=eth0" to change a parameter at the
// command line.
//
// You should see, in 'dmesg' or /var/log/messages, a sequence of "icmp echo"
// printouts intermixed with "ping :: ICMPPingSource" receive reports.  Also
// check out the contents of /click/ping/summary.

define($DEV eth1, $DADDR 131.179.80.139, $GW 131.179.33.1)

FromDevice($DEV)
	-> c :: Classifier(12/0800, 12/0806 20/0002, -)
	-> CheckIPHeader(14)
	-> ip :: IPClassifier(icmp echo-reply, -)
	-> ping :: ICMPPingSource($DEV, $DADDR)
	-> SetIPAddress($GW)
	-> arpq :: ARPQuerier($DEV)
	-> IPPrint
	-> q :: Queue
	-> { input -> t :: PullTee -> output; t [1] -> ToHostSniffers($DEV) }
	-> ToDevice($DEV);
arpq[1]	-> q;
c[1]	-> t :: Tee
	-> [1] arpq;
t[1]	-> host :: ToHost;
c[2]	-> host;
ip[1]	-> host;
