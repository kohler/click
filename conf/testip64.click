
// This test configuration assume the machine that click runs 
// (in this case darkstar) has ip6 address 3ffe:1ce1:2::5.  
// It will reply a neighborhood solitation message.  
// For instance "ping6 3ffe:1ce1:2::5" 

FromDevice(eth0, 1)
  	-> c::Classifier(12/86dd);
	arpr::ARPResponder6(3ffe:1ce1:2::5 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff 00:e0:29:05:e5:6f); 
	c[0] ->Print(afterClassfier, 200)
  	->arpr
	->Print(afterARPResponder6, 200)
	->Queue(1024)
	->ToDevice(eth0);
  	