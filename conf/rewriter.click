				// Fake interface fl0 to net 1.0.0.0
fromlinux :: FromHost(fl0, 10.0.0.0/24)
tolinux :: ToHost
t :: Tee
				// IP packets from 18.26.4.45 --> output(0)
				// ARP replies --> output(1)
				// everything else --> output(2)
//PDOS
cl1 :: Classifier(12/0800 26/121a04 30/121a042f, 12/0806 20/0002, -)
exceptny :: Classifier(12/0800 26/121a0441 30/121a042f, -)
//MEDIAONE
				// match any packet from 18.26.4.*
//cl1 :: Classifier(12/0800 26/121a04 30/1893143f, 12/0806 20/0002, -)
				// except ny!
//exceptny :: Classifier(12/0800 26/121a0441 30/1893143f, -)
				// Pick out ARP requests
cl2 :: Classifier(12/0806 20/0001, -)
				// Give proxy ARP reply for net 1.0.0.0
arr :: ARPResponder(10.0.0.1 1:1:1:1:1:1,
		   10.0.0.0/24 1:1:1:1:1:1)
//MEDIAONE
//arq :: ARPQuerier(24.147.20.63, 00:E0:98:03:7C:AE)
//rw :: Rewriter(1.0.0.0,24.147.20.63,3600)
//PDOS
arq :: ARPQuerier(18.26.4.47, 00:E0:98:03:7C:AE)
rw :: Rewriter(10.0.0.0,18.26.4.47,10)

FromDevice(eth0) -> cl1
cl1[0] -> exceptny
exceptny[0] -> tolinux
exceptny[1] -> Strip(14)
     -> Print(cl1.0, 60) 
     -> [0]rw[0]		// stub,gavia -> 1.0.0.1,gavia
     -> Print(cl1.0rw, 60)
     -> EtherEncap(0x0800, 1:1:1:1:1:1, 0:0:0:0:0:0)
     -> tolinux
cl1[1] -> t
cl1[2] -> tolinux

t[0] -> Print(arqr) -> [1]arq
t[1] -> tolinux

fromlinux -> cl2
cl2[0] -> Print(cl2.0, 60)
       -> arr 
       -> tolinux
cl2[1] -> Strip(14)
     -> Print(cl2.1, 60)
     -> [1]rw[1]		// gavia,1.0.0.1 -> gavia,stub
     -> Print(cl2.1rw)
     -> [0]arq[0]
     -> Print(arq, 60)
//     -> EtherEncap(0x0800, 00:E0:98:03:7C:AE, 00:80:C8:4B:26:5C)
     -> ToDevice(eth0)

