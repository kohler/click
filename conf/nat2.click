//This configuration file fake a ICMP6 Packet which is embedded in a IP6 Header

InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd
60000000 00803aff 
3ffe1ce1 00020000 02000000 00000002  
00000000 00000000 0000ffff 121a043c 
03003e80 00000000 
60000000 00501101 
3ffe1ce1 00020000 02000000 00000002 
00000000 00000000 00000000 1261027d 
0014d641 55445020 7061636b 6574210a 04000000 
01000000 01000000 00000000 00800408 00800408 
53530000 53530000 05000000 00100000 01000000 
54530000 54e30408 54e30408 d8010000 13691369>, 1, 5)

	-> c::Classifier(12/86dd)
	-> Strip(14)
	-> CheckIP6Header(::ffff:121f:ffff)
	-> GetIP6Address(24)
	-> rt6 :: LookupIP6Route(
	3ffe:1ce1:2:0:200::1/128 ::0 0,
	3ffe:1ce1:2:0:200::/80 ::0 1,
	0::ffff:0:0/96 ::0 2,
  	::0/0 ::0 3);
	
	rt6[0] 	-> Print(route0-ok, 200) -> Discard;
	rt6[1] 	-> Print(route1-ok, 200) -> Discard;
	rt6[2] 	-> Print(route2-ok, 200) 
		-> Nat624(18.26.4.125 3ffe:1ce1:2:0:200::2, 18.26.4.123)
		-> Print(after-Nat624, 200)
		-> Discard;
	rt6[3] 	-> Print(route3-ok, 200) -> Discard;