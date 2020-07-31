/*
 * <clicknet/wifi.h> - contains the definitions for 802.11 frames. 
 * It was originally taken from freebsd and modified for click use.
 * John Bicket
 */

/*	$NetBSD: if_ieee80211.h,v 1.5 2000/07/21 04:47:40 onoe Exp $	*/
/* $FreeBSD: src/sys/net/if_ieee80211.h,v 1.3.2.3 2001/07/04 00:12:38 brooks Exp $ */

#ifndef _CLICKNET_WIFI_H_
#define _CLICKNET_WIFI_H_

#define WIFI_EXTRA_MAGIC  0x7492001

enum {
  WIFI_EXTRA_TX			        	= (1<<0), /* packet transmission */
  WIFI_EXTRA_TX_FAIL				= (1<<1), /* transmission failed */
  WIFI_EXTRA_TX_USED_ALT_RATE	= (1<<2), /* used alternate bitrate */
  WIFI_EXTRA_RX_ERR		        	= (1<<3), /* failed crc check */
  WIFI_EXTRA_RX_MORE		   	 	= (1<<4), /* first part of a fragmented skb */
  WIFI_EXTRA_NO_SEQ		        	= (1<<5),
  WIFI_EXTRA_NO_TXF	 	        	= (1<<6),
  WIFI_EXTRA_DO_RTS_CTS			= (1<<7),
  WIFI_EXTRA_DO_CTS				= (1<<8),
  WIFI_EXTRA_MCS					= (1<<9),
  WIFI_EXTRA_MCS_SGI				= (1<<10),
  WIFI_EXTRA_MCS_BW_40			= (1<<11),
  WIFI_EXTRA_TX_NOACK			= (1<<12)
};

struct click_wifi_extra {
  uint32_t magic;

  uint64_t tsft;

  uint16_t flags;
  uint16_t channel;

  uint8_t rssi;
  uint8_t silence;
  uint8_t power;
  uint8_t pad;

  int8_t rate;		/* bitrate in Mbps*2 or MCS index */
  int8_t rate1;		/* bitrate in Mbps*2 or MCS index */
  int8_t rate2;		/* bitrate in Mbps*2 or MCS index */
  int8_t rate3;		/* bitrate in Mbps*2 or MCS index */

  uint8_t max_tries;
  uint8_t max_tries1;
  uint8_t max_tries2;
  uint8_t max_tries3;

} CLICK_SIZE_PACKED_ATTRIBUTE;

/*
 * generic definitions for IEEE 802.11 frames
 */
#define WIFI_ADDR_LEN 6

