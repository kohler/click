
// This test configuration assume the machine that click runs 
// (in this case mindwipe) has ip6 address 3ffe:1ce1:2::5.  
// It will reply a neighborhood solitation message.  
// For instance "ping6 3ffe:1ce1:2::5" 
// It will print out the following:
// Print afterClassfier |  86 : 3333ff00 00050000 c04371ef 86dd6000 00000020 3aff3ffe 1ce10002 00000200 00000000 0002ff02 00000000 00000000 0001ff00 00058700 8b9d0000 00003ffe 1ce10002 00000000 00000000 00050101 0000c043 71ef

// Print after-NDAdv |  86 : 0000c043 71ef00a0 c99cfd9e 86dd6000 00000020 3aff3ffe 1ce10002 00000000 00000000 00053ffe 1ce10002 00000200 00000000 00028800 55184000 00003ffe 1ce10002 00000000 00000000 00050201 00a0c99c fd9e


	FromDevice(eth0, 1)
  	-> c::Classifier(12/86dd 54/87);
	nda::NDAdv(3ffe:1ce1:2::5/128 00:a0:c9:9c:fd:9e); 
	c[0] ->Print(afterClassfier, 200)
  	->nda
	->Print(after-NDAdv, 200)
	->Queue(1024)
	->ToDevice(eth0);
  	