// test-tap.click

// This user-level configuration tests the KernelTap element, which accesses
// Linux's ethertap device (or, equivalently, *BSD's /dev/tun* devices). These
// devices let user-level programs trade packets with the kernel.  You will
// need to run it as root.
//
// This configuration should work on FreeBSD, OpenBSD, and Linux. It should
// produce a stream of 'tap-ok' printouts if all goes well. On OpenBSD, you
// may need to run
//   route add 1.0.0.0 -interface 1.0.0.1
// after starting the Click configuration.

tap :: KernelTap(1.0.0.1/8);

ICMPPingSource(1.0.0.2, 1.0.0.1)
	-> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2)
	-> tap;

tap -> Print(tap-in) -> c ::Classifier(12/0800, 12/0806);

c[0] -> Strip(14)
	-> ch :: CheckIPHeader
	-> IPPrint(tap-ok)
	-> Discard;
  ch[1] -> Print(tap-bad) -> Discard;

c[1] -> ARPResponder(0/0 1:1:1:1:1:1) -> tap;
