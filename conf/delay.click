
RatedSource(DATA \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
45 00 00 28  00 00 00 00  40 11 77 c3  01 00 00 01
02 00 00 02  13 69 13 69  00 14 d6 41  55 44 50 20
70 61 63 6b  65 74 21 0a>, LIMIT 5, STOP false)
	-> SetTimestamp
	-> Print(yy)
	-> Queue
	-> DelayUnqueue(2000)
	-> Print(xx)
	-> Discard;
