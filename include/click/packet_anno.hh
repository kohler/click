#ifndef CLICK_PACKET_ANNO_HH
#define CLICK_PACKET_ANNO_HH

#define MAKE_ANNOTATIONINFO(offset, size)	((size) << 16 | (offset))
#define ANNOTATIONINFO_SIZE(ai)			((ai) >> 16)
#define ANNOTATIONINFO_OFFSET(ai)		((uint16_t) (ai))

#define DST_IP_ANNO_OFFSET		0
#define DST_IP_ANNO_SIZE		4

#define DST_IP6_ANNO_OFFSET		0
#define DST_IP6_ANNO_SIZE		16

// bytes 16-31
#define WIFI_EXTRA_ANNO_OFFSET		16
#define WIFI_EXTRA_ANNO_SIZE		28
#define WIFI_EXTRA_ANNO(p)		((click_wifi_extra *) ((p)->anno_u8() + WIFI_EXTRA_ANNO_OFFSET))

// byte 16
#define PAINT_ANNO_OFFSET		16
#define PAINT_ANNO_SIZE			1
#define PAINT_ANNO(p)			((p)->anno_u8(PAINT_ANNO_OFFSET))
#define SET_PAINT_ANNO(p, v)		((p)->set_anno_u8(PAINT_ANNO_OFFSET, (v)))

// byte 17
#define ICMP_PARAMPROB_ANNO_OFFSET	17
#define ICMP_PARAMPROB_ANNO_SIZE	1
#define ICMP_PARAMPROB_ANNO(p)		((p)->anno_u8(ICMP_PARAMPROB_ANNO_OFFSET))
#define SET_ICMP_PARAMPROB_ANNO(p, v)	((p)->set_anno_u8(ICMP_PARAMPROB_ANNO_OFFSET, (v)))

// byte 19
#define FIX_IP_SRC_ANNO_OFFSET		19
#define FIX_IP_SRC_ANNO_SIZE		1
#define FIX_IP_SRC_ANNO(p)		((p)->anno_u8(FIX_IP_SRC_ANNO_OFFSET))
#define SET_FIX_IP_SRC_ANNO(p, v)	((p)->set_anno_u8(FIX_IP_SRC_ANNO_OFFSET, (v)))

// bytes 20-21
#define VLAN_TCI_ANNO_OFFSET		20
#define VLAN_TCI_ANNO_SIZE		2
#define VLAN_TCI_ANNO(p)		((p)->anno_u16(VLAN_TCI_ANNO_OFFSET))
#define SET_VLAN_TCI_ANNO(p, v)		((p)->set_anno_u16(VLAN_TCI_ANNO_OFFSET, (v)))

// bytes 20-23
#define AGGREGATE_ANNO_OFFSET		20
#define AGGREGATE_ANNO_SIZE		4
#define AGGREGATE_ANNO(p)		((p)->anno_u32(AGGREGATE_ANNO_OFFSET))
#define SET_AGGREGATE_ANNO(p, v)	((p)->set_anno_u32(AGGREGATE_ANNO_OFFSET, (v)))

#define FWD_RATE_ANNO_OFFSET		20
#define FWD_RATE_ANNO_SIZE		4
#define FWD_RATE_ANNO(p)		((p)->anno_s32(FWD_RATE_ANNO_OFFSET))
#define SET_FWD_RATE_ANNO(p, v)		((p)->set_anno_s32(FWD_RATE_ANNO_OFFSET, (v)))

#define MISC_IP_ANNO_OFFSET		20
#define MISC_IP_ANNO_SIZE		4
#define MISC_IP_ANNO(p)                 ((p)->anno_u32(MISC_IP_ANNO_OFFSET))
#define SET_MISC_IP_ANNO(p, v)		((p)->set_anno_u32(MISC_IP_ANNO_OFFSET, (v).addr()))

// bytes 24-27
#define EXTRA_PACKETS_ANNO_OFFSET	24
#define EXTRA_PACKETS_ANNO_SIZE		4
#define EXTRA_PACKETS_ANNO(p)		((p)->anno_u32(EXTRA_PACKETS_ANNO_OFFSET))
#define SET_EXTRA_PACKETS_ANNO(p, v)	((p)->set_anno_u32(EXTRA_PACKETS_ANNO_OFFSET, (v)))

#define REV_RATE_ANNO_OFFSET		24
#define REV_RATE_ANNO_SIZE		4
#define REV_RATE_ANNO(p)		((p)->anno_s32(REV_RATE_ANNO_OFFSET))
#define SET_REV_RATE_ANNO(p, v)		((p)->set_anno_s32(REV_RATE_ANNO_OFFSET, (v)))

