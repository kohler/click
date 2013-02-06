#ifndef SIMCLICK_H
#define SIMCLICK_H
/*
 * simclick.h
 *
 * API for sending packets to Click. Mostly intended for use
 * by a network simulator which wants to use Click to do routing.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packet types used - generally going to be ethernet, but could
 * possibly be something else I suppose...
 */
#define SIMCLICK_PTYPE_UNKNOWN 0
#define SIMCLICK_PTYPE_ETHER 1
#define SIMCLICK_PTYPE_IP 2

/*
 * Not a whole lot to this. We have to create a click router object
 * and also send packets and trigger events.
 */

/*
 * This contains per packet data we need to preserve when the packet gets
 * dragged through click. Heavily biased towards ns-2 right now.
 */
typedef struct {
    int id;			/* Simulator ID number for the packet */
    int fid;			/* Simulator flow ID number for the packet */
    int simtype;		/* Original simulator packet type - useful
				 * for morphing between raw and simulator
				 * packet types */
} simclick_simpacketinfo;


/*
 * Opaque handles for the sim and click instances
 */
typedef struct simclick_node {
    void *clickinfo;
    struct timeval curtime;
} simclick_node_t;

int simclick_click_create(simclick_node_t *sim, const char *router_file);

int simclick_click_send(simclick_node_t *sim,
			int ifid,int type,const unsigned char* data,int len,
			simclick_simpacketinfo* pinfo);
int simclick_sim_send(simclick_node_t *sim,
		      int ifid,int type, const unsigned char* data,int len,
		      simclick_simpacketinfo*);

void simclick_click_run(simclick_node_t *sim);

void simclick_click_kill(simclick_node_t *sim);

/*
 * simclick_click_read_handler will allocate a buffer of adequate length
 * to receive the handler information. This buffer must be freed
 * by the caller. If a non-null value for the "memalloc" parameter
 * is passed in, simclick_click_read_handler will use that function
 * to allocate the memory. If there's a null value there, "malloc" will
 * be used by default. The "memparam" parameter is a caller-specified
 * value which will be passed back to the memory allocation function.
 */
typedef void* (*SIMCLICK_MEM_ALLOC)(size_t,void*);
char* simclick_click_read_handler(simclick_node_t *sim,
				  const char* elementname,
				  const char* handlername,
				  SIMCLICK_MEM_ALLOC memalloc,
				  void* memparam);

int simclick_click_write_handler(simclick_node_t *sim,
				 const char* elemname, const char* handlername,
				 const char* writestring);

/*
 * We also provide a gettimeofday substitute which utilizes the
 * state info passed to us by the simulator.
 */
int simclick_gettimeofday(struct timeval* tv);

/*
 * The simulated system also has to provide a few services to click,
 * notably some way of injecting packets back into the system,
 * mapping interface names to id numbers, and arranging for click
 * to execute at a specified time in the future.
 * We implement
 */
#define SIMCLICK_VERSION		0  // none
#define SIMCLICK_SUPPORTS		1  // int call
#define SIMCLICK_IFID_FROM_NAME		2  // const char *ifname
#define SIMCLICK_IPADDR_FROM_NAME	3  // const char *ifname, char *buf, int len
#define SIMCLICK_MACADDR_FROM_NAME	4  // const char *ifname, char *buf, int len
#define SIMCLICK_SCHEDULE		5  // struct timeval *when
#define SIMCLICK_GET_NODE_NAME		6  // char *buf, int len
#define SIMCLICK_IF_READY		7  // int ifid
#define SIMCLICK_TRACE			8  // const char *event
#define SIMCLICK_GET_NODE_ID		9  // none
#define SIMCLICK_GET_NEXT_PKT_ID	10 // none
#define SIMCLICK_CHANGE_CHANNEL		11 // int ifid, int channelid
#define SIMCLICK_IF_PROMISC		12 // int ifid
#define SIMCLICK_IPPREFIX_FROM_NAME	13 // const char *ifname, char *buf, int len
#define SIMCLICK_GET_RANDOM_INT		14 // uint32_t *result, uint32_t max
#define SIMCLICK_GET_DEFINES		15 // char *buf, size_t *size

int simclick_sim_command(simclick_node_t *sim, int cmd, ...);
int simclick_click_command(simclick_node_t *sim, int cmd, ...);

#ifdef __cplusplus
}
#endif
#endif
