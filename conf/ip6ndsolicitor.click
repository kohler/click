// This configuration is faking the communication between three machines.
// grunt (3ffe:1ce1:2:0:200:c0ff:fe43:71ef) wants to send ip packets to 
// frenulum. (3ffe:1ce1:2::1)
// It first sends out the packet to the default router 
// (the router assume to have ip6 addresses: 3ffe:1ce1:2:0:200::1 & 
// 3ffe:1ce1:2::2), and router routes 
// the packet to its local interface and make a ND Solicitation message.

// It prints out the following

//Print c[2] | 134 : 0000c043 71ef0090 27e0231f 86dd6000 00000050 11403ffe 1ce10002 00000200 c0fffe43 71ef3ffe 1ce10002 00000000 00000000 00010014 d6415544 50207061 636b6574 210a0400 00000100 00000100 00000000 00000080 04080080 04085353 00005353 00000500 00000010 00000100 00005453 000054e3 040854e3 0408d801 00001369 1369

//Print route1-ok | 120 : 60000000 00501140 3ffe1ce1 00020000 0200c0ff fe4371ef 3ffe1ce1 00020000 00000000 00000001 0014d641 55445020 7061636b 6574210a 04000000 01000000 01000000 00000000 00800408 00800408 53530000 53530000 05000000 00100000 01000000 54530000 54e30408 54e30408 d8010000 13691369 

//Print after-IP6NDSolicitor-output0 |  86 : 3333ff00 000100e0 2905e56f 86dd6000 00000020 3aff3ffe 1ce10002 00000000 00000000 0001ff02 00000000 00000000 0001ff00 00018700 b0840000 00003ffe 1ce10002 00000000 00000000 00010101 00e02905 e56f


InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd 
60 00 00 00  00 50 11 40  3f fe 1c e1  00 02 00 00
02 00 c0 ff  fe 43 71 ef  3f fe 1c e1  00 02 00 00
00 00 00 00  00 00 00 01  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08   
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c::Classifier(12/86dd 20/3aff 54/87,
		         12/86dd 20/3aff 54/88,
			 12/86dd);
	
	
c[0] -> Print(c[0], 200) -> Discard;
c[2] -> Print(c[2], 200) -> Strip(14)
	-> CheckIP6Header(3ffe:1ce1:2:0:200::ffff 3ffe:1ce1:2::ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	3ffe:1ce1:2::2/128 ::0 0,
	3ffe:1ce1:2:0:200::1/128 ::0 0,
	3ffe:1ce1:2::/80 ::0 1,
	3ffe:1ce1:2:0:200::/80 ::0 2,
	0::ffff:0:0/96 ::0 3,
  	::/0 ::c0a8:1 4);

nds :: IP6NDSolicitor(3ffe:1ce1:2::1, 00:e0:29:05:e5:6f);
	
rt[1] 	-> Print(route1-ok, 200) 
	-> [0]nds;
		
c[1] 	-> Print(c[1], 200) ->[1]nds;
nds[0] -> Print(after-IP6NDSolicitor-output0, 200)
	-> Discard;
rt[0] 	-> Print(route0-ok, 200) -> Discard;
rt[2] 	-> Print(route2-ok, 200) -> Discard;
rt[3] 	-> Print(route3-ok, 200) -> Discard;
rt[4] 	-> Print(route4-ok, 200) -> Discard;
