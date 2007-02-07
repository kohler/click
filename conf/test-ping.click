// test-ping.click

// This kernel configuration tests the FromDevice and ToDevice elements
// by sending pings to host 18.26.4.9 (am.csail.mit.edu).
//
// It sends pings via 'eth1'; change all occurrences of 'eth1' to something
// else if you want to use another device.
//
// You should see, in 'dmesg' or /var/log/messages, a sequence of "icmp echo"
// printouts intermixed with "ping :: ICMPPingSource" receive reports.  Also
// check out the contents of /click/ping/summary.

FromDevice(eth1)
	-> c :: Classifier(12/0800, 12/0806 20/0002, -)
	-> CheckIPHeader(14)
	-> ip :: IPClassifier(icmp echo-reply, -)
	-> ping :: ICMPPingSource(eth1, 18.26.4.9)
	// -> SetIPAddress(10.0.1.1)
	-> IPPrint
	-> arpq :: ARPQuerier(eth1)
	-> Queue
	-> { input -> t :: PullTee -> output; t -> ToHostSniffers(eth1) }
	-> ToDevice(eth1);
c[1]	-> t :: Tee
	-> [1] arpq;
t[1]	-> host :: ToHost;
c[2]	-> host;
ip[1]	-> host;
