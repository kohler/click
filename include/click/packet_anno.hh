#ifndef CLICK_PACKET_ANNO_HH
#define CLICK_PACKET_ANNO_HH

// byte 0
#define PAINT_ANNO(p)			((p)->user_anno_u8(0))
#define SET_PAINT_ANNO(p, v)		((p)->set_user_anno_u8(0, (v)))
#define PAINT_ANNO_OFFSET		0
#define PAINT_ANNO_LENGTH		1

// byte 1
#define ICMP_PARAMPROB_ANNO(p)		((p)->user_anno_u8(1))
#define SET_ICMP_PARAMPROB_ANNO(p, v)	((p)->set_user_anno_u8(1, (v)))

// byte 3
#define FIX_IP_SRC_ANNO(p)		((p)->user_anno_u8(3))
#define SET_FIX_IP_SRC_ANNO(p, v)	((p)->set_user_anno_u8(3, (v)))

// bytes 4-7
#define AGGREGATE_ANNO(p)		((p)->user_anno_u32(1))
#define SET_AGGREGATE_ANNO(p, v)	((p)->set_user_anno_u32(1, (v)))

#define FWD_RATE_ANNO(p)		((p)->user_anno_s32(1))
#define SET_FWD_RATE_ANNO(p, v)		((p)->set_user_anno_s32(1, (v)))

#define MISC_IP_ANNO(p)                 ((p)->user_anno_u32(1))
#define SET_MISC_IP_ANNO(p, v)		((p)->set_user_anno_u32(1, (v).addr()))

// bytes 8-11
#define EXTRA_PACKETS_ANNO(p)		((p)->user_anno_u32(2))
#define SET_EXTRA_PACKETS_ANNO(p, v)	((p)->set_user_anno_u32(2, (v)))

#define REV_RATE_ANNO(p)		((p)->user_anno_s32(2))
#define SET_REV_RATE_ANNO(p, v)		((p)->set_user_anno_s32(2, (v)))

// byte 10
#define SEND_ERR_ANNO(p)                ((p)->user_anno_u8(10))
#define SET_SEND_ERR_ANNO(p, v)         ((p)->set_user_anno_u8(10, (v)))

// byte 11
#define GRID_ROUTE_CB_ANNO(p)           ((p)->user_anno_u8(11))
#define SET_GRID_ROUTE_CB_ANNO(p, v)    ((p)->set_user_anno_u8(11, (v)))

// bytes 12-15
#define EXTRA_LENGTH_ANNO(p)		((p)->user_anno_u32(3))
#define SET_EXTRA_LENGTH_ANNO(p, v)	((p)->set_user_anno_u32(3, (v)))

// bytes 16-23
#define PACKET_NUMBER_ANNO(p, n)	((p)->user_anno_u32(4 + (n)))
#define SET_PACKET_NUMBER_ANNO(p, n, v)	((p)->set_user_anno_u32(4 + (n), (v)))

// bytes 16-23
#define FIRST_TIMESTAMP_ANNO(p)		(*((Timestamp*) ((p)->user_anno_u32() + 4)))
#define SET_FIRST_TIMESTAMP_ANNO(p, v)	(*((Timestamp*) ((p)->user_anno_u32() + 4)) = (v))

// bytes 16-23
#define IPSEC_SPI_ANNO(p)		((p)->user_anno_u32(4))
#define SET_IPSEC_SPI_ANNO(p, v)	((p)->set_user_anno_u32(4, (v)))
#define IPSEC_SA_DATA_REFERENCE_ANNO(p)	((p)->user_anno_u32(5))
#define SET_IPSEC_SA_DATA_REFERENCE_ANNO(p, v) ((p)->set_user_anno_u32(5, (v)))

#endif
