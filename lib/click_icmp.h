#ifndef CLICK_ICMP_H
#define CLICK_ICMP_H
#include "click_ip.h"

/*
 * click_icmp.h -- our own definitions for ICMP packets
 * based on a file from one of the BSDs
 */

/* types for ICMP packets */

#define ICMP_ECHO_REPLY 0
#define ICMP_DST_UNREACHABLE 3
#define ICMP_SOURCE_QUENCH 4
#define ICMP_REDIRECT 5
#define ICMP_ECHO 8
#define ICMP_TYPE_TIME_EXCEEDED 11
#define ICMP_PARAMETER_PROBLEM 12
#define ICMP_TIME_STAMP 13
#define ICMP_TIME_STAMP_REPLY 14
#define ICMP_INFO_REQUEST 15
#define ICMP_INFO_REQUEST_REPLY 16

/* codes for spefic types of ICMP packets */
/* dest unreachable packets */
#define ICMP_CODE_PROTUNREACH 2
#define ICMP_CODE_PORTUNREACH 3
/* echo packets */
#define ICMP_CODE_ECHO 0
/* timestamp packets */
#define ICMP_CODE_TIMESTAMP 0


/* most icmp request types */
struct icmp_generic 
{
  unsigned char icmp_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp_code;		/* one of the ICMP_CODE_*'s above */
  unsigned short icmp_cksum;		/* 16 1's comp csum */
  unsigned int unused;			/* should be zero */
  /* followed by original IP header and first 8 octets of data */
};


/* parameter problem header */
struct icmp_param 
{
  unsigned char icmp_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp_code;		/* one of the ICMP_CODE_*'s above */
  unsigned short icmp_cksum;		/* 16 1's comp csum */
  unsigned char pointer;		/* which octect was a problem */
  unsigned char unused[3];		/* should be zero */
  /* followed by original IP header and first 8 octets of data */
};


/* redirect header */
struct icmp_redirect 
{
  unsigned char icmp_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp_code;		/* one of the ICMP_CODE_*'s above */
  unsigned short icmp_cksum;		/* 16 1's comp csum */
  unsigned int gateway;			/* address of gateway */
  /* followed by original IP header and first 8 octets of data */
};


/* struct for things with sequence numbers */
struct icmp_sequenced 
{
  unsigned char icmp_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp_code;		/* one of the ICMP_CODE_*'s above */
  unsigned short icmp_cksum;		/* 16 1's comp csum */
  unsigned short identifier;
  unsigned short sequence;
};


/* timestamp header */
struct icmp_time 
{
  unsigned char icmp_type;		/* one of the ICMP_TYPE_*'s above */
  unsigned char icmp_code;		/* one of the ICMP_CODE_*'s above */
  unsigned short icmp_cksum;		/* 16 1's comp csum */
  unsigned short identifier;
  unsigned short sequence;
  unsigned int orig;			/* orignate timestamp */
  unsigned int receive;			/* receive timestamp */
  unsigned int transmit;		/* transmit timestamp */
};


/* different struct names for each type of packet */
#define icmp_unreach icmp_generic
#define icmp_exceeded icmp_generic
#define icmp_quence icmp_generic
#define icmp_info icmp_sequenced
#define icmp_echo icmp_sequenced

#endif