struct click_wifi {
	uint8_t		i_fc[2];
	uint16_t	i_dur;
	uint8_t		i_addr1[WIFI_ADDR_LEN];
	uint8_t		i_addr2[WIFI_ADDR_LEN];
	uint8_t		i_addr3[WIFI_ADDR_LEN];
	uint16_t	i_seq;
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct click_qos_control {
	uint16_t	qos_control;
} CLICK_SIZE_PACKED_ATTRIBUTE;

#define WIFI_QOS_CONTROL_QOS_TID_MASK               0x000F
#define WIFI_QOS_CONTROL_QOS_TID_SHIFT              0
#define WIFI_QOS_CONTROL_QOS_EOSP_MASK              0x0010
#define WIFI_QOS_CONTROL_QOS_EOSP_SHIFT             4
#define WIFI_QOS_CONTROL_ACK_POLICY_MASK            0x0060
#define WIFI_QOS_CONTROL_ACK_POLICY_SHIFT           5
#define WIFI_QOS_CONTROL_QOS_AMSDU_PRESENT_MASK     0x0080
#define WIFI_QOS_CONTROL_QOS_AMSDU_PRESENT_SHIFT    7
#define WIFI_QOS_CONTROL_QOS_STUFF_MASK             0xFF00
#define WIFI_QOS_CONTROL_QOS_STUFF_SHIFT            8

#define	WIFI_FC0_VERSION_MASK	0x03
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
#define	WIFI_FC0_SUBTYPE_ACTION		0xd0
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
#define WIFI_FC0_SUBTYPE_QOS               0x80
#define WIFI_FC0_SUBTYPE_QOS_NULL          0xc0

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
 *		octect information[length]
 */
typedef uint8_t *	wifi_mgt_beacon_t;

#define WIFI_BEACON_INTERVAL(beacon) \
	(beacon[8] + (beacon[9] << 8))
#define WIFI_BEACON_CAPABILITY(beacon) \
	(beacon[10] + (beacon[11] << 8))

#define	WIFI_CAPINFO_ESS		0x01
#define	WIFI_CAPINFO_IBSS		0x02
#define	WIFI_CAPINFO_CF_POLLABLE	0x04
#define	WIFI_CAPINFO_CF_POLLREQ		0x08
#define	WIFI_CAPINFO_PRIVACY		0x10

#define WIFI_MAX_RETRIES 11

#define WIFI_QOS_HAS_SEQ(wh) \
        (((wh)->i_fc[0] & \
          (WIFI_FC0_TYPE_MASK | WIFI_FC0_SUBTYPE_QOS)) == \
          (WIFI_FC0_TYPE_DATA | WIFI_FC0_SUBTYPE_QOS))

/*
 * Management information elements
 */
struct wifi_information {
	char	ssid[WIFI_NWID_LEN+1];
	struct rates {
		uint8_t 	*p;
	} rates;
	struct fh {
		uint16_t 	dwell;
		uint8_t 	set;
		uint8_t 	pattern;
		uint8_t 	index;
	} fh;
	struct ds {
		uint8_t		channel;
	} ds;
	struct cf {
		uint8_t		count;
		uint8_t		period;
		uint8_t		maxdur[2];
		uint8_t		dur[2];
	} cf;
	struct tim {
		uint8_t 	count;
		uint8_t 	period;
		uint8_t 	bitctl;
		/* uint8_t 	pvt[251]; The driver never needs to use this */
	} tim;
	struct ibss {
	    	uint16_t	atim;
	} ibss;
	struct challenge {
		uint8_t 	*p;
		uint8_t		len;
	} challenge;
};

/*
 * HT Capabilities Element
 */

#define WIFI_HT_CAPS_TYPE 45
#define WIFI_HT_CAPS_SIZE 26

#define	WIFI_HT_CI_LDPC 				0x01
#define	WIFI_HT_CI_CHANNEL_WIDTH_SET 	0x02

#define	WIFI_HT_CI_SM_PS_MASK		0x0C
#define	WIFI_HT_CI_SM_PS_SHIFT		2
#define	WIFI_HT_CI_SM_PS_STATIC		0x00
#define	WIFI_HT_CI_SM_PS_DYNAMIC	0x01
#define	WIFI_HT_CI_SM_PS_DISABLED	0x03

#define	WIFI_HT_CI_HT_GF 			0x10
#define	WIFI_HT_CI_SGI_20 			0x20
#define	WIFI_HT_CI_SGI_40			0x40

#define	WIFI_HT_CI_TX_STBC			0x80

#define	WIFI_HT_CI_RX_STBC_MASK		0x300
#define	WIFI_HT_CI_RX_STBC_SHIFT	8
#define	WIFI_HT_CI_RX_STBC_NO		0x00
#define	WIFI_HT_CI_RX_STBC_1SS		0x01
#define	WIFI_HT_CI_RX_STBC_2SS		0x02
#define	WIFI_HT_CI_RX_STBC_3SS		0x03

#define	WIFI_HT_CI_HT_DBACK			0x400
#define	WIFI_HT_CI_HT_MAX_AMSDU		0x800
#define	WIFI_HT_CI_HT_DSSS_CCK		0x1000
#define	WIFI_HT_CI_HT_PSMP			0x2000
#define	WIFI_HT_CI_HT_INTOLLERANT	0x4000
#define	WIFI_HT_CI_HT_LSIG_TXOP		0x8000

#define	WIFI_HT_CI_AMDU_PARAMS_MAX_AMPDU_LENGTH_MASK	0x03
#define	WIFI_HT_CI_AMDU_PARAMS_MAX_AMPDU_LENGTH_SHIFT	0

#define	WIFI_HT_CI_AMDU_PARAMS_MPDU_DENSITY_MASK		0x1C
#define	WIFI_HT_CI_AMDU_PARAMS_MPDU_DENSITY_SHIFT		2

#define	WIFI_HT_CI_SM12_TX_MCS_SET_DEFINED 			0x1
#define	WIFI_HT_CI_SM12_TX_RX_MCS_SET_NOT_EQUAL		0x2
#define	WIFI_HT_CI_SM12_TX_MAX_SS_MASK				0x10
#define	WIFI_HT_CI_SM12_TX_MAX_SS_SHIFT				2
#define	WIFI_HT_CI_SM12_TX_UEQM 					0x016

struct click_wifi_ht_caps {
	uint8_t		type;
	uint8_t		len;
	uint16_t	ht_caps_info;
	uint8_t		ampdu_params;
	uint8_t		rx_supported_mcs[16];
	uint16_t	ht_extended_caps;
	uint32_t	transmit_beam_forming;
	uint8_t		antenna_selection;
} CLICK_SIZE_PACKED_ATTRIBUTE;

/*
 * HT Info Element
 */

#define WIFI_HT_INFO_TYPE 61
#define WIFI_HT_INFO_SIZE 22

struct click_wifi_ht_info {
	uint8_t		type;
	uint8_t		len;
	uint8_t		primary_channel;
	uint8_t		ht_info_1_3;
	uint16_t	ht_info_2_3;
	uint16_t	ht_info_3_3;
	uint8_t		rx_supported_mcs[16];
} CLICK_SIZE_PACKED_ATTRIBUTE;


/*
 * WMM/WME Information Element
 */

/* WMM/WME Elements */
#define WIFI_WME_LEN				24
#define WIFI_WME_OUI				"\x00\x50\xf2"
#define WIFI_WME_OUI_LEN			3
#define WIFI_WME_TYPE				0x02
#define WIFI_WME_SUBTYPE			1
#define WIFI_WME_VERSION			1

/* EDCA categories */
#define AC_BE			0
#define AC_BK			1
#define AC_VI			2
#define AC_VO			3
#define AC_COUNT		4

/* EDCA Values */
#define WIFI_WME_AC_BE_ACI      	0x03
#define WIFI_WME_AC_BE_ECW  		0xA4
#define WIFI_WME_AC_BE_TXOP      	0x0000

#define WIFI_WME_AC_BK_ACI      	0x27
#define WIFI_WME_AC_BK_ECW       	0xA4
#define WIFI_WME_AC_BK_TXOP        	0x0000

#define WIFI_WME_AC_VI_ACI         	0x42
#define WIFI_WME_AC_VI_ECW         	0x43
#define WIFI_WME_AC_VI_TXOP       	0x005E

#define WIFI_WME_AC_VO_ACI        	0x62
#define WIFI_WME_AC_VO_ECW        	0x32
#define WIFI_WME_AC_VO_TXOP       	0x002F

/* WME QoS Info */
#define WIFI_WME_APSD_MASK         	0x80
#define WIFI_WME_APSD_SHIFT        	7

struct edca_ac_param {
	uint8_t   		aci;
	uint8_t   		ecw;
	uint16_t  		txop;
} CLICK_SIZE_PACKED_ATTRIBUTE;

/* WMM/WME Parameter Element */
struct click_wifi_wmm {
	uint8_t			tag_type;
	uint8_t			len;
	char			oui[WIFI_WME_OUI_LEN];
	uint8_t 		type;
	uint8_t 		subtype;
	uint8_t 		version;
	uint8_t 		qosinfo;
	uint8_t 		reserved;
	edca_ac_param 	acparam[AC_COUNT];
} CLICK_SIZE_PACKED_ATTRIBUTE;

/*
 * AMSDU subframe header
 */
struct click_wifi_amsdu_subframe_header {
	uint8_t     da[6];      /* 0-5    Ethernet destination address */
	uint8_t     sa[6];      /* 6-11   Ethernet source address */
	uint16_t    len;        /* A-MSDU Length without including header and padding */
} CLICK_SIZE_PACKED_ATTRIBUTE;

#define WIFI_RATES_MAXSIZE	15
#define WIFI_NWID_MAXSIZE	32

enum {
  WIFI_ELEMID_SSID			= 0,
  WIFI_ELEMID_RATES			= 1,
  WIFI_ELEMID_FHPARMS		= 2,
  WIFI_ELEMID_DSPARMS		= 3,
  WIFI_ELEMID_CFPARMS		= 4,
  WIFI_ELEMID_TIM			= 5,
  WIFI_ELEMID_IBSSPARMS		= 6,
  WIFI_ELEMID_CHALLENGE		= 16,
  WIFI_ELEMID_CSA	        = 37,
  WIFI_ELEMID_ERP			= 42,
  WIFI_ELEMID_HTCAPS		= 45,
  WIFI_ELEMID_XRATES		= 50,
  WIFI_ELEMID_HTINFO		= 61,
  WIFI_ELEMID_VENDOR		= 221
};
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
typedef uint8_t *	wifi_mgt_auth_t;

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

#define WIFI_WEP_HEADERSIZE (WIFI_WEP_IVLEN + WIFI_WEP_KIDLEN)

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

#define WIFI_RATE_SIZE             0x08

#define WIFI_SEQ_FRAG_MASK                 0x000f
#define WIFI_SEQ_FRAG_SHIFT                0
#define WIFI_SEQ_SEQ_MASK                  0xfff0
#define WIFI_SEQ_SEQ_SHIFT                 4

/*
 * 802.11 protocol crypto-related definitions.
 */
#define	WIFI_KEYBUF_SIZE	16
#define	WIFI_MICBUF_SIZE	(8+8)	/* space for both tx+rx keys */


#ifndef WIFI_MAX
#define WIFI_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef WIFI_MIN
#define WIFI_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/*
 * For packet capture, define the same physical layer packet header
 * structure as used in the wlan-ng driver
 */
enum {
  DIDmsg_lnxind_wlansniffrm               = 0x00000044,
  DIDmsg_lnxind_wlansniffrm_hosttime      = 0x00010044,
  DIDmsg_lnxind_wlansniffrm_mactime       = 0x00020044,
  DIDmsg_lnxind_wlansniffrm_channel       = 0x00030044,
  DIDmsg_lnxind_wlansniffrm_rssi          = 0x00040044,
  DIDmsg_lnxind_wlansniffrm_sq            = 0x00050044,
  DIDmsg_lnxind_wlansniffrm_signal        = 0x00060044,
  DIDmsg_lnxind_wlansniffrm_noise         = 0x00070044,
  DIDmsg_lnxind_wlansniffrm_rate          = 0x00080044,
  DIDmsg_lnxind_wlansniffrm_istx          = 0x00090044,
  DIDmsg_lnxind_wlansniffrm_frmlen        = 0x000A0044
};
enum {
        P80211ENUM_msgitem_status_no_value      = 0x00
};
enum {
        P80211ENUM_truth_false                  = 0x00
};

typedef struct {
  uint32_t did;
  uint16_t status;
  uint16_t len;
  uint32_t data;
} p80211item_uint32_t;

typedef struct {
  uint32_t msgcode;
  uint32_t msglen;
#define WLAN_DEVNAMELEN_MAX 16
  uint8_t devname[WLAN_DEVNAMELEN_MAX];
  p80211item_uint32_t hosttime;
  p80211item_uint32_t mactime;
  p80211item_uint32_t channel;
  p80211item_uint32_t rssi;
  p80211item_uint32_t sq;
  p80211item_uint32_t signal;
  p80211item_uint32_t noise;
  p80211item_uint32_t rate;
  p80211item_uint32_t istx;
  p80211item_uint32_t frmlen;
} wlan_ng_prism2_header;

#define LWNG_CAP_DID_BASE   (4 | (1 << 6)) /* section 4, group 1 */
#define LWNG_CAPHDR_VERSION 0x80211001

#define WIFI_SLOT_B 20
#define WIFI_DIFS_B 50
#define WIFI_SIFS_B 10
#define WIFI_ACK_B 304 // 192 + (14 * 8) / 1
#define WIFI_PLCP_HEADER_LONG_B 192
#define WIFI_PLCP_HEADER_SHORT_B 96

#define WIFI_SLOT_A 9
#define WIFI_DIFS_A 34
#define WIFI_SIFS_A 16
#define WIFI_ACK_A 44 // 25 + (14 * 8) / 6
#define WIFI_PLCP_HEADER_A 25

#define WIFI_SLOT_N 9
#define WIFI_DIFS_N 28
#define WIFI_SIFS_N 10
#define WIFI_ACK_N 50 // 33 + (14 * 8) / 6.5
#define WIFI_PLCP_HEADER_N 33

#define is_b_rate(b) ((b == 2) || (b == 4) || (b == 11) || (b == 22))

#define WIFI_CW_MIN_B 31
#define WIFI_CW_MAX_B 1023

#define WIFI_CW_MIN 15
#define WIFI_CW_MAX 1023

// 6-byte LLC header (last byte is terminating NUL)
#define WIFI_LLC_HEADER		((const uint8_t *) "\xAA\xAA\x03\x00\x00")
#define WIFI_LLC_HEADER_LEN	6

#endif /* !_CLICKNET_WIFI_H_ */
