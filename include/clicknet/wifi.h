/*
 * <clicknet/wifi.h> - contains the definitions for 802.11 frames. 
 * It was originally taken from freebsd and modified for click use.
 * John Bicket
 */

/*	$NetBSD: if_ieee80211.h,v 1.5 2000/07/21 04:47:40 onoe Exp $	*/
/* $FreeBSD: src/sys/net/if_ieee80211.h,v 1.3.2.3 2001/07/04 00:12:38 brooks Exp $ */

#ifndef _CLICKNET_WIFI_H_
#define _CLICKNET_WIFI_H_

/*
 * generic definitions for IEEE 802.11 frames
 */
CLICK_SIZE_PACKED_STRUCTURE(
struct click_wifi {,
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[6];
	u_int8_t	i_addr2[6];
	u_int8_t	i_addr3[6];
	u_int8_t	i_seq[2];
});

#define	WIFI_FC0_VERSION_MASK		0x03
#define	WIFI_FC0_VERSION_0		0x00
#define	WIFI_FC0_TYPE_MASK		0x0c
#define	WIFI_FC0_TYPE_MGT		0x00
#define	WIFI_FC0_TYPE_CTL		0x04
#define	WIFI_FC0_TYPE_DATA		0x08

#define	WIFI_FC0_SUBTYPE_MASK		0xf0
/* for TYPE_MGT */
#define	WIFI_FC0_SUBTYPE_ASSOC_REQ	0x00
#define	WIFI_FC0_SUBTYPE_ASSOC_RESP	0x10
#define	WIFI_FC0_SUBTYPE_REASSOC_REQ	0x20
#define	WIFI_FC0_SUBTYPE_REASSOC_RESP	0x30
#define	WIFI_FC0_SUBTYPE_PROBE_REQ	0x40
#define	WIFI_FC0_SUBTYPE_PROBE_RESP	0x50
#define	WIFI_FC0_SUBTYPE_BEACON		0x80
#define	WIFI_FC0_SUBTYPE_ATIM		0x90
#define	WIFI_FC0_SUBTYPE_DISASSOC	0xa0
#define	WIFI_FC0_SUBTYPE_AUTH		0xb0
#define	WIFI_FC0_SUBTYPE_DEAUTH		0xc0
/* for TYPE_CTL */
#define	WIFI_FC0_SUBTYPE_PS_POLL	0xa0
#define	WIFI_FC0_SUBTYPE_RTS		0xb0
#define	WIFI_FC0_SUBTYPE_CTS		0xc0
#define	WIFI_FC0_SUBTYPE_ACK		0xd0
#define	WIFI_FC0_SUBTYPE_CF_END		0xe0
#define	WIFI_FC0_SUBTYPE_CF_END_ACK	0xf0
/* for TYPE_DATA (bit combination) */
#define WIFI_FC0_SUBTYPE_DATA		0x00
#define	WIFI_FC0_SUBTYPE_CF_ACK		0x10
#define	WIFI_FC0_SUBTYPE_CF_POLL	0x20
#define	WIFI_FC0_SUBTYPE_CF_ACPL	0x30
#define	WIFI_FC0_SUBTYPE_NODATA		0x40
#define	WIFI_FC0_SUBTYPE_CFACK		0x50
#define	WIFI_FC0_SUBTYPE_CFPOLL		0x60
#define	WIFI_FC0_SUBTYPE_CF_ACK_CF_ACK	0x70

#define	WIFI_FC1_DIR_MASK		0x03
#define	WIFI_FC1_DIR_NODS		0x00	/* STA->STA */
#define	WIFI_FC1_DIR_TODS		0x01	/* STA->AP  */
#define	WIFI_FC1_DIR_FROMDS		0x02	/* AP ->STA */
#define	WIFI_FC1_DIR_DSTODS		0x03	/* AP ->AP  */

#define	WIFI_FC1_MORE_FRAG		0x04
#define	WIFI_FC1_RETRY			0x08
#define	WIFI_FC1_PWR_MGT		0x10
#define	WIFI_FC1_MORE_DATA		0x20
#define	WIFI_FC1_WEP			0x40
#define	WIFI_FC1_ORDER			0x80

#define	WIFI_NWID_LEN			32

/*
 * BEACON management packets
 *
 *	octect timestamp[8]
 *	octect beacon interval[2]
 *	octect capability information[2]
 *	information element
 *		octect elemid
 *		octect length
 *		octect information[length[
 */
typedef u_int8_t *	wifi_mgt_beacon_t;

#define WIFI_BEACON_INTERVAL(beacon) \
	(beacon[8] + (beacon[9] << 8))
#define WIFI_BEACON_CAPABILITY(beacon) \
	(beacon[10] + (beacon[11] << 8))

#define	WIFI_CAPINFO_ESS		0x01
#define	WIFI_CAPINFO_IBSS		0x02
#define	WIFI_CAPINFO_CF_POLLABLE	0x04
#define	WIFI_CAPINFO_CF_POLLREQ		0x08
#define	WIFI_CAPINFO_PRIVACY		0x10


/*
 * Management information elements
 */
