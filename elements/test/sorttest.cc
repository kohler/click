// -*- c-basic-offset: 4 -*-
/*
 * sorttest.{cc,hh} -- regression test element for Vector
 * Eddie Kohler
 *
 * Copyright (c) 2008 Regents of the University of California
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
#include "sorttest.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#if CLICK_USERLEVEL
# include "elements/userlevel/fromfile.hh"
#endif
CLICK_DECLS

SortTest::SortTest()
{
}

SortTest::~SortTest()
{
}

static const char *classes[] = {
"ToyTCP",
"SetIPDSCP",
"DupPath",
"CheckIPHeader",
"Classifier",
"ARPFaker",
"ForceICMP",
"Error",
"RoundRobinUnqueue",
"CheckTCPHeader",
"IPAddrRewriter",
"ProtocolTranslator64",
"Pct",
"DecIP6HLIM",
"ProgressBar",
"EtherSpanTree",
"TCPIPSend",
"Paint",
"Tee",
"ICMPRewriter",
"AnonymizeIPAddr",
"SpinlockAcquire",
"AggregateFirst",
"Counter",
"Null1",
"IP6Fragmenter",
"IPMirror",
"PullTee",
"AggregateCounter",
"SortedIPLookup",
"AddressTranslator",
"ThreadMonitor",
"KernelTun",
"ChatterSocket",
"LookupIP6Route",
"Print80211",
"TimeFilter",
"SetUDPChecksum",
"PaintTee",
"TCPAck",
"RatedUnqueue",
"PrioSched",
"RandomSwitch",
"IPPrint",
"AggregateFilter",
"BandwidthRatedUnqueue",
"StrideSwitch",
"Unqueue2",
"IP6Mirror",
"IPsecAuthSHA1",
"Switch",
"CPUQueue",
"Unqueue",
"RatedSource",
"ICMPPingEncap",
"NotifierQueue",
"LinearIPLookup",
"AdaptiveRED",
"ICMPSendPings",
"SetIP6DSCP",
"DelayShaper",
"SetRandIPAddress",
"FromNetFlowSummaryDump",
"SetCRC32",
"HashSwitch",
"TimeRange",
"RandomSource",
"Meter",
"IP6Print",
"RFC2507Decomp",
"IPEncap",
"DecIPTTL",
"HostEtherFilter",
"ICMPPingRewriter",
"IPClassifier",
"DiscardNoFree",
"TCPReflector",
"CopyTCPSeq",
"CheckPacket",
"FastTCPFlows",
"PacketTest",
"PollDevice",
"RoundRobinSwitch",
"BandwidthShaper",
"Null5",
"SetAnnoByte",
"PullSwitch",
"GetIPAddress",
"IPRewriterPatterns",
"ScheduleLinux",
"Null2",
"GetIP6Address",
"Print",
"StaticThreadSched",
"Discard",
"FromHost",
"CompareBlock",
"TimedSource",
"CheckPaint",
"ICMPError",
"DelayUnqueue",
"KernelTap",
"ProtocolTranslator46",
"TimeSortedSched",
"ToIPFlowDumps",
"AlignmentInfo",
"EtherEncap",
"KernelHandlerProxy",
"BandwidthMeter",
"SetIPChecksum",
"DevirtualizeInfo",
"LinuxIPLookup",
"SendPattern",
"CheckCRC32",
"MarkIPCE",
"AggregateLast",
"BufferConverter",
"LookupIPRouteMP",
"Null8",
"IP6NDAdvertiser",
"FastUDPFlows",
"UDPIPEncap",
"BandwidthRatedSplitter",
"RIPSend",
"IPInputCombo",
"CheckPattern",
"IPReassembler",
"ToDevice",
"FromDevice",
"ForceTCP",
"ControlSocket",
"FromIPSummaryDump",
"PrintAiro",
"StrideSched",
"Null3",
"Idle",
"Queue",
"Null",
"PerfCountAccum",
"PullNull",
"CheckICMPHeader",
"CheckLength",
"RoundRobinSched",
"Strip",
"TCPRewriter",
"IPsecESPUnencap",
"FixIPSrc",
"ICMPPingSource",
"ForceUDP",
"QuitWatcher",
"Burster",
"IPFilter",
"UnstripIPHeader",
"ToHost",
"ICMPPingResponder",
"IPOutputCombo",
"FastUDPSourceIP6",
"ToHostSniffers",
"Unstrip",
"Null4",
"SimpleQueue",
"SetTCPChecksum",
"SetCycleCount",
"MarkIPHeader",
"IPsecDES",
"RadixIPLookup",
"IPFragmenter",
"Scramble",
"RandomSample",
"TimedSink",
"LookupIPRoute2",
"FastUDPSource",
"BigHashMapTest",
"RoundTripCycleCount",
"IPsecESPEncap",
"SpinlockRelease",
"IPGWOptions",
"StaticSwitch",
"SerialLink",
"Shaper",
"MSQueue",
"B8B10",
"DebugBridge",
"DropBroadcasts",
"SetTimestamp",
"RandomBitErrors",
"RED",
"AggregateIPFlows",
"FrontDropQueue",
"ToIPSummaryDump",
"CheckUDPHeader",
"PokeHandlers",
"ARPQuerier",
"SetIP6Address",
"Suppressor",
"CPUSwitch",
"SortedTaskSched",
"WebGen",
"StaticPullSwitch",
"QueueYankTest",
"SetIPAddress",
"CheckIP6Header",
"StoreIPAddress",
"ToDevice",
"BIM",
"Null6",
"PushNull",
"ICMP6Error",
"FromDump",
"ARPResponder",
"RFC2507Comp",
"FromDevice",
"ForceIP",
"TimestampAccum",
"AggregateIP",
"FTPPortMapper",
"DynamicUDPIPEncap",
"EtherMirror",
"IPRewriter",
"ScheduleInfo",
"TCPDemux",
"TCPConn",
"RatedSplitter",
"EnsureEther",
"SetPacketType",
"IPRateMonitor",
"DriverManager",
"RoundRobinIPMapper",
"AverageCounter",
"EtherSwitch",
"PrintOld",
"IP6NDSolicitor",
"MixedQueue",
"CycleCountAccum",
"PaintSwitch",
"TCPBuffer",
"ToDump",
"Null7",
"StaticIPLookup",
"FromDAGDump",
"StripToNetworkHeader",
"DRRSched",
"SetPerfCount",
"AddressInfo",
"InfiniteSource",
"StripIPHeader",
"MarkIP6Header",
"Align",
"CopyFlowID",
"PerfCountInfo",
"AggregateLength",
"TrieIPLookup",
"CheckIPHeader2",
"Block",
"SpinlockInfo"
};

static const char * const sorted_classes[] = {
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
"WebGen"
};

static Vector<String> *strvec;
static Vector<size_t> *sizevec;

static int compar(const void *xa, const void *xb)
{
    const char *a = * (const char **) xa;
    const char *b = * (const char **) xb;
    return strcmp(a, b);
}

static int string_compar(const void *xa, const void *xb)
{
    const String *a = (const String *) xa;
    const String *b = (const String *) xb;
    return String::compare(*a, *b);
}

static int size_t_compar(const void *xa, const void *xb)
{
    const size_t *a = (const size_t *) xa;
    const size_t *b = (const size_t *) xb;
    return (*a > *b ? 1 : (*a == *b ? 0 : -1));
}

static int string_permute_compar(const void *xa, const void *xb)
{
    const int *a = (const int *) xa;
    const int *b = (const int *) xb;
    int diff = String::compare((*strvec)[*a], (*strvec)[*b]);
    return (diff ? diff : *a - *b);
}

static int size_t_permute_compar(const void *xa, const void *xb)
{
    const int *a = (const int *) xa;
    const int *b = (const int *) xb;
    ssize_t diff = (*sizevec)[*a] - (*sizevec)[*b];
    return (diff < 0 ? -1 : (diff == 0 ? *a - *b : 1));
}

static int string_rev_compar(const void *xa, const void *xb)
{
    const String *a = (const String *) xa;
    const String *b = (const String *) xb;
    return String::compare(*b, *a);
}

static int size_t_rev_compar(const void *xa, const void *xb)
{
    const size_t *a = (const size_t *) xa;
    const size_t *b = (const size_t *) xb;
    return (*b > *a ? 1 : (*a == *b ? 0 : -1));
}

static int string_permute_rev_compar(const void *xa, const void *xb)
{
    const int *a = (const int *) xa;
    const int *b = (const int *) xb;
    int diff = String::compare((*strvec)[*b], (*strvec)[*a]);
    return (diff ? diff : *a - *b);
}

static int size_t_permute_rev_compar(const void *xa, const void *xb)
{
    const int *a = (const int *) xa;
    const int *b = (const int *) xb;
    ssize_t diff = (*sizevec)[*b] - (*sizevec)[*a];
    return (diff < 0 ? -1 : (diff == 0 ? *a - *b : 1));
}


int
SortTest::configure(Vector<String> &conf, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
    _output = _stdc = false;
    String filename;
#endif
    bool numeric = false, permute = false;
    _reverse = false;
    if (cp_va_kparse_remove_keywords(conf, this, errh,
				     "NUMERIC", 0, cpBool, &numeric,
				     "REVERSE", 0, cpBool, &_reverse,
				     "PERMUTE", 0, cpBool, &permute,
#if CLICK_USERLEVEL
				     "FILE", 0, cpFilename, &filename,
				     "OUTPUT", 0, cpBool, &_output,
				     "STDC", 0, cpBool, &_stdc,
#endif
				     cpEnd) < 0)
	return -1;
    
#if CLICK_USERLEVEL
    if (filename) {
	FromFile ff;
	int r, complain = 0;
	String s;
	size_t sz;
	ff.filename() = filename;
	if (ff.initialize(errh) < 0)
	    return -1;
	while ((r = ff.read_line(s, errh, false)) > 0) {
	    if (!numeric)
		_strvec.push_back(s);
	    else if ((s = cp_uncomment(s)) && cp_integer(s, &sz))
		_sizevec.push_back(sz);
	    else if (s && s[0] != '#' && !complain)
		complain = errh->lwarning(ff.landmark(), "not integer");
	}
    } else
#endif
    if (numeric) {
	int complain = 0;
	size_t sz;
	for (String *sp = conf.begin(); sp != conf.end(); sp++)
	    if (*sp && cp_integer(*sp, &sz))
		_sizevec.push_back(sz);
	    else if (*sp && !complain)
		complain = errh->warning("argument not integer");
    } else
	for (String *sp = conf.begin(); sp != conf.end(); sp++)
	    _strvec.push_back(cp_unquote(*sp) + "\n");

    if (permute) {
	int s = (_strvec.size() ? _strvec.size() : _sizevec.size());
	for (int i = 0; i < s; i++)
	    _permute.push_back(i);
    }
    
    return 0;
}

int
SortTest::initialize_vec(ErrorHandler *)
{
    void *begin;
    size_t n, size;
    if (_permute.size())
	begin = _permute.begin(), n = _permute.size(), size = sizeof(int);
    else if (_strvec.size())
	begin = _strvec.begin(), n = _strvec.size(), size = sizeof(String);
    else
	begin = _sizevec.begin(), n = _sizevec.size(), size = sizeof(size_t);

    int (*compar)(const void *, const void *);
    if (_strvec.size()) {
	strvec = &_strvec;
	if (_permute.size())
	    compar = (_reverse ? string_permute_rev_compar : string_permute_compar);
	else
	    compar = (_reverse ? string_rev_compar : string_compar);
	
#if CLICK_USERLEVEL
	if (_stdc)
	    qsort(begin, n, size, compar);
	else
#endif
	click_qsort(begin, n, size, compar);

#if CLICK_USERLEVEL
	if (_output && _permute.size())
	    for (int *a = _permute.begin(); a != _permute.end(); a++)
		fwrite(_strvec[*a].data(), _strvec[*a].length(), 1, stdout);
	else if (_output)
	    for (String *a = _strvec.begin(); a != _strvec.end(); a++)
		fwrite(a->data(), a->length(), 1, stdout);
#endif
    }
    if (_sizevec.size()) {
	sizevec = &_sizevec;
	if (_permute.size())
	    compar = (_reverse ? size_t_permute_rev_compar : size_t_permute_compar);
	else
	    compar = (_reverse ? size_t_rev_compar : size_t_compar);
	
#if CLICK_USERLEVEL
	if (_stdc)
	    qsort(begin, n, size, compar);
	else
#endif
	click_qsort(begin, n, size, compar);

#if CLICK_USERLEVEL
	if (_output && _permute.size())
	    for (int *a = _permute.begin(); a != _permute.end(); a++)
		printf("%zu\n", _sizevec[*a]);
	else if (_output)
	    for (size_t *a = _sizevec.begin(); a != _sizevec.end(); a++)
		printf("%zu\n", *a);
#endif
    }

    return 0;
}

int
SortTest::initialize(ErrorHandler *errh)
{
    if (_strvec.size() || _sizevec.size())
	return initialize_vec(errh);
    
    size_t nclasses = sizeof(classes) / sizeof(classes[0]);

    click_random_srandom();
    click_qsort(classes, nclasses, sizeof(classes[0]), compar);

    for (int i = 0; i < 20; i++) {
	for (size_t x = 0; x < nclasses; x++)
	    if (strcmp(classes[x], sorted_classes[x]) != 0)
		return errh->error("sort %d, element %u differs (%s vs. %s)", i, x, classes[x], sorted_classes[x]);

	for (size_t permute = 0; permute < nclasses * 2; permute++) {
	    size_t a = random() % nclasses;
	    size_t b = random() % nclasses;
	    const char *xa = classes[a];
	    classes[a] = classes[b];
	    classes[b] = xa;
	}

	click_qsort(classes, nclasses, sizeof(classes[0]), compar);
    }

    memcpy(classes, sorted_classes, sizeof(classes));
    click_qsort(classes, nclasses, sizeof(classes[0]), compar);

    errh->message("All tests pass!");
    return 0;
}

EXPORT_ELEMENT(SortTest)
#include <click/vector.cc>
CLICK_ENDDECLS