// byte 26
#define SEND_ERR_ANNO_OFFSET		26
#define SEND_ERR_ANNO_SIZE		1
#define SEND_ERR_ANNO(p)                ((p)->anno_u8(SEND_ERR_ANNO_OFFSET))
#define SET_SEND_ERR_ANNO(p, v)         ((p)->set_anno_u8(SEND_ERR_ANNO_OFFSET, (v)))

// byte 27
#define GRID_ROUTE_CB_ANNO_OFFSET	27
#define GRID_ROUTE_CB_ANNO_SIZE		1
#define GRID_ROUTE_CB_ANNO(p)           ((p)->anno_u8(GRID_ROUTE_CB_ANNO_OFFSET))
#define SET_GRID_ROUTE_CB_ANNO(p, v)    ((p)->set_anno_u8(GRID_ROUTE_CB_ANNO_OFFSET, (v)))

// bytes 28-31
#define IPREASSEMBLER_ANNO_OFFSET	28
#define IPREASSEMBLER_ANNO_SIZE		4

#define EXTRA_LENGTH_ANNO_OFFSET	28
#define EXTRA_LENGTH_ANNO_SIZE		4
#define EXTRA_LENGTH_ANNO(p)		((p)->anno_u32(EXTRA_LENGTH_ANNO_OFFSET))
#define SET_EXTRA_LENGTH_ANNO(p, v)	((p)->set_anno_u32(EXTRA_LENGTH_ANNO_OFFSET, (v)))

// bytes 32-39
#define FIRST_TIMESTAMP_ANNO_OFFSET	32
#define FIRST_TIMESTAMP_ANNO_SIZE	8
#define CONST_FIRST_TIMESTAMP_ANNO(p)	(*(reinterpret_cast<const Timestamp *>((p)->anno_u8() + FIRST_TIMESTAMP_ANNO_OFFSET)))
#define FIRST_TIMESTAMP_ANNO(p)		(*(reinterpret_cast<Timestamp *>((p)->anno_u8() + FIRST_TIMESTAMP_ANNO_OFFSET)))
#define SET_FIRST_TIMESTAMP_ANNO(p, v)	(*(reinterpret_cast<Timestamp *>((p)->anno_u8() + FIRST_TIMESTAMP_ANNO_OFFSET)) = (v))

// bytes 32-35
#define PACKET_NUMBER_ANNO_OFFSET	32
#define PACKET_NUMBER_ANNO_SIZE		4
#define PACKET_NUMBER_ANNO(p)		((p)->anno_u32(PACKET_NUMBER_ANNO_OFFSET))
#define SET_PACKET_NUMBER_ANNO(p, v)	((p)->set_anno_u32(PACKET_NUMBER_ANNO_OFFSET, (v)))

#define IPSEC_SPI_ANNO_OFFSET		32
#define IPSEC_SPI_ANNO_SIZE		4
#define IPSEC_SPI_ANNO(p)		((p)->anno_u32(IPSEC_SPI_ANNO_OFFSET))
#define SET_IPSEC_SPI_ANNO(p, v)	((p)->set_anno_u32(IPSEC_SPI_ANNO_OFFSET, (v)))

// bytes 36-39
#define SEQUENCE_NUMBER_ANNO_OFFSET	36
#define SEQUENCE_NUMBER_ANNO_SIZE	4
#define SEQUENCE_NUMBER_ANNO(p)		((p)->anno_u32(SEQUENCE_NUMBER_ANNO_OFFSET))
#define SET_SEQUENCE_NUMBER_ANNO(p, v)	((p)->set_anno_u32(SEQUENCE_NUMBER_ANNO_OFFSET, (v)))

#if SIZEOF_VOID_P == 4
# define IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET	36
# define IPSEC_SA_DATA_REFERENCE_ANNO_SIZE	4
# define IPSEC_SA_DATA_REFERENCE_ANNO(p)	((p)->anno_u32(IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET))
# define SET_IPSEC_SA_DATA_REFERENCE_ANNO(p, v) ((p)->set_anno_u32(IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET, (v)))
#endif

#if HAVE_INT64_TYPES
// bytes 40-47
# define PERFCTR_ANNO_OFFSET		40
# define PERFCTR_ANNO_SIZE		8
# define PERFCTR_ANNO(p)		((p)->anno_u64(PERFCTR_ANNO_OFFSET))
# define SET_PERFCTR_ANNO(p, v)		((p)->set_anno_u64(PERFCTR_ANNO_OFFSET, (v)))

# if SIZEOF_VOID_P == 8
#  define IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET	40
#  define IPSEC_SA_DATA_REFERENCE_ANNO_SIZE	8
#  define IPSEC_SA_DATA_REFERENCE_ANNO(p)	((p)->anno_u64(IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET))
#  define SET_IPSEC_SA_DATA_REFERENCE_ANNO(p, v) ((p)->set_anno_u64(IPSEC_SA_DATA_REFERENCE_ANNO_OFFSET, (v)))
# endif
#endif

#endif
