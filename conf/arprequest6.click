//-> ARPQuerier6(3ffe:1ce1:0:b5::1 00:e0:29:05:e5:6f)

InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd
60 00 00 00  00 50 11 40  00 00 00 00  00 00 00 00  
00 00 00 00  c0 a8 00 01  3f fe 00 00  00 00 00 00  
00 00 00 00  12 61 02 7d  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c::Classifier(12/86dd);

InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd
60 00 00 00  00 50 11 40  00 00 00 00  00 00 00 00  
00 00 00 00  c0 a8 00 01  00 00 00 00  00 00 00 00  
00 00 00 00  12 61 02 7d  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c2::Classifier(12/86dd);
	
	c[0] -> Strip(14)
	-> CheckIP6Header(::c0a8:ffff ::1261:ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	3ffe::1261:027d ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::2222 0,
	::c0a8:0001 ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000 ::0 1,
	0::ffff:0:0 ffff:ffff:ffff:ffff:ffff:ffff:0:0 ::0 2,
  	::0 ::0 ::c0a8:1 3);
	arpq :: ARPQuerier6(3ffe:1ce1:0:b5::1, 00:e0:29:05:e5:6f);
	
	rt[0] 	-> Print(route0-ok, 200) 
		-> [0]arpq;
		
	c2[0] 	-> [1]arpq;
	arpq[0]-> Print(afterARPQuerier6-output0, 200)
	
	
		-> Discard;
	rt[1] 	-> Print(route1-ok, 200) -> Discard;
	rt[2] 	-> Print(route2-ok, 200) -> Discard;
	rt[3] 	-> Print(route3-ok, 200) -> Discard;



  	