// grid-single-if-gw.click

// configuration for running a click userlevel grid gateway on a
// machine with a single interface, when other grid machines are on
// the same segment.  caveat: sometimes see dups on the grid nodes.

// SETUP (under userlevel in linux): 
// `ifconfig eth0 0 -promisc'
// `insmod -x -o tap0 ethertap unit=0'
// `insmod -x -o tap1 ethertap unit=1' (ethertap must be a module)
// `route add default gw 18.26.4.1' (or whatever)
// run this configuration
// check that you have routes out the tap0 and tap1 interfaces


// device interface
eth :: FromDevice(eth0, 0) 
to_eth :: Queue -> ToDevice(eth0) 

// IP interfaces on gateway machine
tun1 :: Tun(tap, 18.26.4.25, 255.255.255.0) // gateway's regular address
to_tun1 :: Queue -> tun1

tun2 :: Tun(tap, 18.26.7.1, 255.255.255.0) // gateway's grid address
to_tun2 :: Queue -> tun2

ip_cl :: Classifier(16/121a0419, 16/121a0701, 16/121a07, -) // get IP for 18.26.7.1, 18.26.7.*, *

nb :: Neighbor(2000, 00:90:27:E0:23:03, 18.26.7.1)
nb [0] -> to_eth
nb [1] -> ip_cl

eth -> eth_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			       12/0806 20/0002, // arp replies 
			       12/0800 30/121a0419, // ip for us as 18.26.4.25
			       12/0800 30/121a0701, // ip for us as 18.26.7.1
			       12/0800 30/121a07, // ip for 18.26.7.*
			       12/BABE) // grid protocol	

eth_demux [0] -> ARPResponder(18.26.7.0/24 18.26.4.25 00:90:27:E0:23:03) -> to_eth
eth_demux [1] -> [1] arpq :: ARPQuerier(18.26.4.25, 00:90:27:E0:23:03) -> to_eth
eth_demux [2] -> Strip(14) -> Discard // linux handles 
Idle -> to_tun1
eth_demux [3] -> Strip(14) -> Discard // linux handles -> to_tun2
eth_demux [4] -> Strip(14) -> to_nb_ip :: GetIPAddress(16) -> [1] nb
eth_demux [5] -> [0] nb


ip_cl [0] -> to_tun1
ip_cl [1] -> to_tun2
ip_cl [2] -> to_nb_ip // send 18.26.7.* to Grid processing
ip_cl [3] -> gw_cl :: Classifier(16/121a04, -) // get local wired IP for 18.26.4.*
gw_cl [0] -> GetIPAddress(16) -> [0] arpq // ARP and send local net traffic
gw_cl [1] -> SetIPAddress(18.26.4.1) -> [0] arpq // ARP and send gateway traffic

tun1 -> ip_cl
tun2 -> ip_cl

h :: Hello(500, 100, 00:90:27:E0:23:03, 18.26.7.1) -> to_eth
