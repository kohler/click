/* -*- c-basic-offset: 4 -*- */
#ifndef CLICKNET_ICMP6_H
#define CLICKNET_ICMP6_H
#include <clicknet/ip6.h>

/*
 * <clicknet/icmp6.h> -- our own definitions for ICMP6 packets
 * Based on RFC 1885
 */

/* types for ICMP6 packets */
#define ICMP6_UNREACH		1
#define ICMP6_PKTTOOBIG		2
#define ICMP6_TIMXCEED		3
#define ICMP6_PARAMPROB		4

#define ICMP6_ECHO		128
#define ICMP6_ECHOREPLY		129
#define ICMP6_MEMBERSHIPQUERY	130
#define ICMP6_MEMBERSHIPREPORT	131
#define ICMP6_MEMBERSHIPREDUCTION 132

#define ICMP6_REDIRECT		137

/* codes for spefic types of ICMP6 packets */
/* ICMP6 Error Message - Type: 1 */
#define ICMP6_UNREACH_NOROUTE	0
#define ICMP6_UNREACH_ADMIN	1
#define ICMP6_UNREACH_NOTNEIGHBOR 2
#define ICMP6_UNREACH_ADDR	3
#define ICMP6_UNREACH_NOPORT	4

/* ICMP6 Time Exceeded Message - Type: 3 */
#define ICMP6_TIMXCEED_TRANSIT	0
#define ICMP6_TIMXCEED_REASSEMBLY 1

/* ICMP6 Parameter Problem Message - Type: 4 */
#define ICMP6_PARAMPROB_HEADER	0
#define ICMP6_PARAMPROB_NEXTHEADER 1
#define ICMP6_PARAMPROB_OPTION	2

/* remove possible definitions for symbols */
#undef icmp6_identifier
#undef icmp6_sequence
#undef icmp6_pointer
#undef icmp6_maxdelay


/* most ICMP6 request types */
struct click_icmp6 {
    uint8_t icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
    uint8_t icmp6_code;		/* one of the ICMP6_CODE_*'s above */
    uint16_t icmp6_cksum;		/* 16 1's comp csum */
    uint32_t padding;			/* should be zero */
    /* followed by as much of invoking packet as will fit without the ICMPv6 packet exceeding 576 octets*/
};

/* packet too big header */
struct click_icmp6_pkttoobig {
    uint8_t icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
    uint8_t icmp6_code;		/* one of the ICMP6_CODE_*'s above */
    uint16_t icmp6_cksum;		/* 16 1's comp csum */
    uint32_t icmp6_mtusize;			/* maximum packet size */
  /* followed by as much of invoking packet as will fit without the ICMPv6 packet exceeding 576 octets*/
};

/* parameter problem header */
struct click_icmp6_paramprob {
    uint8_t icmp6_type;		/* one of the ICMP_TYPE_*'s above */
    uint8_t icmp6_code;		/* one of the ICMP6_CODE_*'s above */
    uint16_t icmp6_cksum;		/* 16 1's comp csum */
    uint32_t icmp6_pointer;		/* which octect in orig. IP6 pkt was a problem */
  /* followed by as much of invoking packet as will fit without the ICMPv6 packet exceeding 576 octets*/
};


/* struct for things with sequence numbers - echo request & echo reply msgs*/
struct click_icmp6_sequenced {
    uint8_t icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
    uint8_t icmp6_code;		/* one of the ICMP6_CODE_*'s above */
    uint16_t icmp6_cksum;		/* 16 1's comp csum */
    uint16_t icmp6_identifier;
    uint16_t icmp6_sequence;
    /* Followed by: */
    /* Echo Request: zero or more octets of arbitary data */
    /* Echo Reply: the data fromm the invoking Echo Request msg */
};

/* struct for group membership messages */
struct click_icmp6_membership {
    uint8_t icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
    uint8_t icmp6_code;		/* one of the ICMP6_CODE_*'s above */
    uint16_t icmp6_cksum;		/* 16 1's comp csum */
    uint16_t icmp6_maxdelay;   /* maximum response delay, in milliseconds */
    uint16_t padding;
    /* followed by multicast address */
};

/* struct for redirect messages */
struct click_icmp6_redirect {
    uint8_t icmp6_type;
    uint8_t icmp6_code;
    uint16_t icmp6_cksum;
    uint32_t padding;
    struct in6_addr icmp6_target;
    struct in6_addr icmp6_dst;
};

/* different struct names for each type of packet */
#define click_icmp6_unreach	click_icmp6
#define click_icmp6_timxceed	click_icmp6
#define click_icmp6_echo	click_icmp6_sequenced

#endif
