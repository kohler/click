#ifndef FAKEPCAP_H
#define FAKEPCAP_H

struct pcap_t;
#define pcap_fileno(p)			-1
#define pcap_close(p)			(void)0
#define pcap_open_live(a,b,c,d,e)	0
#define pcap_open_offline(a,b)		0
#define pcap_dispatch(a,b,c,d)		0
#define PCAP_ERRBUF_SIZE 		16


#define FAKE_TCPDUMP_MAGIC		0xa1b2c3d4
#define FAKE_PCAP_VERSION_MAJOR		2
#define FAKE_PCAP_VERSION_MINOR		4

#define FAKE_DLT_EN10MB			1	/* Ethernet (10Mb) */
#define FAKE_DLT_RAW			12	/* raw IP */

/*
 * The first record in the file contains saved values for some
 * of the flags used in the printout phases of tcpdump.
 * Many fields here are 32 bit ints so compilers won't insert unwanted
 * padding; these files need to be interchangeable across architectures.
 */
struct fake_pcap_file_header {
	u_int32_t magic;
	u_int16_t version_major;
	u_int16_t version_minor;
	int32_t thiszone;	/* gmt to local correction */
	u_int32_t sigfigs;	/* accuracy of timestamps */
	u_int32_t snaplen;	/* max length saved portion of each pkt */
	u_int32_t linktype;	/* data link type (DLT_*) */
};

struct fake_bpf_timeval
{
	u_int32_t tv_sec;
	u_int32_t tv_usec;
};

/*
 * Each packet in the dump file is prepended with this generic header.
 * This gets around the problem of different headers for different
 * packet interfaces.
 */
struct fake_pcap_pkthdr {
	struct fake_bpf_timeval ts;	/* time stamp */
	u_int32_t caplen;	/* length of portion present */
	u_int32_t len;		/* length this packet (off wire) */
};

#endif
