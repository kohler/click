/* -*- c-basic-offset: 4 -*- */
#ifndef CLICKNET_TCP_H
#define CLICKNET_TCP_H

/*
 * <clicknet/tcp.h> -- our own definition of the TCP header
 * based on a file from one of the BSDs
 */

typedef	uint32_t tcp_seq_t;

struct click_tcp {
    uint16_t	th_sport;		/* 0-1   source port */
    uint16_t	th_dport;		/* 2-3   destination port */
    tcp_seq_t	th_seq;			/* 4-7   sequence number */
    tcp_seq_t	th_ack;			/* 8-11  acknowledgement number */
#if CLICK_BYTE_ORDER == CLICK_LITTLE_ENDIAN
    unsigned	th_x2 : 4;		/* 12    (unused) */
    unsigned	th_off : 4;		/*       data offset in words */
#elif CLICK_BYTE_ORDER == CLICK_BIG_ENDIAN
    unsigned	th_off : 4;		/* 12    data offset in words */
    unsigned	th_x2 : 4;		/*       (unused) */
#else
#   error "unknown byte order"
#endif
    uint8_t	th_flags;		/* 13    flags */
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
#define	TH_ECE	0x40
#define	TH_CWR	0x80
    uint16_t	th_win;			/* 14-15 window */
    uint16_t	th_sum;			/* 16-17 checksum */
    uint16_t	th_urp;			/* 18-19 urgent pointer */
};

/*
 * TCP sequence number comparisons
 */

#define SEQ_LT(x,y)	((int)((x)-(y)) < 0)
#define SEQ_LEQ(x,y)	((int)((x)-(y)) <= 0)
#define SEQ_GT(x,y)	((int)((x)-(y)) > 0)
#define SEQ_GEQ(x,y)	((int)((x)-(y)) >= 0)

/*
 * TCP options
 */

#define TCPOPT_EOL		0
#define TCPOPT_NOP		1
#define TCPOPT_MAXSEG		2
#define TCPOLEN_MAXSEG		4
#define TCPOPT_WSCALE		3
#define TCPOLEN_WSCALE		3
#define TCPOPT_SACK_PERMITTED	4
#define TCPOLEN_SACK_PERMITTED	2
#define TCPOPT_SACK		5
#define TCPOPT_TIMESTAMP	8
#define TCPOLEN_TIMESTAMP	10

#endif
