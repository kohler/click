
//Suppose grunt ping6 frenulum
//It is first sending its N.solicitation message to local link for mindwipe's link layer address since mindwipe is it's default route. mindwipe respond it with N.Advertisement message. grunt send echo Request dst to frenulum to mindwipe, mindwipe route it to frenulum (it first send N. solicitation message on the local link, frenulum respondes it with N. Advertisement message, then mindwipe send the packet to frenulum's with encapsulated ethernet header). If frenulum knows grunts' link layer address, it replies with a echo reply message. 

//We could set mindwipe as the router for frenulum if des is grunt. Then we have the exact process of the above from frenulum to grunt.

//grunt - ip6 address is : 3ffe:1ce1:2:0:200::2
//frenulum -ip6 address is 3ffe:1ce1:2::1
//mindwipe - ip6 addresses are : 3ffe:1ce1:2:0:200::1 and 3ffe:1ce1:2::2 
//mindwipe is our click router, the above two IP6 address is by imagination, not real, i.e. the router itself sees itself have these two ip6 address.

//Type the following for the test:

//mindwipe: in click/userlevel directory:
//mindwipe: ./click ../conf/ip601.click

//grunt: route flush -inet6
//grunt: route add -inet6 3ffe:1ce1:2::1 3ffe:1ce1:2:0:200::1
//grunt: ping6 3ffe:1ce1:2::1




FromDevice(eth0, 1)
  	-> c::Classifier(12/86dd 20/3aff 54/87,
		         12/86dd 20/3aff 54/88,
			 12/86dd); 
	nda::NDAdv(3ffe:1ce1:2:0:200::1/128 00:A0:C9:9C:FD:9E, 
				fe80::2a0:c9ff:fe9c:fd9e/128 00:A0:C9:9C:FD:9E); 
	c[0] ->Print(Rec:N.Solicitation, 200)
  	->nda
	->Print(Send:N.Adv, 200)
	->Queue(1024)
	->ToDevice(eth0);

  	c[2] -> Print(Rec:IP6-packet, 200) ->Strip(14)
	-> CheckIP6Header(3ffe:1ce1:2:0:200::ffff 3ffe:1ce1:2::ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	3ffe:1ce1:2::2/128 ::0 0,
	3ffe:1ce1:2:0:200::1/128 ::0 0,
	3ffe:1ce1:2::/80 ::0 1,
	3ffe:1ce1:2:0:200::/80 ::0 1,
	0::ffff:0:0/96 ::0 2,
  	::0/0 ::c0a8:1 3);
	nds :: NDSol(fe80::2a0:c9ff:fe9c:fd9e, 00:a0:c9:9c:fd:9e);
	
	rt[1] 	-> Print(route1-ok, 200) 
		-> [0]nds;
		
	c[1] 	-> Print(Rec:N.Adv, 200) ->[1]nds;
	nds[0]	-> Print(send:N.AdvOrIP6, 200)
		-> ToDevice(eth0);

	

	rt[0] 	-> Print(route0-ok, 200) -> Discard;
	rt[2] 	-> Print(route2-ok, 200) -> Discard;
	rt[3] 	-> Print(route3-ok, 200) -> Discard;
	
