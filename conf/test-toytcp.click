FromDevice(eth0, 0)
  -> cl :: Classifier(12/0800 23/06, 12/0806 20/0001)
  -> Strip(34)
  -> ToyTCP(5432)
//  -> IPEncap(6, 10.2.2.2, 10.0.0.1)
  -> IPEncap(6, 18.26.4.106, 18.26.4.123)
  -> SetTCPChecksum
//  -> EtherEncap(0x0800, 00:02:2d:00:42:96, 0:e0:98:1:f2:5c)
  -> EtherEncap(0x0800, 0:e0:29:5:e5:6f, 00:90:27:e0:23:1f)
  -> td :: ToDevice(eth0);

cl[1]
//  -> ARPResponder(10.2.2.2 00:02:2d:00:42:96)
  -> ARPResponder(18.26.4.106 0:e0:29:5:e5:6f)
  -> td;
