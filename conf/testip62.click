//FromDevice(eth0, 1)
//The following infinate source is faking a Neighborhood Solitation Message.

InfiniteSource(\<3333ff00 00050000 c04371ef 86dd
6000 00000020 3aff
3ffe 1ce10002 00000200 c0fffe43 71ef
ff02 00000000 00000000 0001ff00 0005
8700 5a6c0000 0000
3ffe 1ce10002 00000000 00000000 0005
0101 0000c043 71ef>, 1, 5)
	
  	-> c::Classifier(12/86dd);
	arpr::ARPResponder6(3ffe:1ce1:2::5 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff 00:e0:29:05:e5:6f); 
	c[0] ->Print(afterClassfier, 200)
  	->arpr
	->Print(afterARPResponder6, 200)
  	-> Discard;