// This configuration should work on FreeBSD, OpenBSD, and Linux.
// It should produce a stream of tun-ok printouts if all goes well.
// On OpenBSD, you may need to run
//   route add 1.0.0.0 -interface 1.0.0.1
// after you start the click configuration.

tun :: KernelTap(1.0.0.1/8);

ICMPSendPings(1.0.0.2, 1.0.0.1)
	-> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
	-> tun;

tun -> Strip(14)
	-> ch :: CheckIPHeader;

ch[0] -> Print(tun-ok) -> Discard;
ch[1] -> Print(tun-bad) -> Discard;



