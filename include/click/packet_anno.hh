#ifndef PACKET_ANNO_HH
#define PACKET_ANNO_HH

#define PAINT_ANNO(p)			((p)->user_anno_c(0))
#define SET_PAINT_ANNO(p, c)		((p)->set_user_anno_c(0, (c)))

#define ICMP_PARAM_PROB_ANNO(p)		((p)->user_anno_c(1))
#define SET_ICMP_PARAM_PROB_ANNO(p, c)	((p)->set_user_anno_c(1, (c)))

#define FIX_IP_SRC_ANNO(p)		((p)->user_anno_c(3))
#define SET_FIX_IP_SRC_ANNO(p, c)	((p)->set_user_anno_c(3, (c)))

#endif
