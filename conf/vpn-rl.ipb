//
// Virtual Private Networking using IPSec
//

// redlab's side

// 18.26.4.22 -> zeus -> IPSec Tunnel -> redlab -> 18.26.4.200
//
// IPSec Security Policy Database
//
// 0. Packets for 18.26.4.22
// 1. Tunnel packets from zwolle to us
// 2. ARP packets

spd :: Classifier(12/0800 30/121a0416,
              12/0800 30/121a040a 26/121a0461,
              12/0806 20/0002,
              -);

FromBPF(de0) -> [0]spd;
outq :: Queue(20) -> ToBPF(de0);

//
// IPSec Incoming SAD (Security Association Database)
// 
//
// 0. ESP Packets to 18.26.4.0a with SPI 0x00000001
//

isad :: Classifier(9/32 16/121a040a 20/00000001,
	       -);
//
// IPSec Outgoing SAD (Security Association Database)
// 
//
// 0. Any packets headed to 18.26.04
//

osad :: Classifier(16/121a04,
	      -);

// Common code to send IP packets, including ARP.
// IP dst annotation must already be set.
arpq :: ARPQuerier(18.26.4.10, 00:00:c0:c7:71:ef);
spd[2] -> [1]arpq;
arpq[0] -> outq;


isad[0] -> Strip(20)
        -> Des(1, FFFFFFFFFFFFFFFF, 0123456789abcdef)
        -> DeEsp
	-> Print(rtun)
        -> GetIPAddress(16)
        -> StaticIPLookup(18.26.4.200 18.26.4.200)
	-> [0]arpq;

isad[1] -> Discard;


osad[0] -> Print(tun)
        -> Annotate
        -> Esp(0x00000001, 8)
        -> Des(0, FFFFFFFFFFFFFFFF, 0123456789abcdef)
        -> IPEncap(50, 18.26.4.10, 18.26.4.97)
        -> GetIPAddress(16)
        -> StaticIPLookup(18.26.4.97 18.26.4.97)
        -> [0]arpq;

osad[1] -> Print("How'd we get here?")
        -> Discard;


spd[0] -> Strip(14)
       -> [0]osad;
spd[1] -> Strip(14)
	->Print(rtun)
       -> [0]isad;
spd[3] -> Discard;

























