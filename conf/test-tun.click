# This configuration should work on FreeBSD, OpenBSD, and Linux.
# It should produce a stream of tun-ok printouts if all goes well.

tun :: Tun(tun, 1.0.0.1, 255.0.0.0);

ICMPSendPings(1.0.0.2, 1.0.0.1) -> tun;

tun -> ch :: CheckIPHeader;

ch[0] -> Print(tun-ok) -> Discard;
ch[1] -> Print(tun-bad) -> Discard;



