// zwolle <mobile> -> redlab <fixed>

fi :: Classifier(12/0800 26/121a0416 30/121a040a,
             12/0800 26/121a040a 30/121a0416,
             -)

out :: Queue(20)

out[0] -> ToDump(/tmp/out)

snp :: SnoopTCP;

snp[2] -> out;

FromDump(/tmp/dmp) -> fi


// Packets to Mobile host

fi[1]   -> Strip(14)
	-> [0]snp[0]
	-> EtherEncap(0x0800, 00:00:c0:75:70:ef,00:00:c0:c7:71:ef)
        -> out;

// Packets from Mobile host

fi[0]	-> Strip(14)
	-> [1]snp[1]
	-> EtherEncap(0x0800, 00:00:c0:c7:71:ef,00:00:c0:75:70:ef)
        -> out;

fi[2] -> Discard;
