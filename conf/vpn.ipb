//
// Virtual Private Networking using IPSec
//

// zwolle's side

// 18.26.4.0 -> zeus -> IPSec Tunnel -> redlab -> 18.26.4.200
//
// IPSec Security Policy Database
//
// 0. Packets destined for tsb
// 1. Tunnel packets from redlab to us
// 2. ARP packets

spd :: Classifier(12/0800 30/121a04c8,
              12/0800 30/121a0461 26/121a040a,
              12/0806 20/0002,
              -);

FromBPF(de0) -> [0]spd;
outq :: Queue(20) -> ToBPF(de0);

//
// IPSec Incoming SAD (Security Association Database)
// 
//
// 0. ESP Packets to zeus with SPI 0x00000001
//

isad :: Classifier(9/32 16/121a0461 20/00000001,
               9/32,
	       -);
//
// IPSec Outgoing SAD (Security Association Database)
// 
//
// 0. Any packets to 18.26.4.200
//

osad :: Classifier(16/121a04c8,
	      -);

// Common code to send IP packets, including ARP.
// IP dst annotation must already be set.
arpq :: ARPQuerier(18.26.4.97, 00:00:c0:ca:68:ef);
spd[2] -> [1]arpq;
arpq[0] -> outq;


isad[0] -> Strip(20)
        -> Des(1, FFFFFFFFFFFFFFFF, 0123456789abcdef)
        -> DeEsp
        -> Print(rtun)
        -> GetIPAddress(16)
        -> StaticIPLookup(18.26.4.22 18.26.4.22)
	-> [0]arpq;

isad[1] -> Discard;

isad[2] -> Print(Not in SAD)
        -> Discard;


osad[0] -> Print(tun)
        -> Annotate
        -> Esp(0x00000001, 8)
        -> Des(0, FFFFFFFFFFFFFFFF, 0123456789abcdef)
        -> IPEncap(50, 18.26.4.97, 18.26.4.10)
        -> GetIPAddress(16)
        -> StaticIPLookup(18.26.4.10 18.26.4.10)
        -> [0]arpq;

osad[1] -> Print("How'd we get here?")
        -> Discard;


spd[0] -> Strip(14)
       -> [0]osad;
spd[1] -> Strip(14)
       -> [0]isad;
spd[3] -> Discard;

























