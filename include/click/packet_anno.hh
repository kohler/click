#ifndef CLICK_PACKET_ANNO_HH
#define CLICK_PACKET_ANNO_HH

// byte 0
#define PAINT_ANNO(p)			((p)->user_anno_c(0))
#define SET_PAINT_ANNO(p, v)		((p)->set_user_anno_c(0, (v)))
#define PAINT_ANNO_OFFSET		0
#define PAINT_ANNO_LENGTH		1

// byte 1
#define ICMP_PARAM_PROB_ANNO(p)		((p)->user_anno_c(1))
#define SET_ICMP_PARAM_PROB_ANNO(p, v)	((p)->set_user_anno_c(1, (v)))

// byte 3
#define FIX_IP_SRC_ANNO(p)		((p)->user_anno_c(3))
#define SET_FIX_IP_SRC_ANNO(p, v)	((p)->set_user_anno_c(3, (v)))

// bytes 4-7
#define AGGREGATE_ANNO(p)		((p)->user_anno_u(1))
#define SET_AGGREGATE_ANNO(p, v)	((p)->set_user_anno_u(1, (v)))

#define FWD_RATE_ANNO(p)		((p)->user_anno_i(1))
#define SET_FWD_RATE_ANNO(p, v)		((p)->set_user_anno_i(1, (v)))

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

// bytes 12-15
#define EXTRA_LENGTH_ANNO(p)		((p)->user_anno_u(3))
#define SET_EXTRA_LENGTH_ANNO(p, v)	((p)->set_user_anno_u(3, (v)))

// bytes 16-23
#define FIRST_TIMESTAMP_ANNO(p)		(*((const struct timeval *)((p)->all_user_anno_u() + 4)))
#define SET_FIRST_TIMESTAMP_ANNO(p, v)	(*((struct timeval *)((p)->all_user_anno_u() + 4)) = (v))

#endif
