#ifndef CLICKNET_DHCP_H
#define CLICKNET_DHCP_H

/* DHCP message OP code */
#define DHCP_BOOTREQUEST	1
#define DHCP_BOOTREPLY		2

/* DHCP Client State*/
enum dhcp_client_state {
	DHCP_CLIENT_INIT_STATE = 1,
	DHCP_CLIENT_SELECTING_STATE,
	DHCP_CLIENT_REQUESTING_STATE,
	DHCP_CLIENT_INIT_REBOOT,
	DHCP_CLIENT_REBOOTING,
	DHCP_CLIENT_BOUND,
	DHCP_CLIENT_RENEWING,
	DHCP_CLIENT_REBINDING
};


/* DHCP message type */
#define DHCP_DISCOVER           1
#define DHCP_OFFER              2
#define DHCP_REQUEST            3
#define DHCP_DECLINE            4
#define DHCP_ACK                5
#define DHCP_NACK               6
#define DHCP_RELEASE            7
#define DHCP_INFORM             8

#define DHCP_MAGIC 0x63538263

CLICK_SIZE_PACKED_STRUCTURE(
struct click_dhcp {,
	uint8_t  op;           /* message type */
	uint8_t  htype;        /* hardware address type */
	uint8_t  hlen;         /* hardware address length */
	uint8_t  hops;         /* should be zero in client's message */
	uint32_t xid;          /* transaction id */
	uint16_t secs;         /* elapsed time in sec. from trying to boot */
	uint16_t flags;
	uint32_t ciaddr;       /* (previously allocated) client IP address */
	uint32_t yiaddr;       /* 'your' client IP address */
	uint32_t siaddr;       /* should be zero in client's messages */
	uint32_t giaddr;       /* should be zero in client's messages */
	uint8_t  chaddr[16];   /* client's hardware address */
	uint8_t  sname[64];    /* server host name, null terminated string */
	uint8_t  file[128];    /* boot file name, null terminated string */
	uint32_t magic;        /* magic cookie */
	uint8_t  options[312]; /* message options */
});



#endif /* CLICKNET_DHCP_H */
