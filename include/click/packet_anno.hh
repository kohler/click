#ifndef CLICK_PACKET_ANNO_HH
#define CLICK_PACKET_ANNO_HH

// byte 0
#define PAINT_ANNO(p)			((p)->user_anno_c(0))
#define SET_PAINT_ANNO(p, v)		((p)->set_user_anno_c(0, (v)))
#define PAINT_ANNO_OFFSET		0
#define PAINT_ANNO_LENGTH		1

// byte 1
#define ICMP_PARAMPROB_ANNO(p)		((p)->user_anno_c(1))
#define SET_ICMP_PARAMPROB_ANNO(p, v)	((p)->set_user_anno_c(1, (v)))

// byte 3
#define FIX_IP_SRC_ANNO(p)		((p)->user_anno_c(3))
#define SET_FIX_IP_SRC_ANNO(p, v)	((p)->set_user_anno_c(3, (v)))

// bytes 4-7
#define AGGREGATE_ANNO(p)		((p)->user_anno_u(1))
#define SET_AGGREGATE_ANNO(p, v)	((p)->set_user_anno_u(1, (v)))

#define FWD_RATE_ANNO(p)		((p)->user_anno_i(1))
#define SET_FWD_RATE_ANNO(p, v)		((p)->set_user_anno_i(1, (v)))

#define MISC_IP_ANNO(p)                 ((p)->user_anno_u(1))
#define SET_MISC_IP_ANNO(p, v)             ((p)->set_user_anno_i(1, (v).addr()))

// bytes 8-11
#define EXTRA_PACKETS_ANNO(p)		((p)->user_anno_u(2))
#define SET_EXTRA_PACKETS_ANNO(p, v)	((p)->set_user_anno_u(2, (v)))

#define REV_RATE_ANNO(p)		((p)->user_anno_i(2))
#define SET_REV_RATE_ANNO(p, v)		((p)->set_user_anno_i(2, (v)))

// byte 10
#define SEND_ERR_ANNO(p)                ((p)->user_anno_c(10))
#define SET_SEND_ERR_ANNO(p, v)         ((p)->set_user_anno_c(10, (v)))

// byte 11
#define GRID_ROUTE_CB_ANNO(p)           ((p)->user_anno_c(11))
#define SET_GRID_ROUTE_CB_ANNO(p, v)    ((p)->set_user_anno_c(11, (v)))

// bytes 8 - 15
#define WIFI_NUM_FAILURES(p)           ((p)->user_anno_c(8))
#define SET_WIFI_NUM_FAILURES(p, v)    ((p)->set_user_anno_c(8, (v)))

#define WIFI_FROM_CLICK(p)           ((p)->user_anno_c(9) == 0xaa)
#define SET_WIFI_FROM_CLICK(p)    ((p)->set_user_anno_c(9, (0xaa)))

#define WIFI_TX_SUCCESS_ANNO(p)           ((p)->user_anno_c(10))
#define SET_WIFI_TX_SUCCESS_ANNO(p, v)    ((p)->set_user_anno_c(10, (v)))

#define WIFI_TX_POWER_ANNO(p)           ((p)->user_anno_c(11))
#define SET_WIFI_TX_POWER_ANNO(p, v)    ((p)->set_user_anno_c(11, (v)))

#define WIFI_RATE_ANNO(p)           ((p)->user_anno_c(12))
#define SET_WIFI_RATE_ANNO(p, v)    ((p)->set_user_anno_c(12, (v)))

#define WIFI_RETRIES_ANNO(p)           ((p)->user_anno_c(13))
#define SET_WIFI_RETRIES_ANNO(p, v)    ((p)->set_user_anno_c(13, (v)))

#define WIFI_SIGNAL_ANNO(p)           ((p)->user_anno_c(14))
#define SET_WIFI_SIGNAL_ANNO(p, v)    ((p)->set_user_anno_c(14, (v)))

#define WIFI_NOISE_ANNO(p)           ((p)->user_anno_c(15))
#define SET_WIFI_NOISE_ANNO(p, v)    ((p)->set_user_anno_c(15, (v)))

// bytes 12-15
#define EXTRA_LENGTH_ANNO(p)		((p)->user_anno_u(3))
#define SET_EXTRA_LENGTH_ANNO(p, v)	((p)->set_user_anno_u(3, (v)))

// bytes 16-23
#define FIRST_TIMESTAMP_ANNO(p)		(*((const struct timeval *)((p)->all_user_anno_u() + 4)))
#define SET_FIRST_TIMESTAMP_ANNO(p, v)	(*((struct timeval *)((p)->all_user_anno_u() + 4)) = (v))

#endif
