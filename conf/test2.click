// test2.click

// This test file contains both push and pull processing and Random
// Early Detection dropping (although no packets are dropped in this
// configuration).

InfiniteSource(DATA \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
45 00 00 28  00 00 00 00  40 11 77 c3  01 00 00 01  
02 00 00 02  13 69 13 69  00 14 d6 41  55 44 50 20  
70 61 63 6b  65 74 21 0a  04 00 00 00  01 00 00 00  
01 00 00 00  00 00 00 00  00 80 04 08  00 80 04 08  
53 53 00 00  53 53 00 00  05 00 00 00  00 10 00 00  
01 00 00 00  54 53 00 00  54 e3 04 08  54 e3 04 08  
d8 01 00 00>, LIMIT 600000, STOP true)
	-> Strip(14)
	-> CheckIPHeader(BADSRC 18.26.4.255 2.255.255.255 1.255.255.255)
	-> RED(10, 100, .5)
	-> Queue
	-> Discard;
