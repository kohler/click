#ifndef CLICK_TCP_H
#define CLICK_TCP_H

/*
 * click_tcp.h -- our own definition of the TCP header
 * based on a file from one of the BSDs
 */

typedef	unsigned int tcp_seq;

struct click_tcp {
    unsigned short th_sport;		/* 0-1   source port */
    unsigned short th_dport;		/* 2-3   destination port */
    tcp_seq th_seq;			/* 4-7   sequence number */
    tcp_seq th_ack;			/* 8-11  acknowledgement number */
#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    unsigned char th_x2:4;		/* 12    (unused) */
    unsigned char th_off:4;		/*       data offset in words */
#endif
#if CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    unsigned char th_off:4;		/* 12    data offset in words */
    unsigned char th_x2:4;		/*       (unused) */
#endif
    unsigned char th_flags;		/* 13    flags */
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
    unsigned short th_win;		/* 14-15 window */
    unsigned short th_sum;		/* 16-17 checksum */
    unsigned short th_urp;		/* 18-19 urgent pointer */
};

/*
 * TCP sequence number comparisons
 */

#define SEQ_LT(x,y)	((int)((x)-(y)) < 0)
#define SEQ_LEQ(x,y)	((int)((x)-(y)) <= 0)
#define SEQ_GT(x,y)	((int)((x)-(y)) > 0)
#define SEQ_GEQ(x,y)	((int)((x)-(y)) >= 0)

#endif
