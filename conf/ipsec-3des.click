
RatedSource(\<0000000011111111222222223333333344444444555555556666>,
            1,1,1)
	-> IPEncap(17, 4.0.0.2, 1.0.0.2) 
	-> SetIPChecksum
	-> CheckIPHeader(18.26.4.255 2.255.255.255 1.255.255.255)
        -> Print(crypt,128)

	-> IPsecESPEncap(0x00000001)
	-> IPsecAuthSHA1(0)
        -> Print(auth0,128)

        -> IPsecDES(1, 0123456789abcdef)
        -> IPsecDES(0, a12345ddd9abcdef)
        -> IPsecDES(2, A12345ccc9abcdef)

        -> IPEncap(50, 4.0.0.10, 1.0.0.2)
        -> SetIPChecksum
	-> EtherEncap(0x0800, 0:0:0:0:0:0, 1:1:1:1:1:1)
	-> Strip(14)
        -> Strip(20)
	-> MarkIPHeader

        -> IPsecDES(0, A12345ccc9abcdef)
        -> IPsecDES(2, a12345ddd9abcdef)
        -> IPsecDES(0, 0123456789abcdef)

        -> Print(auth0, 128)
	-> IPsecAuthSHA1(1)
        -> IPsecESPUnencap()
        -> Print(crypt,128)

	-> Discard;
  
