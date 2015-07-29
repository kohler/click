#ifndef CLICK_FAKEPCAP_HH
#define CLICK_FAKEPCAP_HH
#include <click/string.hh>
#include <click/packet.hh>
CLICK_DECLS

#define FAKE_PCAP_MAGIC			0xA1B2C3D4
#define FAKE_PCAP_MAGIC_NANO		0xA1B23C4D
#define	FAKE_MODIFIED_PCAP_MAGIC	0xA1B2CD34
#define FAKE_PCAP_VERSION_MAJOR		2
#define FAKE_PCAP_VERSION_MINOR		4

/* Canonical (pcap file) data link types (may differ from host versions) */
#define FAKE_DLT_NONE			(-1)	/* Unknown */
#define FAKE_DLT_NULL			0	/* Null encapsulation */
#define FAKE_DLT_EN10MB			1	/* Ethernet (10Mb) */
#define FAKE_DLT_PPP			9	/* PPP */
#define FAKE_DLT_FDDI			10	/* FDDI */
#define FAKE_DLT_PPP_HDLC		50	/* PPP or Cisco HDLC */
#define FAKE_DLT_ATM_RFC1483		100	/* RFC 1483-encapsulated ATM */
#define FAKE_DLT_RAW			101	/* raw IP */
#define FAKE_DLT_C_HDLC			104	/* Cisco HDLC */
#define FAKE_DLT_IEEE802_11             105     /* IEEE 802.11 wireless */
#define FAKE_DLT_LINUX_SLL              113     /* Linux cooked socket */
#define FAKE_DLT_PRISM_HEADER		119	/* 802.11+Prism II monitor code */
#define FAKE_DLT_AIRONET_HEADER	        120     /* Aironet wireless header */
#define FAKE_DLT_SUNATM			123	/* Full Frontal ATM: ATM header + ATM_RFC1483 */
#define FAKE_DLT_IEEE802_11_RADIO	127	/* 802.11 plus radiotap radio header */

/* Host data link types */
#define FAKE_DLT_HOST_RAW		12	/* raw IP */

/*
 * The first record in the file contains saved values for some
 * of the flags used in the printout phases of tcpdump.
 * Many fields here are 32 bit ints so compilers won't insert unwanted
 * padding; these files need to be interchangeable across architectures.
 */
struct fake_pcap_file_header {
	uint32_t magic;
	uint16_t version_major;
	uint16_t version_minor;
	int32_t thiszone;	/* gmt to local correction */
	uint32_t sigfigs;	/* accuracy of timestamps */
	uint32_t snaplen;	/* max length saved portion of each pkt */
	uint32_t linktype;	/* data link type (DLT_*) */
};

struct fake_bpf_timeval {
	int32_t tv_sec;
	int32_t tv_usec;
};

union fake_bpf_timeval_union {
    struct fake_bpf_timeval tv;
    Timestamp::rep_t timestamp_rep;
    inline static Timestamp make_timestamp(const fake_bpf_timeval_union* x, bool nano);
};

/*
 * Each packet in the dump file is prepended with this generic header.
 * This gets around the problem of different headers for different
 * packet interfaces.
 */
struct fake_pcap_pkthdr {
	fake_bpf_timeval_union ts;	/* time stamp */
	uint32_t caplen;	/* length of portion present */
	uint32_t len;		/* length this packet (off wire) */
};

/* Unfortunately, Linux tcpdump generates a different format. */
struct fake_modified_pcap_pkthdr {
	struct fake_pcap_pkthdr hdr;	/* the regular header */
	uint32_t ifindex;	/* index, in *capturing* machine's list of
				   interfaces, of the interface on which this
				   packet came in. */
	uint16_t protocol;	/* Ethernet packet type */
	uint8_t pkt_type;	/* broadcast/multicast/etc. indication */
	uint8_t pad;		/* pad to a 4-byte boundary */
};

// Parsing and unparsing.
int fake_pcap_parse_dlt(const String&);
String fake_pcap_unparse_dlt(int);
int fake_pcap_canonical_dlt(int, bool from_file);

// Handling FORCE_IP.
bool fake_pcap_dlt_force_ipable(int);
bool fake_pcap_force_ip(Packet*&, int);
bool fake_pcap_force_ip(WritablePacket*&, int);


inline bool
fake_pcap_force_ip(WritablePacket*& p, int dlt)
{
    return fake_pcap_force_ip(reinterpret_cast<Packet*&>(p), dlt);
}

inline Timestamp
fake_bpf_timeval_union::make_timestamp(const fake_bpf_timeval_union* x, bool nano)
{
    if (nano) {
#if TIMESTAMP_REP_BIG_ENDIAN && TIMESTAMP_NANOSEC
        return Timestamp(x->timestamp_rep);
#else
        return Timestamp::make_nsec(x->tv.tv_sec, x->tv.tv_usec);
#endif
    } else {
#if TIMESTAMP_REP_BIG_ENDIAN && !TIMESTAMP_NANOSEC
        return Timestamp(x->timestamp_rep);
#else
        return Timestamp::make_usec(x->tv.tv_sec, x->tv.tv_usec);
#endif
    }
}

CLICK_ENDDECLS
#endif
