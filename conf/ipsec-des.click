
InfiniteSource(DATA \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
45 00 00 28  00 00 00 00  40 11 77 c3  01 00 00 01
02 00 00 02  13 69 13 69  00 14 d6 41  55 44 50 20
70 61 63 6b  65 74 21 0a>, LIMIT 1, STOP true)
	-> Strip(14)
	-> CheckIPHeader(18.26.4.255 2.255.255.255 1.255.255.255)
        -> IPPrint(start)
	-> IPsecESPEncap(0x00000001, 8, true)
        -> IPsecDES(1, FFFFFFFFFFFFFFFF, 0123456789abcdef)
	-> MarkIPHeader
        -> IPPrint(encrypt)
        -> IPEncap(50, 1.0.0.2, 2.0.0.2)
        -> SetIPChecksum
	-> IPPrint(encap)
        -> Strip(20)
	-> MarkIPHeader
        -> IPPrint(unencap)
        -> IPsecDES(0, FFFFFFFFFFFFFFFF, 0123456789abcdef)
        -> IPsecESPUnencap(true)
	-> MarkIPHeader
        -> IPPrint(decrypt)
	-> Discard;
  
