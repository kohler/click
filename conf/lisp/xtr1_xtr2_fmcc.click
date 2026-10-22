// xtr1_xtr2_fmcc.click -- LISP + FMCC demo: two xTRs, a Map-Server and the
// FMCC controller in one Click process; migrates VM 192.168.122.161.
// Franck Messaoudi
//
// Copyright (c) 2014-2026 Franck Messaoudi
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, subject to the conditions
// listed in the Click LICENSE file. These conditions include: you must
// preserve this copyright notice, and you cannot mention the copyright
// holders in advertising related to the Software without their permission.
// The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
// notice is a summary of the Click LICENSE file; the license in that file is
// legally binding.

// Two xTRs, a Map-Server and the FMCC in one Click process (demo).

FD :: FromDevice(eth0, PROMISC true);

C :: Classifier( 12/0806 20/0001,
                 12/0806 20/0002,
                 12/0800,
                 -);

LC :: LISPClassifierxTR();

FD -> C;

TD :: ToDevice(eth0);

OUT :: Queue(10000) -> TD;
C[0] -> Discard;
C[1] -> OUT;


/********************************** XTR1 ******************************************/
IC :: LISPMapCache;
EB1 :: LISPEtrDatabase(192.168.122.239, 192.168.122.10, 192.168.122.20, 192.168.122.30, 192.168.122.161);
RGSTR1 :: LISPMapRegistrar(EB1, UDP, 192.168.122.239, 192.168.122.108);
RGSTR2 :: LISPMapRegistrar(EB1, UDP, 192.168.122.239, 192.168.122.153);
TR :: LISPVMCounter(192.168.122.210, 192.168.122.20, 192.168.122.30, 192.168.122.161);
UPDTR1 :: LISPMigrationUpdater(192.168.122.239, EB1, TR, RGSTR1); 
RQSTSend :: LISPMapRequester(IC, UDP, 192.168.122.239, 192.168.122.153);


/********************************** XTR2 ******************************************/
EB2 :: LISPEtrDatabase(192.168.122.153, 192.168.122.50, 192.168.122.60, 192.168.122.70, 192.168.122.80);
RGSTR3 :: LISPMapRegistrar(EB2, UDP, 192.168.122.153, 192.168.122.108);
RGSTR4 :: LISPMapRegistrar(EB2, UDP, 192.168.122.153, 192.168.122.153);

UPDTR2 :: LISPMigrationUpdater(192.168.122.153, EB2, TR, RGSTR3); 
//RQSTRes2 :: LISPMapReplier(EB, UDP, 192.168.122.153, 192.168.122.161);
//RQSTRes2 :: LISPMapReplier(EB, UDP, 192.168.122.239, 192.168.122.153);
/********************************** FMCC ******************************************/
FMCCB :: FMCCDatabase;
migrator1 :: FMCCMigrator(FMCCB, UDP, 192.168.122.153, 192.168.122.239);
migrator2 :: FMCCMigrator(FMCCB, UDP, 192.168.122.153, 192.168.122.153);
manager1 :: FMCCManager(192.168.122.239, 192.168.122.153, 192.168.122.161, FMCCB, migrator1);
manager2 :: FMCCManager(192.168.122.239, 192.168.122.153, 192.168.122.161, FMCCB, migrator2);

/********************************** MS_MR ******************************************/

MB  :: LISPMappingDB;
//mapresolver::LISPMapResolver(MB, UDP, 192.168.122.108, 192.168.122.161); 
mapserver1  :: LISPMapServer(MB, UDP, 192.168.122.108, 192.168.122.239);
mapserver2  :: LISPMapServer(MB, UDP, 192.168.122.108, 192.168.122.239);

/******************************************************************************/


RGSTR1 -> mapserver1;
RGSTR2 -> manager1;

RGSTR3 -> mapserver2;
RGSTR4 -> manager2;

mapserver1 ->migrator1;
mapserver2 ->migrator2;

migrator1 ->UPDTR1;//Discard;//UPDTR1;
migrator2 ->UPDTR2;//Discard;//UPDTR2;

UPDTR1 -> Discard;

C[2] -> Strip(14) -> LC;
LC[0] -> [0]RQSTSend;
LC[1] -> RGSTR3;
LC[2] -> [1]RQSTSend;
LC[3] -> RGSTR1;
LC[4] ->RGSTR2;
LC[5] ->TR;
LC[6] ->RGSTR4;
LC[7] -> [2]RQSTSend;

/*****************************/
//LC[8] -> UPDTR1;
//C[3] -> UPDTR2;
C[3] -> Discard;
/*****************************/
UPDTR2 -> LC;
