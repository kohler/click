// buffer than grid.click, this configuration uses the PacketSocket element for device input and output

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(00:90:27:E0:23:03, 10.0.0.1)
h :: Hello(1, 00:90:27:E0:23:03, 10.0.0.1)

// device layer els
ps :: PacketSocket(eth0)
q :: Queue -> Print(qps) -> ps

// linux ip layer els
linux :: Tun(tap, 10.0.0.1, 0.0.0.0)

// hook it all up
ps -> Classifier(12/Babe) -> [0] nb [0] -> q 

linux -> [1] nb [1] -> Queue -> linux

h -> q 

