// This configuration is faking the communication between three machines.
// grunt (3ffe:1ce1:2:0:200:c0ff:fe43:71ef) wants to send ip packets to 
// frenulum. (3ffe:1ce1:2::2
// It first sends out the packet to the default router 
// (darkstar - 3ffe:1ce1:2:0:200::1 & 3ffe:1ce1:2::1), and router routes 
// the packet to its local interface and sends out the 
// NB. Solitation Message query the target ip6 address.


InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd 
60 00 00 00  00 50 11 40  3f fe 1c e1  00 02 00 00
02 00 c0 ff  fe 43 71 ef  3f fe 1c e1  00 02 00 00
00 00 00 00  00 00 00 02  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08   
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c::Classifier(12/86dd 20/3aff 53/87,
		         12/86dd 20/3aff 53/88,
			 12/86dd);
	
	
	c[0] -> Discard;
	c[2] -> Strip(14)
	-> CheckIP6Header(3ffe:1ce1:2:0:200::ffff 3ffe:1ce1:2::ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	3ffe:1ce1:2::1 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
	3ffe:1ce1:2:0:200::1 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
	3ffe:1ce1:2:: ffff:ffff:ffff:ffff:ffff:: ::0 1,
	3ffe:1ce1:2:0:200:: ffff:ffff:ffff:ffff:ffff:: ::0 2,
	0::ffff:0:0 ffff:ffff:ffff:ffff:ffff:ffff:: ::0 3,
  	::0 ::0 ::c0a8:1 4);
	arpq :: ARPQuerier6(3ffe:1ce1:2::1, 00:e0:29:05:e5:6f);
	
	rt[1] 	-> Print(route1-ok, 200) 
		-> [0]arpq;
		
	c[1] 	-> [1]arpq;
	arpq[0]-> Print(afterARPQuerier6-output0, 200)
	
	
		-> Discard;
	rt[0] 	-> Print(route1-ok, 200) -> Discard;
	rt[2] 	-> Print(route2-ok, 200) -> Discard;
	rt[3] 	-> Print(route3-ok, 200) -> Discard;
	rt[4] 	-> Print(route4-ok, 200) -> Discard;

  	