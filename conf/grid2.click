// grid2.click

// better than grid.click, this configuration uses the PacketSocket
// element for device input and output

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(2000, 00:E0:98:09:27:C5, 18.26.7.2)
h :: Hello(500, 100, 00:E0:98:09:27:C5, 18.26.7.2)

// device layer els
ps :: FromDevice(eth0, 0)
q :: Queue -> ToDevice(eth0)

// linux ip layer els
linux :: Tun(tap, 18.26.7.2, 255.255.255.0)

// hook it all up
ps -> Classifier(12/Babe) -> [0] nb [0] -> q 

linux -> cl :: Classifier(16/121a07, // ip for 18.26.7.*
			  -) // the rest of the world
cl [0] -> GetIPAddress(16) -> [1] nb [1] -> Queue -> ck :: CheckIPHeader [0] -> linux
ck [1] -> Discard
cl [1] -> SetIPAddress(18.26.7.1) -> [1] nb // for grid gateway
h -> q 

