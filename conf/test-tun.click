// test-tun.click

// This user-level configuration tests the KernelTun element, which
// accesses the Linux Universal Tun/Tap device, *BSD's /dev/tun* devices,
// or Linux's Ethertap devices (/dev/tap*).  These devices let user-level
// programs trade packets with kernel IP processing code.  You will need to
// run it as root.
//
// This configuration should work on FreeBSD, OpenBSD, and Linux.  It should
// produce a stream of 'tun-ok' printouts if all goes well.  On OpenBSD, you
// may need to run
//	route add 1.0.0.0 -interface 1.0.0.1
// after starting the Click configuration.
//
// Also try running 'ping 1.0.0.2' (or any other host in 1.0.0.1/8 except
// for 1.0.0.1).  Click should respond to those pings, and print out a
// 'tun-ping' message for each ping received.

tun :: KernelTun(1.0.0.1/8);

/* Ping the host kernel stack from a tunnel address.
   All packets sent to the KernelTun address, here 1.0.0.1, are processed by
   the host IP stack.  Its responses to these pings will be delivered to
   1.0.0.2 -- meaning they'll pop out of KernelTun.  */
ICMPPingSource(1.0.0.2, 1.0.0.1)
    -> tunq :: Queue -> tun;

tun -> ch :: CheckIPHeader
    -> ipclass :: IPClassifier(icmp type echo, icmp type echo-reply);

/* Respond to pings.
   Our own pings, sent by ICMPPingSource above, will NOT show up here, for
   two reasons.  First, our pings are addressed to 1.0.0.1, which is
   the host stack, not the tunnel; and second, KernelTun isn't a loopback
   device -- it will never emit any packet that you send to it. */
ipclass[0] -> ICMPPingResponder
    -> IPPrint(tun-ping)
    -> tunq;

/* Responses to pings from ICMPPingSource. */
ipclass[1] -> IPPrint(tun-ok) -> Discard;

/* Bad packets: bad IP headers or non-IP. */
ch[1] -> Print(tun-bad) -> Discard;
tun[1] -> Print(tun-nonip) -> Discard;

