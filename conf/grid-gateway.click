// need to get our interface addresses right...

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(00:E0:98:09:27:C5, 10.0.0.2)
h :: Hello(1, 00:E0:98:09:27:C5, 10.0.0.2)

// device layer els
grid_src :: FromBPF(eth0, 1)
grid_dst :: ToBPF(eth0) 

wired_src :: FromBPF(eth0, 1)
wired_dst :: ToBPF(eth0)

// linux ip layer els
linux_tun :: Tun(tap, 10.0.0.2, 0.0.0.0) 
to_linux_q :: Queue -> linux_tun

// demultiplex
wire_in_demux :: Classifier(12/0806 20/0001, // arp request, for proxy reply
			    12/0806 20/0002, // arp replies 
			    12/0800 30/12220473, // ip for us
 			    12/0800 30/122207) // bridge ip for 18.26.7.*

nbr_out_demux :: Classifier(12/0800 30/0a000002, // for gateway
		           -) // everything else

// hook it all up
grid_src -> Classifier(12/Babe) -> [0] nb [0] -> grid_dst

linux_tun -> [1] nb [1] -> nbr_out_demux
nbr_out_demux [0] -> to_linux_q
nbr_out_demux [1] -> [0] arp :: ARPQuerier(10.0.0.2, 00:E0:98:09:27:C5) -> wired_dst

h -> grid_dst

wired_src -> wire_in_demux
// respond to ARPs for us, as well as proxy ARP for 18.26.7.*
wire_in_demux [0] -> ARPResponder(18.26.7.0/24 00:E0:98:09:27:C5, 18.26.4.115 00:E0:98:09:27:C5) -> wired_dst
wire_in_demux [1] -> [1] arp
wire_in_demux [2] -> to_linux_q;
// bridge ip from wired just like packets originated on the gateway
wire_in_demux [3] -> Strip(14) -> [1] nb 
