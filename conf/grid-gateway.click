// -*- c++ -*-
// grid-gateway.click

// single interface on gateway, tunneling to grid machines on the same
// segment.  this is broken.

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(2000, 00:90:27:E0:23:03, 18.26.7.1)
h :: Hello(500, 100, 00:90:27:E0:23:03, 18.26.7.1)

// device layer els
ps :: PacketSocket(eth0)
q :: Queue -> ps

// linux ip layer els
linux_tun :: Tun(tap, 18.26.7.1, 255.255.255.0) // our grid IP interface
to_linux_q :: Queue -> linux_tun

linux_tun2 :: Tun(tap, 18.26.4.25, 255.255.255.0) // our wire IP interface
to_linux_q2 :: Queue -> linux_tun2

// demultiplex
wire_in_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			    12/0806 20/0002, // arp replies 
			    12/0800 30/121a0419, // ip for us from wire: 18.26.4.25
			    12/0800 30/121a0701, // also for us
 			    12/0800 30/121a07) // bridge ip for 18.26.7.*

nbr_out_demux :: Classifier(12/0800 30/121a0419, // data for us, the gateway, from grid
		           -) // for everyone else

// hook it all up
ps -> cl :: Classifier(12/Babe, -) 
cl [0] -> [0] nb [0] -> q // click traffic
cl [1] -> wire_in_demux // other net traffic

linux_tun -> [1] nb [1] -> nbr_out_demux
nbr_out_demux [0] -> to_linux_q
nbr_out_demux [1] -> [0] arp :: ARPQuerier(18.26.4.25, 00:E0:98:09:27:C5) -> q

h -> q

// respond to ARPs for us, as well as proxy ARP for 18.26.7.*
wire_in_demux [0] -> ARPResponder(18.26.7.0/24 00:E0:98:09:27:C5, 18.26.4.25 00:E0:98:09:27:C5) -> q
wire_in_demux [1] -> [1] arp
wire_in_demux [2] -> Print(ip_for_us) -> to_linux_q2
wire_in_demux [3] -> Print(ip_for_us_grid) -> to_linux_q
// bridge ip from wired just like packets originated on the gateway
wire_in_demux [4] -> Strip(14) -> [1] nb 
