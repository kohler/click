// Demonstrate IPRewriter's ability to turn an ordinary
// named process into a transparent DNS proxy.
//
// To test:
//   ../userlevel/click < dnsproxy.click
//   Run named on the local host.
//   dig@1.0.0.2 web.mit.edu
//   It works if you got an answer -- even though there's no
//   name server running on 1.0.0.2.

// You have to remember the CheckIPHeader; otherwise IPRewriter
// will dump core looking for p->ip_header().

// You must send TCP or UDP packets to IPRewriter; otherwise
// an assertion will fail.

Idle
  -> kt :: KernelTap(1.0.0.1/8)
  -> cl :: Classifier(12/0800 30/01010101 23/11,    // IP/UDP to fake address
                      12/0800 23/11 36/0035,  // IP/udp/dns
                      -);

rw :: IPRewriter(pattern 1.1.1.1 1024-65535 1.0.0.1 - 0 1);

// From proxy program.
cl[0] -> Strip(14) -> CheckIPHeader -> rw;

// From requesting host.
cl[1] -> Strip(14) -> CheckIPHeader -> rw;

// To DNS proxy.
rw[0] -> EtherEncap(0x0800, 0:0:0:0:0:0, 0:0:0:0:0:0) -> kt;

// Return from DNS proxy to requesting host.
rw[1] -> EtherEncap(0x0800, 0:0:0:0:0:0, 0:0:0:0:0:0) -> kt;

cl[2] -> Print(other) -> Discard;
