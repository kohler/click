//This configuration file is for GNAT test.
//This is to simulate the situation when an IPv4 private net try to connect to the IPv6 internet (e.g. 6bone).
//18.26.4.125 => 18.26.4.60 , where 18.26.4.60 is actually some ipv4/ipv6 node on internet4/6
//src address is private address in the ipv4 local net.

InfiniteSource( \<0000c043 71ef0090 27e0231f 86dd
60000000 0050113f 00000000 00000000 0000ffff 121a043c 3ffe0000 00000000 00000000 00000001  
0514d641 55445020 7061636b 6574210a 117f0000 01000000 01000000 
00000000 00800408 00800408 53530000 53530000 05000000 00100000 01000000 54530000 54e30408 54e30408 d8010000 13691369>, 1, 5)

	-> Strip(14)
	-> CheckIP6Header()
	-> GetIP6Address(24)
	-> rt6 :: LookupIP6Route(
	3ffe:1ce1:2:0:200::1/128 ::0 0,
	3ffe::/80 ::0 1,
	0::ffff:0:0/96 ::0 2,
  	::0/0 ::0 3);

	rt6[0] 	-> Print(route60-ok, 200) -> Discard;
	
	rt6[2] 	-> Print(route62-ok, 200) -> Discard;
	rt6[3] 	-> Print(route63-ok, 200) -> Discard;

InfiniteSource( \<0000c043 71ef0090 27e0231f  0800
45000064 00004000 3f110e9d 121a047d 121a043c 
0514d641 55445020 7061636b 6574210a 3ae70000 01000000 01000000 
00000000 00800408 00800408 53530000 53530000 05000000 00100000 01000000 54530000 54e30408 54e30408 d8010000 13691369>, 1, 5)

	-> Strip(14)
	-> CheckIPHeader()
	-> rt::StaticIPLookup(18.26.4.112/32 0,
 			18.26.4.255/32 0,
 			18.26.4.0/32 0,
 			18.26.4.0 255.255.255.0 1,
 			255.255.255.255/32 0.0.0.0 0,
 			0.0.0.0/32 0,
			18.26.4.60/32 1,
 			0.0.0.0/0 18.26.4.60 2);
	pt::ProtocolTranslator();

	//AddressTranslator: first part of the argument is static mapping
	//second part of the argument is dynamic mapping
	at::AddressTranslator(1,
				//1,
				1 0 1 0 0 0,
				::ffff:18.26.4.125 3ffe::1,
				1 1 0 0 0 0, 
				//0 0 1 1 0 0,
				0 0 1 1 0 0,
				//1 1 0 0 0 0,
				0,
				::ffff:18.26.4.1 1300 1310);

	rt[0]	-> Print(rt0, 200) -> [0]pt;
		//->Discard;
	rt[1] 	-> Print(rt1, 200) 
		-> [1]pt;
	pt[1]  	-> Print(pt1, 200)	-> [0]at;
	at[0]  	-> Print(after-at0, 200) -> Discard;
	at[1]	-> Print(after-at1, 200) -> [0]pt;
	pt[0]	-> Print(after-pt0, 200) -> Discard;
	

	rt[2]   -> Print(rt2, 200)  ->Discard;
	rt6[1] 	-> Print(route61-ok, 200) -> [1]at;