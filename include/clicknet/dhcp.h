#ifndef CLICKNET_DHCP_H
#define CLICKNET_DHCP_H

struct click_dhcp {
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
} CLICK_SIZE_PACKED_ATTRIBUTE;


#define ETH_10MB	1 /* htype */
#define ETH_10MB_LEN	6 /* hlen */

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



/* DHCP Option codes: */
#define DHO_PAD				0
#define DHO_SUBNET_MASK			1
#define DHO_TIME_OFFSET			2
#define DHO_ROUTERS			3
#define DHO_TIME_SERVERS		4
#define DHO_NAME_SERVERS		5
#define DHO_DOMAIN_NAME_SERVERS		6
#define DHO_LOG_SERVERS			7
#define DHO_COOKIE_SERVERS		8
#define DHO_LPR_SERVERS			9
#define DHO_IMPRESS_SERVERS		10
#define DHO_RESOURCE_LOCATION_SERVERS	11
#define DHO_HOST_NAME			12
#define DHO_BOOT_SIZE			13
#define DHO_MERIT_DUMP			14
#define DHO_DOMAIN_NAME			15
#define DHO_SWAP_SERVER			16
#define DHO_ROOT_PATH			17
#define DHO_EXTENSIONS_PATH		18
#define DHO_IP_FORWARDING		19
#define DHO_NON_LOCAL_SOURCE_ROUTING	20
#define DHO_POLICY_FILTER		21
#define DHO_MAX_DGRAM_REASSEMBLY	22
#define DHO_DEFAULT_IP_TTL		23
#define DHO_PATH_MTU_AGING_TIMEOUT	24
#define DHO_PATH_MTU_PLATEAU_TABLE	25
#define DHO_INTERFACE_MTU		26
#define DHO_ALL_SUBNETS_LOCAL		27
#define DHO_BROADCAST_ADDRESS		28
#define DHO_PERFORM_MASK_DISCOVERY	29
#define DHO_MASK_SUPPLIER		30
#define DHO_ROUTER_DISCOVERY		31
#define DHO_ROUTER_SOLICITATION_ADDRESS	32
#define DHO_STATIC_ROUTES		33
#define DHO_TRAILER_ENCAPSULATION	34
#define DHO_ARP_CACHE_TIMEOUT		35
#define DHO_IEEE802_3_ENCAPSULATION	36
#define DHO_DEFAULT_TCP_TTL		37
#define DHO_TCP_KEEPALIVE_INTERVAL	38
#define DHO_TCP_KEEPALIVE_GARBAGE	39
#define DHO_NIS_DOMAIN			40
#define DHO_NIS_SERVERS			41
#define DHO_NTP_SERVERS			42
#define DHO_VENDOR_ENCAPSULATED_OPTIONS	43
#define DHO_NETBIOS_NAME_SERVERS	44
#define DHO_NETBIOS_DD_SERVER		45
#define DHO_NETBIOS_NODE_TYPE		46
#define DHO_NETBIOS_SCOPE		47
#define DHO_FONT_SERVERS		48
#define DHO_X_DISPLAY_MANAGER		49
#define DHO_DHCP_REQUESTED_ADDRESS	50
#define DHO_DHCP_LEASE_TIME		51
#define DHO_DHCP_OPTION_OVERLOAD	52
#define DHO_DHCP_MESSAGE_TYPE		53
#define DHO_DHCP_SERVER_IDENTIFIER	54
#define DHO_DHCP_PARAMETER_REQUEST_LIST	55
#define DHO_DHCP_MESSAGE		56
#define DHO_DHCP_MAX_MESSAGE_SIZE	57
#define DHO_DHCP_RENEWAL_TIME		58
#define DHO_DHCP_REBINDING_TIME		59
#define DHO_VENDOR_CLASS_IDENTIFIER	60
#define DHO_DHCP_CLIENT_IDENTIFIER	61
#define DHO_NWIP_DOMAIN_NAME		62
#define DHO_NWIP_SUBOPTIONS		63
#define DHO_USER_CLASS			77
#define DHO_FQDN			81
#define DHO_DHCP_AGENT_OPTIONS		82
#define DHO_SUBNET_SELECTION		118 /* RFC3011! */
#define DHO_END				255

#endif /* CLICKNET_DHCP_H */
