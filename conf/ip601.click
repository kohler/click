//ip601.click -- test ip6 routing of click
//Suppose machine A (grunt) ping6 machine B(frenulum) through our click router (mindwipe)

//grunt first sends its N.solicitation message to local link for mindwipe's link layer address since mindwipe is it's default route. mindwipe will respond it with N.Advertisement message. grunt then sends echo Request dst to frenulum to mindwipe, mindwipe routes it to frenulum (it first sends N. solicitation message on the local link, frenulum respondes it with N. Advertisement message, then mindwipe sends the packet to frenulum's with encapsulated ethernet header). If frenulum knows grunts' link layer address, it replies with a echo reply message. 

//We could set mindwipe as the router for frenulum if des is grunt. Then we have the exact process of the above from frenulum to grunt.

//grunt - ip6 address is : 3ffe:1ce1:2:0:200::2
//frenulum -ip6 address is 3ffe:1ce1:2::1
//mindwipe - ip6 addresses are : 3ffe:1ce1:2:0:200::1 and 3ffe:1ce1:2::2 
//mindwipe is our click router, the above two IP6 address is by imagination, not real, i.e. the router may be just a IPv4 only node, it can assume itself have these two ip6 address.

//Type the following for the test:

//mindwipe: in click/userlevel directory:
//mindwipe: ./click ../conf/ip601.click

//grunt: route flush -inet6
//grunt: route add -inet6 3ffe:1ce1:2::1 3ffe:1ce1:2:0:200::1
//grunt: ping6 3ffe:1ce1:2::1

//frenulum: route delete -inet6 3ffe:1ce1:2:0:200::2 or route flush -inet6
//frenulum: route add -inet6 3ffe:1ce1:2:0:200::2 3ffe:1ce1:2:0:200::1

//If you want to run this test, you can replace the router's addresses with your machine' IP6 Addresses and Ethernet Addresses in the following elements: IP6NDAdvertiser, IP6NDSolicitor, LookupIP6Route. 


	nda::IP6NDAdvertiser(3ffe:1ce1:2:0:200::1/128 00:A0:C9:9C:FD:9E, 
			fe80::2a0:c9ff:fe9c:fd9e/128 00:A0:C9:9C:FD:9E); 

	nds :: IP6NDSolicitor(fe80::2a0:c9ff:fe9c:fd9e, 00:a0:c9:9c:fd:9e);

	FromDevice(eth0, 1)
  		-> c::Classifier(12/86dd 20/3aff 54/87,
		         12/86dd 20/3aff 54/88,
			 12/86dd); 
	

  	c[0] 	-> nda
		-> Queue(1024)
		-> ToDevice(eth0);
	
	c[1]    -> [1]nds;
  	c[2] 	//-> Print(ok, 200)
		-> Strip(14)
		-> CheckIP6Header(3ffe:1ce1:2:0:200::ffff 3ffe:1ce1:2::ffff)
		-> GetIP6Address(24)
		-> rt :: LookupIP6Route(
			3ffe:1ce1:2::2/128 ::0 0,
			3ffe:1ce1:2:0:200::1/128 ::0 0,
			3ffe:1ce1:2::/80 ::0 1,
			3ffe:1ce1:2:0:200::/80 ::0 1,
			0::ffff:0:0/96 ::0 2,
  			::0/0 ::c0a8:1 3);
	
	rt[1] 	-> dh:: DecIP6HLIM
		-> [0]nds;
	rt[0] 	-> Print(route0-ok, 200) -> Discard;
	rt[2] 	-> Print(route2-ok, 200) -> Discard;
	rt[3] 	-> Print(route3-ok, 200) -> Discard;
	
	dh[1]	-> ICMP6Error(3ffe:1ce1:2:0:200::1, 3, 0)
		-> [0]nds;
	
	nds[0]	//-> Print(nds0-ok, 200)
		-> ToDevice(eth0);

	

	
	
