#ifndef SIMCLICK_H
#define SIMCLICK_H

/*
 *
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
 * This encapsulates the state info that click needs from the simulator.
 * Right now this is just a timestamp, but we might need to send over
 * more stuff at some point. This is essentially a big old blob of globals,
 * so watch out that it doesn't turn into a monstrosity.
 */
typedef struct {
  struct timeval curtime;
} simclick_simstate;


/*
 * This contains per packet data we need to preserve when the packet gets
 * dragged through click. Heavily biased towards ns-2 right now.
 */
typedef struct {
  /*
   * Simulator ID number for the packet
   */
  int id;

  /*
   * Simulator flow ID number for the packet
   */
  int fid;

  /*
   * Original simulator packet type - useful
   * for morphing between raw and simulator packet types
   */
  int simtype;
} simclick_simpacketinfo;

/*
 * Opaque handles for the sim and click instances
 */
typedef void* simclick_click;
typedef void* simclick_sim;

simclick_click simclick_click_create(simclick_sim siminst,
				     const char* router_file,
				     simclick_simstate* startstate);
int simclick_click_send(simclick_click clickinst,simclick_simstate* state,
			int ifid,int type,const unsigned char* data,int len,
			simclick_simpacketinfo* pinfo);

void simclick_click_run(simclick_click clickinst,simclick_simstate* state);

void simclick_click_kill(simclick_click clickinst,simclick_simstate* state);

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
char* simclick_click_read_handler(simclick_click clickinst,
				  const char* elementname,
				  const char* handlername,
				  SIMCLICK_MEM_ALLOC memalloc,
				  void* memparam);

int simclick_click_write_handler(simclick_click clickinst,
				 const char* elemname, const char* handlername,
				 const char* writestring);

/*
 * The simulated system also has to provide a few services to click,
 * notably some way of injecting packets back into the system,
 * mapping interface names to id numbers, and arranging for click
 * to execute at a specified time in the future.
 */
int simclick_sim_ifid_from_name(simclick_sim siminst,const char* ifname);
void simclick_sim_ipaddr_from_name(simclick_sim siminst,const char* ifname,
				   char* buf,int len);
void simclick_sim_macaddr_from_name(simclick_sim siminst,const char* ifname,
				    char* buf,int len);
int simclick_sim_send_to_if(simclick_sim siminst,simclick_click clickinst,
			    int ifid,int type, const unsigned char* data,
			    int len,simclick_simpacketinfo*);
int simclick_sim_schedule(simclick_sim siminst,simclick_click clickinst,
			  struct timeval* when);
void simclick_sim_get_node_name(simclick_sim siminst,char* buf,int len);

int simclick_sim_if_ready(simclick_sim siminst,simclick_click clickinst,
			  int ifid);

/*
 * We also provide a gettimeofday substitute which utilizes the 
 * state info passed to us by the simulator.
 */
int simclick_gettimeofday(struct timeval* tv);

#ifdef __cplusplus
}
#endif

#endif
