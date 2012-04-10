// -*- c-basic-offset: 4 -*-
/*
 * bhmtest.{cc,hh} -- regression test element for BigHashMap
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "bhmtest.hh"
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# include <sys/time.h>
# include <sys/resource.h>
# include <unistd.h>
#endif
CLICK_DECLS

BigHashMapTest::BigHashMapTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);
#define CHECK_DATA(x, y, l) CHECK(memcmp((x), (y), (l)) == 0)

static int
check1(HashMap<String, int> &h, ErrorHandler *errh)
{
    CHECK(h.size() == 4);
    CHECK(!h.empty());

    char x[4] = "\0\0\0";
    int n = 0;
    for (HashMap<String, int>::const_iterator i = h.begin(); i.live(); i++) {
	CHECK(i.value() >= 1 && i.value() <= 4);
	CHECK(x[i.value() - 1] == 0);
	x[i.value() - 1] = 1;
	n++;
    }
    CHECK(n == 4);

    memset(x, 0, 4);
    n = 0;
    for (HashMap<String, int>::iterator i = h.begin(); i.live(); i++) {
	int oldv = i.value();
	CHECK(i.value() >= 1 && i.value() <= 4);
	i.value() = 5;
	CHECK(i.value() == 5);
	i.value() = oldv;
	CHECK(x[i.value() - 1] == 0);
	x[i.value() - 1] = 1;
	n++;
    }
    CHECK(n == 4);

    return 0;
}

#if CLICK_USERLEVEL
static const char * const classes[] = {
"ARPFaker",
"ARPQuerier",
"ARPResponder",
"AdaptiveRED",
"AddressInfo",
"AddressTranslator",
"AggregateCounter",
"AggregateFilter",
"AggregateFirst",
"AggregateIP",
"AggregateIPFlows",
"AggregateLast",
"AggregateLength",
"Align",
"AlignmentInfo",
"AnonymizeIPAddr",
"AverageCounter",
"B8B10",
"BIM",
"BandwidthMeter",
"BandwidthRatedSplitter",
"BandwidthRatedUnqueue",
"BandwidthShaper",
"BigHashMapTest",
"Block",
"BufferConverter",
"Burster",
"CPUQueue",
"CPUSwitch",
"ChatterSocket",
"CheckCRC32",
"CheckICMPHeader",
"CheckIP6Header",
"CheckIPHeader",
"CheckIPHeader2",
"CheckLength",
"CheckPacket",
"CheckPaint",
"CheckPattern",
"CheckTCPHeader",
"CheckUDPHeader",
"Classifier",
"CompareBlock",
"ControlSocket",
"CopyFlowID",
"CopyTCPSeq",
"Counter",
"CycleCountAccum",
"DRRSched",
"DebugBridge",
"DecIP6HLIM",
"DecIPTTL",
"DelayShaper",
"DelayUnqueue",
"DevirtualizeInfo",
"Discard",
"DiscardNoFree",
"DriverManager",
"DropBroadcasts",
"DupPath",
"DynamicUDPIPEncap",
"EnsureEther",
"Error",
"EtherEncap",
"EtherMirror",
"EtherSpanTree",
"EtherSwitch",
"FTPPortMapper",
"FastTCPFlows",
"FastUDPFlows",
"FastUDPSource",
"FastUDPSourceIP6",
"FixIPSrc",
"ForceICMP",
"ForceIP",
"ForceTCP",
"ForceUDP",
"FromDAGDump",
"FromDevice",
"FromDevice",
"FromDump",
"FromHost",
"FromIPSummaryDump",
"FromNetFlowSummaryDump",
"FrontDropQueue",
"GetIP6Address",
"GetIPAddress",
"HashSwitch",
"HostEtherFilter",
"ICMP6Error",
"ICMPError",
"ICMPPingEncap",
"ICMPPingResponder",
"ICMPPingRewriter",
"ICMPPingSource",
"ICMPRewriter",
"ICMPSendPings",
"IP6Fragmenter",
"IP6Mirror",
"IP6NDAdvertiser",
"IP6NDSolicitor",
"IP6Print",
"IPAddrRewriter",
"IPClassifier",
"IPEncap",
"IPFilter",
"IPFragmenter",
"IPGWOptions",
"IPInputCombo",
"IPMirror",
"IPOutputCombo",
"IPPrint",
"IPRateMonitor",
"IPReassembler",
"IPRewriter",
"IPRewriterPatterns",
"IPsecAuthSHA1",
"IPsecDES",
"IPsecESPEncap",
"IPsecESPUnencap",
"Idle",
"InfiniteSource",
"KernelHandlerProxy",
"KernelTap",
"KernelTun",
"LinearIPLookup",
"LinuxIPLookup",
"LookupIP6Route",
"LookupIPRoute2",
"LookupIPRouteMP",
"MSQueue",
"MarkIP6Header",
"MarkIPCE",
"MarkIPHeader",
"Meter",
"MixedQueue",
"NotifierQueue",
"Null",
"Null1",
"Null2",
"Null3",
"Null4",
"Null5",
"Null6",
"Null7",
"Null8",
"PacketTest",
"Paint",
"PaintSwitch",
"PaintTee",
"Pct",
"PerfCountAccum",
"PerfCountInfo",
"PokeHandlers",
"PollDevice",
"Print",
"Print80211",
"PrintAiro",
"PrintOld",
"PrioSched",
"ProgressBar",
"ProtocolTranslator46",
"ProtocolTranslator64",
"PullNull",
"PullSwitch",
"PullTee",
"PushNull",
"Queue",
"QueueYankTest",
"QuitWatcher",
"RED",
"RFC2507Comp",
"RFC2507Decomp",
"RIPSend",
"RadixIPLookup",
"RandomBitErrors",
"RandomSample",
"RandomSource",
"RandomSwitch",
"RatedSource",
"RatedSplitter",
"RatedUnqueue",
"RoundRobinIPMapper",
"RoundRobinSched",
"RoundRobinSwitch",
"RoundRobinUnqueue",
"RoundTripCycleCount",
"ScheduleInfo",
"ScheduleLinux",
"Scramble",
"SendPattern",
"SerialLink",
"SetAnnoByte",
"SetCRC32",
"SetCycleCount",
"SetIP6Address",
"SetIP6DSCP",
"SetIPAddress",
"SetIPChecksum",
"SetIPDSCP",
"SetPacketType",
"SetPerfCount",
"SetRandIPAddress",
"SetTCPChecksum",
"SetTimestamp",
"SetUDPChecksum",
"Shaper",
"SimpleQueue",
"SortedIPLookup",
"SortedTaskSched",
"SpinlockAcquire",
"SpinlockInfo",
"SpinlockRelease",
"StaticIPLookup",
"StaticPullSwitch",
"StaticSwitch",
"StaticThreadSched",
"StoreIPAddress",
"StrideSched",
"StrideSwitch",
"Strip",
"StripIPHeader",
"StripToNetworkHeader",
"Suppressor",
"Switch",
"TCPAck",
"TCPBuffer",
"TCPConn",
"TCPDemux",
"TCPIPSend",
"TCPReflector",
"TCPRewriter",
"Tee",
"ThreadMonitor",
"TimeFilter",
"TimeRange",
"TimeSortedSched",
"TimedSink",
"TimedSource",
"TimestampAccum",
"ToDevice",
"ToDevice",
"ToDump",
"ToHost",
"ToHostSniffers",
"ToIPFlowDumps",
"ToIPSummaryDump",
"ToyTCP",
"TrieIPLookup",
"UDPIPEncap",
"Unqueue",
"Unqueue2",
"Unstrip",
"UnstripIPHeader",
"WebGen",
0
};

static const char * const nonclasses[] = {
"RA PFaker",
"ARQPuerier",
"ARPeRsponder",
"AdaptvieRED",
"AddresIsnfo",
"AddressrTanslator",
"AggregatCeounter",
"AggregateiFlter",
"gAgregateFirst",
"AggergateIP",
"AgrgegateIPFlows",
"AggrgeateLast",
"AggreagteLength",
"Aigln",
"AlingmentInfo",
"AnonmyizeIPAddr",
"AveraegCounter",
"B81B0",
"BMI",
"BadnwidthMeter",
"BanwdidthRatedSplitter",
"BandiwdthRatedUnqueue",
"BandwdtihShaper",
"BiHgashapTesMt",
"Blokc",
"Buffrenverter",
"Burste",
"CPUueue",
"CPSUwitch",
"ChaterSocket",
"ChecCRC32",
"ChcICMPHeader",
"Check6Header",
"heckIPHeader",
"heckIPHeader2",
"ChckLength",
"ChcPacket",
"CheckPant",
"CheckPasadttern",
"CheckTCPHfddeader",
"CheckUsDPsaHeader",
"Classqfdifier",
"ComperwareBlock",
"ContfrolSocket",
"CopyFasgdslowID",
"CopyTCsPSeq",
"Coun zter",
"CyclsefCountAccum",
"DRRSached",
"DebugsaBridge",
"DecIP6HdLfIM",
"DecIPTTsL",
"eDlayShaper",
"DleayUnqueue",
"DeivrtualizeInfo",
"Disacrd",
"cardNoFree",
"DrverManager",
"ropDBroadcasts",
"DupPh",
"DynmiacUDPIPEncap",
"ENSUREETHER",
"ERROR",
"ETHERENCAP",
"ETHERMIRROR",
"ETHERSPANTREE",
"ETHERSWITCH",
"FTPPortapMper",
"FastTCPFls",
"FastUDPFl",
"Fa",
"Fadd",
"FixI",
"ForecICMP",
"FocreIP",
0
};
#endif

int
BigHashMapTest::initialize(ErrorHandler *errh)
{
    HashMap<String, int> h(-1);

    h.insert("Foo", 1);
    h.insert("bar", 2);
    h.insert("facker", 3);
    h.insert("Anne Elizabeth Dudfield", 4);

    CHECK(check1(h, errh) == 0);

    // check copy constructor
    {
	HashMap<String, int> hh(h);
	CHECK(check1(hh, errh) == 0);
	hh.insert("crap", 5);
    }

    CHECK(check1(h, errh) == 0);

#if CLICK_USERLEVEL
    HashMap<String, int> map(-1);

    struct rusage ru0, ru1;
    Timestamp ts0, ts1;
    if (getrusage(RUSAGE_SELF, &ru0) < 0)
	return errh->error("rusage: %s", strerror(errno));
    ts0.assign_now();

    for (int i = 0; i < 100; i++) {
	map.clear();
	for (const char * const *s = classes; *s; s++)
	    map.insert(*s, s - classes);
	int value = 0;
	for (const char * const *s = classes; *s; s++)
	    value += map.find(*s);
	for (const char * const *s = nonclasses; *s; s++)
	    value += map.find(*s);
    }

    if (getrusage(RUSAGE_SELF, &ru1) < 0)
	return errh->error("rusage: %s", strerror(errno));
    ts1.assign_now();

    Timestamp ru_delta = Timestamp(ru1.ru_utime) - Timestamp(ru0.ru_utime);
    ts1 -= ts0;
    errh->message("%p{timestamp}u %p{timestamp} total", &ru_delta, &ts1);
#endif

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(BigHashMapTest)
CLICK_ENDDECLS
