#ifndef FAKEPCAP_H
#define FAKEPCAP_H

struct pcap_t;
#define pcap_fileno(p)			-1
#define pcap_close(p)			(void)0
#define pcap_open_live(a,b,c,d,e)	0
#define pcap_open_offline(a,b)		0
#define pcap_dispatch(a,b,c,d)		0
#define PCAP_ERRBUF_SIZE 		16

#endif
