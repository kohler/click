//This configuration file is for GNAT test.
//a fake ipv6 package that address to a IPv4 node (0::ffff:1261:027d)
// is passed through the route table and be recognized its destination is 
//actually a ipv4 node, and go through GNAT (Address Translator and 
// Protocol Translator 
// become a ipv4 packet. Next it should be passed to CheckIPHeader, 
//GetIPAddress, etc, as if it is a normal ip4 packet


InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd
60 00 00 00  00 50 11 40  3f fe 1c e1  00 02 00 00
02 00 00 00  00 00 00 02  00 00 00 00  00 00 00 00  
00 00 ff ff  12 1a 04 3c  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> c::Classifier(12/86dd);
	
	arp::ARPQuerier(18.26.4.116, 00:a0:c9:9c:fd:9e); 
	
	InfiniteSource( \<0000c043 71ef0090 27e0231f  86dd
60 00 00 00  00 50 11 40  3f fe 1c e1  00 02 00 00
02 00 00 00  00 00 00 02  00 00 00 00  00 00 00 00  
00 00 ff ff  12 1a 04 3c  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	->c2::Classifier(12/0806 20/0002)-> Print(OK, 200)
	->[1]arp;
	
	
	c[0] -> Strip(14)
	-> CheckIP6Header(::ffff:121f:ffff)
	-> GetIP6Address(24)
	-> rt6 :: LookupIP6Route(
	3ffe:1ce1:2:0:200::1/128 ::0 0,
	3ffe:1ce1:2:0:200::/80 ::0 1,
	0::ffff:0:0/96 ::0 2,
  	::0/0 ::0 3);
	at::AddressTranslator(0,
				1 1 0 0 0 0, 
				//0 0 1 1 0 0,
				0 0 1 1 0 0,
				//1 1 0 0 0 0,
				0,
				::ffff:18.26.4.125 1300 1310);
	pt::ProtocolTranslator();
		 
	rt6[0] 	-> Print(route0-ok, 200) -> Discard;
	rt6[1] 	-> Print(route1-ok, 200) -> [1]at;
	rt6[2] 	-> Print(route2-ok, 200) 
		->[0]at;
	
	at[0] -> Print(after-AT-0, 200)
		-> [0]pt
		-> Print(after-PT, 200)
		
		-> CheckIPHeader(BADSRC 18.26.4.255)
		-> rt::StaticIPLookup(18.26.4.116/32 0,
 			18.26.4.255/32 0,
 			18.26.4.0/32 0,
 			18.26.4.0 255.255.255.0 1,
 			255.255.255.255/32 0.0.0.0 0,
 			0.0.0.0/32 0,
 			0.0.0.0/0 18.26.4.60 1);
	rt[0]	-> Print(rt0, 200) -> Discard;
	rt[1]   -> dt::DecIPTTL
		-> Print(rt1, 200)
		-> [0]arp;
	arp[0]  ->Discard;
	dt[1]   -> Discard;
	rt6[3] 	-> Print(route3-ok, 200) -> Discard;
	at[1] 	-> Print(after-AT-1, 200) ->[1]pt;
	pt[1] 	-> Discard;