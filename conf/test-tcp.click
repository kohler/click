
RatedSource(\<000000001111111122222222333333334444444455555555666666667777>,
            10,5,1)
	-> Unqueue(2)
	-> IPEncap(6, 4.0.0.2, 1.0.0.2) 
	-> ForceTCP(-1, true, 8)
	-> SetTCPChecksum
	-> SetIPChecksum
        -> IPPrint(b)
	-> tb :: TCPBuffer(false);

tb -> IPPrint(c) -> Discard;

