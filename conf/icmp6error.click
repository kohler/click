//This configuration file is to test the icm6error element.

//To run the test with another machine, you can change  the source address in InfiniteSource (i.e.3ffe:1ce1:2:0:200::2) to the ip6 machine that you want to send the packet to.  Also, change the (IP6ADDR ETHADD) pair of IP6NDSolicitor to your click router machines' setting.  
//You can use tcpdump on the other ip6 machine to check if it actually acknolowdges the packet.

//Or you can simply change the last line "ToDevice(eth0)" to "Discard" to run the test on local machine.
//-> ToDevice(eth0);


InfiniteSource( \<00 00 c0 ae 67 ef 00 90 27 e0 23 1f 86 dd
60 00 00 00  00 50 11 01  3f fe	1c e1  00 02 00 00  
02 00 00 00  00 00 00 02  00 00 00 00  00 00 00 00  
00 00 00 00  12 61 02 7d  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	
  	-> c::Classifier(12/86dd 54/88,
			 12/86dd);

   c[1] -> Print(Rec-IP6packet, 200) 
	-> Strip(14)
	-> CheckIP6Header(3ffe:1ce1:2:0:200::ffff)
	-> Print(before-ICMP6Error, 200)
	-> dh:: DecIP6HLIM
	-> Print(after-dh-0, 200)
	-> Discard;
	
	nds :: IP6NDSolicitor(fe80::2a0:c9ff:fe9c:fd9e, 00:a0:c9:9c:fd:9e);
	
	
   dh[1]-> ICMP6Error(3ffe:1ce1:2:0:200::1, 3, 0)
	-> Print(after-ICMP6Error, 200)
	-> [0]nds;
	
    c[0]-> Print(Rec:N.Adv, 200) ->[1]nds;

 nds[0]	-> Print(send:N.AdvOrIP6, 200)
	-> Discard;
	
	
	
	
