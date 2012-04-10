// -*- c-basic-offset: 4 -*-
/*
 * hashtabletest.{cc,hh} -- regression test element for HashTable<K, V>
 * Eddie Kohler
 *
 * Copyright (c) 2008 Meraki, Inc.
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
#include "hashtabletest.hh"
#include <click/hashtable.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# include <sys/time.h>
# include <sys/resource.h>
# include <unistd.h>
#endif
CLICK_DECLS

HashTableTest::HashTableTest()
{
}

#if 0
# define MAP_S2I HashMap<String, int>
# define MAP_VALUE(m, k) (m).find((k))
# define IT_VALUE(it) (it).value()
# define MAP_INSERT(m, k, v) (m).insert((k), (v))
# define MAP_FIND_VALUE(m, k) (m).find((k))
#elif 1
# define MAP_S2I HashTable<String, int>
# define MAP_VALUE(m, k) (m)[(k)]
# define IT_VALUE(it) (it)->second
# define MAP_INSERT(m, k, v) (m).set((k), (v))
# define MAP_FIND_VALUE(m, k) ({ MAP_S2I::iterator it = (m).find((k)); it ? it->second : 0; })
#else
# define MAP_S2I HashTable<Pair<String, int> >
# define MAP_VALUE(m, k) (m).find((k))->second
# define IT_VALUE(it) (it)->second
# define MAP_INSERT(m, k, v) (m).set(make_pair((k), (v)))
# define MAP_FIND_VALUE(m, k) ({ MAP_S2I::iterator it = (m).find((k)); it ? it->second : 0; })
#endif

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test `%s' failed", __FILE__, __LINE__, #x);
#define CHECK_DATA(x, y, l) CHECK(memcmp((x), (y), (l)) == 0)

static int
check1(MAP_S2I &h, ErrorHandler *errh)
{
    CHECK(h.size() == 4);
    CHECK(!h.empty());

    char x[4] = "\0\0\0";
    int n = 0;
    for (MAP_S2I::const_iterator i = h.begin(); i.live(); i++) {
	CHECK(IT_VALUE(i) >= 1 && IT_VALUE(i) <= 4);
	CHECK(x[IT_VALUE(i) - 1] == 0);
	x[IT_VALUE(i) - 1] = 1;
	n++;
    }
    CHECK(n == 4);

    memset(x, 0, 4);
    n = 0;
    for (MAP_S2I::iterator i = h.begin(); i.live(); i++) {
	int oldv = IT_VALUE(i);
	CHECK(IT_VALUE(i) >= 1 && IT_VALUE(i) <= 4);
	IT_VALUE(i) = 5;
	CHECK(IT_VALUE(i) == 5);
	IT_VALUE(i) = oldv;
	CHECK(x[IT_VALUE(i) - 1] == 0);
	x[IT_VALUE(i) - 1] = 1;
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
"HashTableTest",
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

struct MyHashContainerEntry {
    int _key;
    struct MyHashContainerEntry *_hashnext;

    typedef int key_type;
    typedef int key_const_reference;

    key_const_reference hashkey() const {
	return _key;
    }

    MyHashContainerEntry(int key) : _key(key), _hashnext(0) {};
};

typedef HashContainer<MyHashContainerEntry> MyHashContainer;

int
HashTableTest::initialize(ErrorHandler *errh)
{
    MyHashContainer my_hashcontainer;
    SizedHashAllocator<sizeof(MyHashContainerEntry)> my_alloc;
    int my_num_to_insert = 1000;
    for (int i = 0; i < my_num_to_insert; ++i) {
	void *p = my_alloc.allocate();
	MyHashContainerEntry *e = new(p) MyHashContainerEntry(i);
	MyHashContainer::iterator insert_it = my_hashcontainer.find(i);
	CHECK(!insert_it.get());
	my_hashcontainer.insert_at(insert_it, e);
	my_hashcontainer.balance();
    }
    CHECK(my_hashcontainer.size() == 1000);
    for (MyHashContainer::iterator it = my_hashcontainer.begin(); it.live();) {
	MyHashContainerEntry *e = it.get();
	my_hashcontainer.erase(it);
	e->~MyHashContainerEntry();
	my_alloc.deallocate(e);
    }
    CHECK(my_hashcontainer.size() == 0);

    MAP_S2I h;

    MAP_INSERT(h, "Foo", 1);
    MAP_INSERT(h, "bar", 2);
    MAP_INSERT(h, "facker", 3);
    MAP_INSERT(h, "Anne Elizabeth Dudfield", 4);

    CHECK(check1(h, errh) == 0);

    // check copy constructor
    {
	MAP_S2I hh(h);
	CHECK(check1(hh, errh) == 0);
	MAP_INSERT(hh, "crap", 5);
    }

    CHECK(check1(h, errh) == 0);

    h.erase("Foo");
    h.erase("Anne Fuckfuckabeth Fuckfield");

    CHECK(h.size() == 3);
    CHECK(MAP_VALUE(h, "bar") == 2);
    CHECK(MAP_VALUE(h, "facker") == 3);
    CHECK(MAP_VALUE(h, "Anne Elizabeth Dudfield") == 4);

    MAP_S2I hh;
    h.clear();
    h["Crap"] = 1;
    h["Crud"] = 2;
    h["Crang"] = 3;
    h["Dumb"] = 3;
    for (MAP_S2I::iterator it = h.begin(); it; )
	if (it.key() == "Crud")
	    it = h.erase(it);
	else {
	    hh[it.key()] = it.value();
	    ++it;
	}
    CHECK(hh["Crap"] == 1);
    CHECK(hh["Crang"] == 3);
    CHECK(hh["Dumb"] == 3);
    CHECK(h.find("Crud") == h.end());
    CHECK(hh.find("Crud") == hh.end());

#if CLICK_USERLEVEL
    MAP_S2I map;

    struct rusage ru0, ru1;
    Timestamp ts0, ts1;
    if (getrusage(RUSAGE_SELF, &ru0) < 0)
	return errh->error("rusage: %s", strerror(errno));
    ts0.assign_now();

    for (int i = 0; i < 100; i++) {
	map.clear();
	for (const char * const *s = classes; *s; s++)
	    MAP_INSERT(map, *s, s - classes);
	for (const char * const *s = classes; *s; s++)
	    MAP_INSERT(map, *s + 1, s - classes + 1000);
	for (const char * const *s = classes; *s; s++)
	    MAP_INSERT(map, *s + 2, s - classes + 2000);
	int value = 0;
	for (const char * const *s = classes; *s; s++)
	    value += MAP_FIND_VALUE(map, *s);
	for (const char * const *s = classes; *s; s++)
	    value += MAP_FIND_VALUE(map, *s + 1);
	for (const char * const *s = classes; *s; s++)
	    value += MAP_FIND_VALUE(map, *s + 2);
	for (const char * const *s = nonclasses; *s; s++)
	    value += MAP_FIND_VALUE(map, *s);
    }

    if (getrusage(RUSAGE_SELF, &ru1) < 0)
	return errh->error("rusage: %s", strerror(errno));
    ts1.assign_now();

    Timestamp ru_delta = Timestamp(ru1.ru_utime) - Timestamp(ru0.ru_utime);
    ts1 -= ts0;
    errh->message("Time: %p{timestamp}u %p{timestamp} total %u/%u", &ru_delta, &ts1, map.size(), map.bucket_count());
#endif

    {
	char blah[] = "Hello, this is a story I will tell.\0\0\0\0";
	size_t l = strlen(blah);
	hashcode_t a = String::hashcode(blah, blah + l);
	memmove(blah + 1, blah, l);
	hashcode_t b = String::hashcode(blah + 1, blah + l + 1);
	memmove(blah + 2, blah + 1, l);
	hashcode_t c = String::hashcode(blah + 2, blah + l + 2);
	memmove(blah + 3, blah + 2, l);
	hashcode_t d = String::hashcode(blah + 3, blah + l + 3);
	CHECK(a == b);
	CHECK(a == c);
	CHECK(a == d);
    }

    {
	HashTable<String, int> htx;
	htx["Hello"] = 1;
	if (!htx["Goodbye"])
	    htx["Goodbye"] = 2;
	CHECK(htx["NOT IN TABLE"] == 0);
	CHECK(htx["Hello"] == 1);
	CHECK(htx["Goodbye"] == 2);
    }

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(HashTableTest)
CLICK_ENDDECLS