struct wifi_information {
	char	ssid[WIFI_NWID_LEN+1];
	struct rates {
		u_int8_t 	*p;
	} rates;
	struct fh {
		u_int16_t 	dwell;
		u_int8_t 	set;
		u_int8_t 	pattern;
		u_int8_t 	index;
	} fh;
	struct ds {
		u_int8_t	channel;
	} ds;
	struct cf {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	maxdur[2];
		u_int8_t	dur[2];
	} cf;
	struct tim {
		u_int8_t 	count;
		u_int8_t 	period;
		u_int8_t 	bitctl;
		/* u_int8_t 	pvt[251]; The driver never needs to use this */
	} tim;
	struct ibss {
	    	u_int16_t	atim;
	} ibss;
	struct challenge {
		u_int8_t 	*p;
		u_int8_t	len;
	} challenge;
};


#define WIFI_RATES_MAXSIZE              15
#define WIFI_NWID_MAXSIZE               32
#define	WIFI_ELEMID_SSID		0
#define	WIFI_ELEMID_RATES		1
#define	WIFI_ELEMID_FHPARMS		2
#define	WIFI_ELEMID_DSPARMS		3
#define	WIFI_ELEMID_CFPARMS		4
#define	WIFI_ELEMID_TIM			5
#define	WIFI_ELEMID_IBSSPARMS		6
#define	WIFI_ELEMID_CHALLENGE		16
#define	WIFI_ELEMID_ERP		        42
#define	WIFI_ELEMID_VENDOR	        221

/*
 * AUTH management packets
 *
 *	octect algo[2]
 *	octect seq[2]
 *	octect status[2]
 *	octect chal.id
 *	octect chal.length
 *	octect chal.text[253]
 */
typedef u_int8_t *	wifi_mgt_auth_t;

#define WIFI_AUTH_ALGORITHM(auth) \
    (auth[0] + (auth[1] << 8))
#define WIFI_AUTH_TRANSACTION(auth) \
    (auth[2] + (auth[3] << 8))
#define WIFI_AUTH_STATUS(auth) \
    (auth[4] + (auth[5] << 8))

#define	WIFI_AUTH_ALG_OPEN		0x0000
#define	WIFI_AUTH_ALG_SHARED		0x0001

#define WIFI_AUTH_OPEN_REQUEST		1
#define WIFI_AUTH_OPEN_RESPONSE		2

#define WIFI_AUTH_SHARED_REQUEST	1
#define WIFI_AUTH_SHARED_CHALLENGE	2
#define WIFI_AUTH_SHARED_RESPONSE	3
#define WIFI_AUTH_SHARED_PASS		4

/*
 * Reason codes
 *
 * Unlisted codes are reserved
 */
#define	WIFI_REASON_UNSPECIFIED		1
#define	WIFI_REASON_AUTH_EXPIRE		2
#define	WIFI_REASON_AUTH_LEAVE		3
#define	WIFI_REASON_ASSOC_EXPIRE	4
#define	WIFI_REASON_ASSOC_TOOMANY	5
#define	WIFI_REASON_NOT_AUTHED		6  
#define	WIFI_REASON_NOT_ASSOCED		7
#define	WIFI_REASON_ASSOC_LEAVE		8
#define	WIFI_REASON_ASSOC_NOT_AUTHED	9

/*
 * Status code
 *
 * Unlisted codes are reserved
 */
#define WIFI_STATUS_SUCCESS		0x0000
#define	WIFI_STATUS_UNSPECIFIED		1
#define	WIFI_STATUS_CAPINFO		10
#define	WIFI_STATUS_NOT_ASSOCED		11
#define	WIFI_STATUS_OTHER		12
#define	WIFI_STATUS_ALG			13
#define	WIFI_STATUS_SEQUENCE		14
#define	WIFI_STATUS_CHALLENGE		15
#define	WIFI_STATUS_TIMEOUT		16
#define	WIFI_STATUS_BASIC_RATES		18
#define WIFI_STATUS_TOO_MANY_STATIONS   22
#define	WIFI_STATUS_RATES		23
#define WIFI_STATUS_SHORTSLOT_REQUIRED  25


#define	WIFI_WEP_KEYLEN			5	/* 40bit */
#define	WIFI_WEP_IVLEN			3	/* 24bit */
#define	WIFI_WEP_KIDLEN			1	/* 1 octet */
#define	WIFI_WEP_CRCLEN			4	/* CRC-32 */
#define	WIFI_WEP_NKID			4	/* number of key ids */

#define WIFI_WEP_NOSUP	-1
#define WIFI_WEP_OFF	0
#define WIFI_WEP_ON	1
#define WIFI_WEP_MIXED	2

#define WIFI_AUTH_NONE	0
#define WIFI_AUTH_OPEN	1
#define WIFI_AUTH_SHARED	2

#define WIFI_POWERSAVE_NOSUP	-1
#define WIFI_POWERSAVE_OFF		0
#define WIFI_POWERSAVE_CAM		1
#define WIFI_POWERSAVE_PSP		2
#define WIFI_POWERSAVE_PSP_CAM	3
#define WIFI_POWERSAVE_ON		WIFI_POWERSAVE_CAM

#define	WIFI_RATE_BASIC			0x80
#define	WIFI_RATE_VAL			0x7f

#endif /* !_CLICKNET_WIFI_H_ */
