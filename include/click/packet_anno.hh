#ifndef PACKET_ANNO_HH
#define PACKET_ANNO_HH

// byte 0
#define PAINT_ANNO(p)			((p)->user_anno_c(0))
#define SET_PAINT_ANNO(p, v)		((p)->set_user_anno_c(0, (v)))

// byte 1
#define ICMP_PARAM_PROB_ANNO(p)		((p)->user_anno_c(1))
#define SET_ICMP_PARAM_PROB_ANNO(p, v)	((p)->set_user_anno_c(1, (v)))

// byte 3
#define FIX_IP_SRC_ANNO(p)		((p)->user_anno_c(3))
#define SET_FIX_IP_SRC_ANNO(p, v)	((p)->set_user_anno_c(3, (v)))

// bytes 4-7
#define FWD_RATE_ANNO(p)		((p)->user_anno_i(1))
#define SET_FWD_RATE_ANNO(p, v)		((p)->set_user_anno_i(1, (v)))

#define EXTRA_LENGTH_ANNO(p)		((p)->user_anno_u(1))
#define SET_EXTRA_LENGTH_ANNO(p, v)	((p)->set_user_anno_u(1, (v)))

// bytes 8-11
#define REV_RATE_ANNO(p)		((p)->user_anno_i(2))
#define SET_REV_RATE_ANNO(p, v)		((p)->set_user_anno_i(2, (v)))

#define PACKET_COUNT_ANNO(p)		((p)->user_anno_u(2))
#define SET_PACKET_COUNT_ANNO(p, v)	((p)->set_user_anno_u(2, (v)))

#endif
