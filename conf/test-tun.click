// test-tun.click

// This user-level configuration tests the KernelTun element, which
// accesses the Linux Universal Tun/Tap device, *BSD's /dev/tun* devices,
// or Linux's Ethertap devices (/dev/tap*). These devices let user-level
// programs trade packets with kernel IP processing code. You will need to
// run it as root.
//
// This configuration should work on FreeBSD, OpenBSD, and Linux. It should
// produce a stream of `tun-ok' printouts if all goes well. On OpenBSD, you
// may need to run
//   route add 1.0.0.0 -interface 1.0.0.1
// after starting the Click configuration.

tun :: KernelTun(1.0.0.1/8);
tun[1] -> Print(tun-nonip) -> Discard;
ICMPSendPings(1.0.0.2, 1.0.0.1)
      -> tun;

tun -> ch :: CheckIPHeader;

ch[0] -> IPPrint(tun-ok)
      -> IPFilter(allow icmp type echo)
      -> ICMPPingResponder
      -> IPPrint(tun-ping)
      -> tun;
ch[1] -> Print(tun-bad)
      -> Discard;
