fd :: FromDevice(00:00:5A:98:F9:28);
td :: ToDevice(00:00:5A:98:F9:28);
out :: Queue(1024) -> td;

fd -> cl :: Classifier(12/0800, 12/0806);

cl[0]
   -> Strip(14)
   -> WebGen(7.1.0.0/16, 1.0.0.1, 500)
   -> EtherEncap(0x0800, 00:00:5A:98:F9:28, 00:03:47:0D:39:57)
   -> out;

cl[1]
   -> ARPResponder(1.0.0.2 00:00:5A:98:F9:28)
   -> out;
