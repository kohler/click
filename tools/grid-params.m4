dnl 
dnl grid-params.h
dnl 
dnl This file defines various Grid Click configuration parameters.  
dnl 

dnl
dnl Why m4?  Why not cpp?  The answer is that cpp wants to tokenize
dnl its whole input.  This causes cpp to put spaces around characters
dnl such as `/', which have special meanings in Click.  e.g. patterns
dnl for Classifier look like ``04/5F'', with no space around `/'.
dnl

dnl 
dnl Control and debugging facility parameters.  Two ControlSockets, one
dnl read-only, one read-write.  The read-write socket should be blocked 
dnl from most of the world by firewall code on the relay nodes.
dnl 
define(CONTROL_PORT,         7777)
define(CONTROL_RO,           false)
define(CONTROL2_PORT,        7779)
define(CONTROL2_RO,          true)

define(CHATTER_PORT,         7776)

define(ROUTELOG_PORT,        7778)
define(ROUTELOG_CHANNEL,     routelog)

define(PROBE_PORT,	     7774)
define(PROBE_CHANNEL,        probechannel)


dnl
dnl Geographic forwarding parameters.
dnl
dnl -1 means don't filter by range 
define(MAX_RANGE_FILTER,     -1) 


dnl 
dnl Local DSDV parameters.  Time parameters in milliseconds.
dnl 
define(ROUTE_TIMEOUT,        3200)

define(BROADCAST_PERIOD,     1300)
define(BROADCAST_JITTER,     300)
define(BROADCAST_MIN_PERIOD, 500)

dnl 100 ~= infinity 
define(NUM_HOPS,             100) 

dnl
dnl Link tracking parameters.  Time parameters in milliseconds.
dnl
define(LINK_TRACKER_TAU,     2000)
define(LINK_STAT_WINDOW,     20000)


dnl
dnl Static configuration parameters.  These parameters depend on the
dnl various protocol structures, but are the same for everyone in the
dnl network running the same version of the protocol.
dnl
dnl The parameters in this section are constants which can be derived from
dnl the Click binary by using the GridHeaderInfo element.  The calling
dnl script should define them for m4 using a -D option.
dnl
dnl Alternatively, the calling script can use the statically defined
dnl defaults by defining USE_STATIC_PROTO and USE_STATIC_OFFSETS

dnl 
dnl Protocol numbers, in hexadecimal.
dnl
ifdef(`USE_STATIC_PROTO',
`errprint(`Warning: you are using statically configured Grid protocol numbers.  They are likely to be incorrect!')
define(GRID_ETH_PROTO,           7fff)
define(GRID_PROTO_LR_HELLO,      02)
define(GRID_PROTO_NBR_ENCAP,     03)
define(GRID_PROTO_LOC_QUERY,     04)
define(GRID_PROTO_LOC_REPLY,     05)
define(GRID_PROTO_ROUTE_PROBE,   06)
define(GRID_PROTO_ROUTE_REPLY,   07)
define(GRID_PROTO_GEOCAST,       08)
define(GRID_PROTO_LINK_PROBE,    09)') dnl USE_STATIC_PROTO


dnl 
dnl Header offsets, in decimal.
dnl
ifdef(`USE_STATIC_OFFSETS',
`errprint(`Warning: you are using statically configured header offsets.  They are likely to be incorrect!')

dnl Offset of Grid protocol number in Grid packet. 
dnl sizeof(ether) + offsetof(grid_hdr, type) 
define(OFFSET_GRID_PROTO,        19)

dnl Offset of IP packet in Grid encapsulation.
dnl sizeof(ether) + sizeof(grid_hdr) + sizeof(grid_nbr_encap) 
define(OFFSET_ENCAP_IP,          82)

dnl Offset of destination IP in location query. 
dnl sizeof(ether) + sizeof(grid_hdr) + offsetof(grid_loc_query, dst_ip) 
define(OFFSET_LOC_QUERY_DST,     66)

dnl Offset of destination IP in location query. 
dnl sizeof(ether) + sizeof(grid_hdr) + offsetof(grid_nbr_encap, dst_ip) 
define(OFFSET_LOC_REPLY_DST,     66)

dnl Offset of destination IP in route probe.
dnl sizeof(ether_ + sizeof(grid_hdr) + offsetof(grid_nbr_encap, dst_ip) 
define(OFFSET_ROUTE_PROBE_DEST,  66)

dnl 
dnl Add this many bytes to the front of IP packets received from the
dnl kernel, to make room for Grid headers.
dnl 
dnl Should be sizeof(grid_hdr) + sizeof(grid_nbr_encap) 
dnl (since KernelTap include Ethernet headers) 
dnl 
define(TUN_INPUT_HEADROOM,       68)') dnl USE_STATIC_OFFSETS 


dnl 
dnl Network-wide IP parameters.
dnl 
define(GRID_ANY_GATEWAY_IP,      18.26.7.254)
define(GRID_ANY_GATEWAY_HEX_IP,  121a07fe) dnl skanky, should be dynamically generated!

dnl The allocated Grid subnet. 
define(GRID_NET1,                18.26.7)
define(GRID_NET1_NETMASK,        24)
dnl The internal Grid NAT network. 
define(GRID_NET2,                10.2)
define(GRID_NET2_NETMASK,        16)

dnl
dnl Node-specific IP, Ethernet, and geographic forwarding parameters.
dnl These need to be defined individually for each node.
dnl

dnl define(POS_LAT, ?)  dnl Latitude and longitude, in integer milliseconds (3,600,000 ms per degree)
dnl define(POS_LON, ?)  
dnl define(ARG_LOC_GOOD, ?) dnl Boolean
dnl define(ARG_LOC_ERR, ?) dnl Metres (unsigned short; not decimal)
dnl define(ARG_LOC_TAG, ?) dnl String tag, no spaces or weird characters

dnl This node's IP address, e.g 18.26.7.96 
dnl define(GRID_IP, ?) 
dnl define(GRID_HEX_IP, ?) 
dnl define(GRID_NETMASK, ?)

dnl This node's Ethernet address, e.g. 00:90:27:e0:23:03 
dnl define(GRID_MAC_ADDR, ?) 

dnl This node's actual wireless device, e.g. eth0.  Used to obtain signal 
dnl strength stats.
dnl define(REAL_NET_DEVICE, ?) 

dnl Device to read and write net packets from and to.  Probably the
dnl same as REAL_NET_DEVICE
dnl define(GRID_NET_DEVICE, ?) 

dnl If defined, geographic forwarding (including location queries/replies) is disabled.
dnl define(DISABLE_GF, 1)

dnl
dnl Gateway configuration.
dnl

dnl Is this node a gateway? 
dnl define(IS_GATEWAY, ?)

dnl These need to be defined individually for each gateway. 
dnl define(GW_IP, ?) 
dnl define(GW_MAC_ADDR, ?)
dnl define(GW_NET_DEVICE, ?)

define(BOGUS_IP,                 10.3.5.4)
define(BOGUS_NETMASK,            255.255.255.255)

dnl
dnl Other special configuration parameters.
dnl
define(ICMP_TIMXCEED,            11)
define(ICMP_TIMXCEED_INTRANS,    0)

dnl Use kernel click queues?
dnl define(USE_KQ, ?)

dnl
dnl End file.
dnl
