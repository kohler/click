// need to change the following lines to correspond to your particular
// udpgen setting - hopefully we will have a script soon

ar   :: ARPResponder(7.0.0.2 255.255.255.255 00:E0:29:05:E2:D4);
iph  :: IPEncap(17, 7.0.0.2, 5.0.0.2);
ethh :: EtherEncap(0x0800, 00:E0:29:05:E2:D4, 00:C0:95:E2:09:14);

c0   :: Classifier(12/0806 20/0001, -);
pd   :: PollDevice(eth1);
td   :: ToDevice(eth1);
out  :: Queue(8192) -> Counter -> td;
tol  :: ToLinux;

pd -> [0]c0;
c0[0] -> ar -> out;
c0[1] -> tol;

// ether header size 14
// ip    header size 20
// udp   header size 8
// pkt len is 42+length of payload
// generates 64 byte packets, which have 22 byte payloads

rs :: RatedSource(\<00000000111111112222222233333333444444445555>, 100000, 10);

rs -> UDPEncap(1234,1234,1) 
   -> iph 
   -> ethh
   -> out;

// ticket for RatedSource must be smaller so it won't overflow the Queue
ScheduleInfo(td 1, pd .1, rs .1);
