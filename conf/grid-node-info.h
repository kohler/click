// grid-node-info.h

// Grid physical device
#define NET_DEVICE wvlan0

// MAC address of NET_DEVICE
#define MAC_ADDR 00:60:1D:23:A7:A6

// tunnel device for user-level grid configuration
#define TUN_DEVICE tap

// this node's Grid address information
#define GRID_IP 18.26.7.2
#define GRID_NETMASK 255.255.255.0

// Grid network number in Click hex notation, with IP source address
// offset (typically 16) e.g. 16/121a07
#define GRID_NET_HEX 16/121a07 // 18.26.7.*

// Grid network gateway
#define GRID_GW 18.26.7.1

// immediate neighbor table entry timeout
#define NBR_TIMEOUT 2000 // msecs

// Hello broadcast paramters
#define HELLO_PERIOD 500 // msecs
#define HELLO_JITTER 100 // msecs

// Grid ethernet protocol number in Click hex notation with ethernet
// protocol offset (typically 12) e.g. 12/7FFF
#define GRID_ETH_PROTO 12/7FFF
