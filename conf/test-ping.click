// test-ping.click

// This kernel configuration tests the FromDevice and ToDevice
// elements by sending pings to host 131.179.80.139 (read.cs.ucla.edu)
// via 'eth0'. Change the 'define' statement to use another device or
// address or gateway address, or run e.g. `click-install
// test-ping.click DEV=eth1` to change a parameter at the command
// line.
//
// You should see, in `dmesg` or /var/log/messages, a sequence of
// "icmp echo" printouts intermixed with "ping :: ICMPPingSource"
// receive reports. If you don't, make sure that your device and
// gateway address are correct. (The address in
// `/click/SetIPAddress*/addr` should equal the default gateway for
// the relevant device.) Also check out the contents of
// /click/ping/summary.

define($DEV eth0, $DADDR 131.179.80.139, $GW $DEV:gw)

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
