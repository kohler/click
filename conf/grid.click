rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor
h :: Hello(1, 00:90:27:E0:23:03, 10.0.0.1)

// device layer els
src :: FromBPF(eth0, 1)
dst :: ToBPF(eth0) 

// linux ip layer els
linux :: Tun(tap, 10.0.0.1, 0.0.0.0)

// hook it all up
src -> Classifier(12/Babe) -> [0] nb [0] -> dst

linux -> [1] nb [1] -> Queue -> linux

h -> dst

