
//The following infinite source is faking a Neighborhood Solitation Message.
//It should print out the following:

//Print afterClassfier |  86 : 0000c043 71ef0090 27e0231f 86dd6000 00000020 3aff3ffe 1ce10002 00000000 00000000 00023ffe 1ce10002 00000200 c0fffe43 71ef8700 af080000 00003ffe 1ce10002 00000200 c0fffe43 71ef0101 009027e0 231f

//Print after-IP6NDAdvertiser |  78 : 009027e0 231f00e0 2905e56f 86dd6000 00000018 3aff3ffe 1ce10002 00000200 c0fffe43 71ef3ffe 1ce10002 00000000 00000000 00028800 baa04000 00003ffe 1ce10002 00000200 c0fffe43 71ef



InfiniteSource(\<0000c043 71ef0090 27e0231f 86dd
6000 00000020 3aff
3ffe 1ce10002 00000000 00000000 0002
3ffe 1ce10002 00000200 c0fffe43 71ef
8700 af080000 0000
3ffe 1ce10002 00000200 c0fffe43 71ef
0101 009027e0 231f>, 1, 5)
	
  	-> c::Classifier(12/86dd 54/87);

nda :: IP6NDAdvertiser(3ffe:1ce1:0002:0000:0200:c0ff:fe43:71ef/128 00:e0:29:05:e5:6f); 
c[0] -> Print(afterClassfier, 200)
  	-> nda
	-> Print(after-IP6NDAdvertiser, 200)
  	-> Discard;
