#ifndef CLICK_ICMP6_H
#define CLICK_ICMP6_H
#include "click_ip6.h"

/*
 * click_icmp6.h -- our own definitions for ICMP6 packets
 * Based on RFC 1885
 */

/* types for ICMP6 packets */
#define ICMP6_DST_UNREACHABLE 1
#define ICMP6_PKT_TOOBIG 2
#define ICMP6_TYPE_TIME_EXCEEDED 3
#define ICMP6_PARAMETER_PROBLEM 4

#define ICMP6_ECHO_REQUEST 128
#define ICMP6_ECHO_REPLY 129
#define ICMP6_GRP_MEM_QRY 130
#define ICMP6_GRP_MEM_REP 131
#define ICMP6_GRP_MEM_RED 132

/* codes for spefic types of ICMP6 packets */
/* ICMP6 Error Message - Type: 1 */
#define ICMP6_CODE_NOROUTE_TO_DST 0
#define ICMP6_CODE_DST_COMM_PROH 1
#define ICMP6_CODE_NOT_A_NEIGH 2
#define ICMP6_CODE_ADD_UNREACH 3
#define ICMP6_CODE_PORTUNREACH 4

/* ICMP6 Time Exceeded Message - Type: 3 */
#define ICMP6_CODE_HOP_LIM_EXC 0
#define ICMP6_CODE_FRG_REASSEM_TIME_EXC 1

/* ICMP6 Time Exceeded Message - Type: 4 */
#define ICMP6_CODE_ERR_HEAD_FIELD 0
#define ICMP6_CODE_UNRCG_NXT_HEADER 1
#define ICMP6_CODE_UNRCG_IP6_OPTION 2


/* most ICMP6 request types */
struct icmp6_generic 
{
  unsigned char icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
  unsigned char icmp6_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp6_cksum;		/* 16 1's comp csum */
  unsigned int unused;			/* should be zero */
  /* followed by as much of invoking packet as will fit without the ICMPv6 packet exceeding 576 octets*/
};

/* packet too big header */
struct icmp6_pkt_toobig 
{
  unsigned char icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
  unsigned char icmp6_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp6_cksum;		/* 16 1's comp csum */
  unsigned int icmp6_mtusize;			/* maximum packet size */
  /* followed by as much of invoking packet as will fit without the ICMPv6 packet exceeding 576 octets*/
};

/* parameter problem header */
struct icmp6_param 
{
  unsigned char icmp6_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp6_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp6_cksum;		/* 16 1's comp csum */
  unsigned int  pointer;		/* which octect in orig. IP6 pkt was a problem */
  /* followed by as much of invoking packet as will fit without the ICMPv6 packet exceeding 576 octets*/
};


/* struct for things with sequence numbers - echo request & echo reply msgs*/
struct icmp6_sequenced 
{
  unsigned char icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
  unsigned char icmp6_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp6_cksum;		/* 16 1's comp csum */
  unsigned short identifier;
  unsigned short sequence;
  /* Followed by: */
  /* Echo Request: zero or more octets of arbitary data */
  /* Echo Reply: the data fromm the invoking Echo Request msg */
};

/* struct for group membership messages */
struct icmp6_group 
{
  unsigned char icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
  unsigned char icmp6_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp6_cksum;		/* 16 1's comp csum */
  unsigned short max_res_delay;   /* maximum response delay, in milliseconds */
  unsigned short unused;
  /* followed by multiacast address */
};


/* different struct names for each type of packet */
#define icmp6_dst_unreach icmp6_generic
#define icmp6_time_exceeded icmp6_generic
#define icmp6_echo icmp6_sequenced

#endif
