// test.click

// This configuration should print this line five times:
// ok:   40 | 45000028 00000000 401177c3 01000001 02000002 13691369

// Run it at user level with
// 'click test.click'

// Run it in the Linux kernel with
// 'click-install test.click'
// Messages are printed to the system log (run 'dmesg' to see them, or look
// in /var/log/messages), and to the file '/click/messages'.

InfiniteSource(DATA \<00 00 c0 ae 67 ef  00 00 00 00 00 00  08 00
45 00 00 28  00 00 00 00  40 11 77 c3  01 00 00 01
02 00 00 02  13 69 13 69  00 14 d6 41  55 44 50 20
70 61 63 6b  65 74 21 0a>, LIMIT 5, STOP true)
	-> Strip(14)
	-> Align(4, 0)    // in case we're not on x86
	-> CheckIPHeader(BADSRC 18.26.4.255 2.255.255.255 1.255.255.255)
        -> Print(ok)
	-> Discard;
