rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(00:E0:98:09:27:C5, 10.0.0.2)
h :: Hello(1, 00:E0:98:09:27:C5, 10.0.0.2)

// device layer els
src :: FromBPF(eth0, 1)
dst :: ToBPF(eth0) 

// linux ip layer els
linux :: Tun(10.0.0.2, 0.0.0.0)

// hook it all up
src -> Classifier(12/Babe) -> [0] nb [0] -> dst

linux -> [1] nb [1] -> Queue -> linux

h -> dst

