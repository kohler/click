//This configuration file is for NAT test.
//a fake ipv6 package that address to a IPv4 node (0::ffff:1261:027d)
// is passed through the route table and be recognized its destination is 
//actually a ipv4 node, and go through Nat624 (the 6-to-4 translator) 
// become a ipv4 packet. Next it should passed to CheckIPHeader, GetIPAddress, 
// LookupIPRoute.



InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd
60 00 00 00  00 50 11 40  00 00 00 00  00 00 00 00  
00 00 00 00  c0 a8 00 01  00 00 00 00  00 00 00 00  
00 00 ff ff  12 61 02 7d  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c::Classifier(12/86dd);
	
	c[0] -> Strip(14)
	-> CheckIP6Header(::c0a8:ffff ::1261:ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	::1261:027d ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
	::c0a8:0001 ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000 ::0 1,
	0::ffff:0:0 ffff:ffff:ffff:ffff:ffff:ffff:0:0 ::0 2,
  	::0 ::0 ::c0a8:1 3);
	
	rt[0] 	-> Print(route0-ok, 200) -> Discard;
	rt[1] 	-> Print(route1-ok, 200) -> Discard;
	rt[2] 	-> Print(route2-ok, 200) 
		-> Nat624(18.2.0.1 ::5555:5555:5555, 18.2.0.2)
		-> Print(after-Nat624, 200)
		-> Discard;
	rt[3] 	-> Print(route3-ok, 200) -> Discard;