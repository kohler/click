#ifndef CLICK_TCP_H
#define CLICK_TCP_H

/*
 * click_tcp.h -- our own definition of the TCP header
 * based on a file from one of the BSDs
 */

#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

typedef	unsigned int tcp_seq;

struct tcp_header {
    unsigned short th_sport;		/* source port */
    unsigned short th_dport;		/* destination port */
    tcp_seq th_seq;		/* sequence number */
    tcp_seq th_ack;		/* acknowledgement number */
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned char th_x2:4;		/* (unused) */
    unsigned char th_off:4;		/* data offset */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned char th_off:4;		/* data offset */
    unsigned char th_x2:4;		/* (unused) */
#endif
    unsigned char th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
    unsigned short th_win;		/* window */
    unsigned short th_sum;		/* checksum */
    unsigned short th_urp;		/* urgent pointer */
};

/*
 * TCP sequence number comparisons
 */

#define SEQ_LT(x,y)	((int)((x)-(y)) < 0)
#define SEQ_LEQ(x,y)	((int)((x)-(y)) <= 0)
#define SEQ_GT(x,y)	((int)((x)-(y)) > 0)
#define SEQ_GEQ(x,y)	((int)((x)-(y)) >= 0)

#endif
