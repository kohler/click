// testip6-linux.click

//  This configuration should print this line at the end:
//  Print ok | 120 : 60000000 00501140 00000000 00000000 00000000 c0a80001
//  You can run it at user level as
// ` [userlevel]#./click ..conf/testip6.click'


InfiniteSource( \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
60 00 00 00  00 50 11 40  00 00 00 00  00 00 00 00  
00 00 00 00  c0 a8 00 01  00 00 00 00  00 00 00 00  
00 00 00 00  12 61 02 7d  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00  13 69 13 69>, 1, 5)
	-> Strip(14)
	-> CheckIP6Header(::c0a8:ffff)
	-> Print(ok, 3) 
	-> Discard;
	