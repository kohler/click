
InfiniteSource(DATA \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
45 00 00 28  00 00 00 00  40 11 77 c3  01 00 00 01
02 00 00 02  13 69 13 69  00 14 d6 41  55 44 50 20
70 61 63 6b  65 74 21 0a>, LIMIT 2, STOP true)
	-> Strip(14)
	-> CheckIPHeader(18.26.4.255 2.255.255.255 1.255.255.255)
        -> Print(encrypt0)
        
	-> IPsecESPEncap(0x00000001, 8, true)
        -> IPsecDES(1, FFFFFFFFFFFFFFFF, 0123456789abcdef)
        -> Print(encrypt1)
        -> IPsecDES(0, FFFFFFFFFFFFFFFF, 9876543210fedcba)
        -> Print(encrypt2)
        -> IPsecDES(2, FFFFFFFFFFFFFFFF, 0123456789ABCDEF)
        -> Print(encrypt3)

        -> IPEncap(50, 1.0.0.2, 2.0.0.2)
        -> SetIPChecksum
        -> Strip(20)
	-> MarkIPHeader

        -> Print(decrypt3)
        -> IPsecDES(0, FFFFFFFFFFFFFFFF, 0123456789ABCDEF)
        -> Print(decrypt2)
        -> IPsecDES(2, FFFFFFFFFFFFFFFF, 9876543210fedcba)
        -> Print(decrypt1)
        -> IPsecDES(0, FFFFFFFFFFFFFFFF, 0123456789abcdef)

        -> IPsecESPUnencap(true)
	-> MarkIPHeader
        -> Print(decrypt0)
	-> Discard;
  
