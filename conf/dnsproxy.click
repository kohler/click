// Demonstrate IPRewriter's ability to turn an ordinary
// named process into a transparent DNS proxy.
//
// To test:
//   click dnsproxy.click
//   Run named on the local host.
//   dig@1.0.0.2 web.mit.edu
//   It works if you got an answer -- even though there's no
//   name server running on 1.0.0.2.

// You have to remember the CheckIPHeader; otherwise IPRewriter
// will dump core looking for p->ip_header().

// You must send TCP or UDP packets to IPRewriter; otherwise
// an assertion will fail.

Idle
  -> kt :: KernelTun(1.0.0.1/8)
  -> cl :: Classifier(16/01010101 9/11,    // IP/UDP to fake address
                      9/11 22/0035,  // IP/udp/dns
                      -);

rw :: IPRewriter(pattern 1.1.1.1 1024-65535 1.0.0.1 - 0 1);

// From proxy program.
cl[0] -> CheckIPHeader -> rw;

// From requesting host.
cl[1] -> CheckIPHeader -> rw;

// To DNS proxy.
rw[0] -> kt;

// Return from DNS proxy to requesting host.
rw[1] -> kt;

cl[2] -> Print(other) -> Discard;
