FromDevice(wi0, 1)
  -> cl :: Classifier(12/0800 23/06, 12/0806 20/0001)
  -> Strip(14)
  -> CheckIPHeader
  -> Strip(20)
  -> ToyTCP(53)
  -> IPEncap(6, 10.2.2.2, 10.0.0.1)
  -> SetTCPChecksum
  -> CheckTCPHeader
  -> EtherEncap(0x0800, 00:02:2d:00:42:96, 0:e0:98:1:f2:5c)
  -> td :: ToDevice(wi0);

cl[1]
  -> ARPResponder(10.2.2.2 00:02:2d:00:42:96)
  -> td;
