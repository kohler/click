// testip6.click

//  This configuration should print this line at the end:
//  Print ok | 120 : 60000000 00501140 00000000 00000000 00000000 c0a80001
//  You can run it at user level as
// ` [userlevel]#./click ..conf/testip6.click'


InfiniteSource( \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
60 00 00 00  00 50 11 40  00 00 00 00  00 00 00 00  
00 00 00 00  c0 a8 00 01  00 00 00 00  00 00 00 00  
00 00 00 00  12 61 02 7d  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c::Classifier(12/0806 20/0001,
                  12/0806 20/0002,
                  12/0800,
                  -);
	t::Tee(3);
	c[1] -> [0]t;
	arpq1 :: ARPQuerier6(::1261:027d, 00:A0:CC:52:14:33);
	ar1 :: ARPResponder6(::1261:027d ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff 00:A0:CC:52:14:33);
	t[0] -> [1]arpq1;
	t[1] -> Discard;
	t[2] -> Discard;
	c[0] -> ar1 -> Discard;
	//c[1] -> Discard;
	c[3] -> Discard;
	c[2] -> Strip(14)
	-> CheckIP6Header(::c0a8:ffff ::1261:ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	::1261:027d ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
	::c0a8:0001 ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000 ::0 1,
  	::0 ::0 ::c0a8:1 1);
	icmpjunk :: Classifier(44/1261FFFF,44/C0A8FFFF,-);
	icmpdisc :: Discard;
	icmpjunk[0] -> icmpdisc;
	icmpjunk[1] -> icmpdisc;
	icmpjunk[2] -> [0]rt;
	//icmpjunk[2] -> icmpdisc;
	rt[0] 	-> Print(ok, 24) -> Discard;
	rt[1] 	-> dt1 :: DecIP6HLIM
	      	-> fr1 :: IP6Fragmenter(150) 
		-> [0]arpq1 -> Queue(200) 
		-> Print(ok, 3) -> Discard;
	
	dt1[1]  -> ICMP6Error(::c0a8:ffff, 3, 0) 
		-> Print(ok, 3) -> icmpjunk;
	fr1[1]	-> ICMP6Error(::c0a8:ffff, 2, 0) 
		-> Print(ok, 12) -> icmpjunk;
	
