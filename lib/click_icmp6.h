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
/* dest unreachable packets */
#//define ICMP_CODE_PROTUNREACH 2
#define ICMP6_CODE_PORTUNREACH 4
/* echo packets */
//#define ICMP6_CODE_ECHO 0
/* timestamp packets */
//#define ICMP6_CODE_TIMESTAMP 0


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
  unsigned char icmp_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp_cksum;		/* 16 1's comp csum */
  unsigned char pointer[3];		/* which octect was a problem */
  unsigned char unused;		/* should be zero */
  /* followed by original IP header and first 8 octets of data */
};


/* struct for things with sequence numbers */
struct icmp6_sequenced 
{
  unsigned char icmp6_type;		/* one of the ICMP6_TYPE_*'s above */
  unsigned char icmp6_code;		/* one of the ICMP6_CODE_*'s above */
  unsigned short icmp6_cksum;		/* 16 1's comp csum */
  unsigned short identifier;
  unsigned short sequence;
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
#define icmp6_unreach icmp6_generic
#define icmp6_exceeded icmp6_generic
//#define icmp_quence icmp_generic
//#define icmp_info icmp_sequenced
#define icmp_echo icmp6_sequenced

#endif
