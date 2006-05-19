AddressInfo(real_interface 198.18.2.50 00:02:B3:BF:26:7A);

from    :: FromHost(fake0, 198.18.2.50/28);

from_cl :: Classifier(12/0806 20/0001, // ARP request
                      12/0806 20/0002, // ARP response
                      12/0800);        // IP packets


host_cl :: Classifier(12/0806 20/0001, // ARP request
                      12/0806 20/0002, // ARP response
                      12/0800);        // IP packets


arpQ :: ARPQuerier(real_interface);


out :: Queue(200) -> LinkUnqueue(0.04s, 10000Mbps)
                  -> Queue(200)
                  -> ToDevice(eth1);


in  :: PollDevice(eth1, PROMISC true);


from -> host_cl; // separate IP and ARP packets

// Just generate a bogus response.  This is needed for the
// OS to just give us frames with the 1:1:1:1:1:1 dst
// and  0:1:2:3:4:5 src (default for fake0 device)
host_cl[0] //-> ARPPrint("Kernel ARP")
           -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1)
           -> ToHost;

host_cl[1] -> Discard;

// ARPQuerier will make the right ethernet header for us
// we need to strip the prev eth header and check the IP
// packet validity.  as arpQ expects well formed IP packets
host_cl[2] -> Strip(14)
           -> CheckIPHeader(0, CHECKSUM true)
           -> [0]arpQ
           -> out;

//Idle -> arpQ -> Discard;

in -> Queue(200)
   -> LinkUnqueue(0.04s, 10000Mbps)
   -> from_cl;

// respond with the real MAC and not the fake
from_cl[0] //-> ARPPrint("Req")
           -> ARPResponder(real_interface)
           -> out;

// add comments
from_cl[1] //-> ARPPrint("Real ARP resp")
           -> [1]arpQ;

// send out ARP queries from the Click ARPQuerier
arpQ[1] -> out;

// These are just non-ARP packets going to the host
from_cl[2] -> ToHost;

