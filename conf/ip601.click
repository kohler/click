
//Suppose grunt ping frenulum
//It is first sending its N.solicitation message to local link for mindwipe's link layer address since mindwipe is it's default route. mindwipe respond it with N.Advertisement message. Grunt send echo Request dst to frenulum to mindwipe, mindwipe route it to frenulum (it first send N. solicitation message on the local link, frenulum respondes it with N. Advertisement message, then mindwipe send the packet to frenulum's with encapsulated ethernet header).


FromDevice(eth0, 1)
  	-> c::Classifier(12/86dd 20/3aff 54/87,
		         12/86dd 20/3aff 54/88,
			 12/86dd); 
	arpr::ARPResponder6(3ffe:1ce1:2:0:200::1 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff 00:A0:C9:9C:FD:9E); 
	c[0] ->Print(afterClassfier, 200)
  	->arpr
	->Print(afterARPResponder6, 200)
	->Queue(1024)
	->ToDevice(eth0);

  	c[2] -> Print(c[2], 200) ->Strip(14)
	-> CheckIP6Header(3ffe:1ce1:2:0:200::ffff 3ffe:1ce1:2::ffff)
	-> GetIP6Address(24)
	-> rt :: LookupIP6Route(
	3ffe:1ce1:2::1 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
	3ffe:1ce1:2:0:200::1 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff ::0 0,
	3ffe:1ce1:2:: ffff:ffff:ffff:ffff:ffff:: ::0 1,
	3ffe:1ce1:2:0:200:: ffff:ffff:ffff:ffff:ffff:: ::0 2,
	0::ffff:0:0 ffff:ffff:ffff:ffff:ffff:ffff:: ::0 3,
  	::0 ::0 ::c0a8:1 4);
	arpq :: ARPQuerier6(fe80::2a0:c9ff:fe9c:fd9e, 00:a0:c9:9c:fd:9e);
	
	rt[1] 	-> Print(route1-ok, 200) 
		-> [0]arpq;
		
	c[1] 	-> Print(c[1], 200) ->[1]arpq;
	arpq[0]-> Print(afterARPQuerier6-output0, 200)
	
	
		-> ToDevice(eth0);
	rt[0] 	-> Print(route1-ok, 200) -> Discard;
	rt[2] 	-> Print(route2-ok, 200) -> Discard;
	rt[3] 	-> Print(route3-ok, 200) -> Discard;
	rt[4] 	-> Print(route4-ok, 200) -> Discard;
