/*
 * A "simulator" testbed for simclick library. Note that this could
 * have been just a normal old C file except for the fact that I
 * decided I wanted to use an existing template heap class, and
 * it seemed like as good a time as any to exercise my rusty
 * STL skills.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stl.h>
#include <hash_map.h>
#include "CUT_BinHeap.h"
#include "click/simclick.h"

const int TESTSIM_IFID_KERNELTAP=0;
const int TESTSIM_IFID_FIRSTIF=1;

class Simulator {
public:
  Simulator();
  virtual ~Simulator(); 
  
  int nextevent();
  struct timeval gettime() { return cursimtime_; };

  class SimTime : public timeval {
  public:
    SimTime() {tv_sec = 0, tv_usec = 0;};
    SimTime(long sec,long usec) { tv_sec = sec, tv_usec = usec; };
    SimTime(struct timeval tv) {tv_sec = tv.tv_sec,tv_usec = tv.tv_usec;};

    bool operator<(const struct timeval& tv) const { 
      return ((tv_sec < tv.tv_sec) ||
	      ((tv_sec == tv.tv_sec) && (tv_usec < tv.tv_usec)));
    }
    bool operator==(const struct timeval& tv) const {
      return ((tv_sec == tv.tv_sec) && (tv_usec == tv.tv_usec));
    }
    bool operator>(const struct timeval& tv) const {
      return ((tv_sec > tv.tv_sec) ||
	      ((tv_usec == tv.tv_usec) && (tv_usec > tv.tv_usec)));
    }
    SimTime operator+(SimTime rhs) const {
      SimTime result;
      result.tv_sec = tv_sec + rhs.tv_sec + (tv_usec + rhs.tv_usec)/1000000;
      result.tv_usec = (tv_usec + rhs.tv_usec) % 1000000;
      return result;
    }
  };

  // Base class for all simulator events
  class SimEvent {
  public:
    SimEvent() {};
    virtual ~SimEvent();  
    virtual int go(SimTime* when) = 0;
  };
protected:
  SimTime cursimtime_;
  typedef CUT_BinHeap< SimTime,SimEvent*,less_equal<SimTime> > SimBinHeap;
  SimBinHeap eventheap_;
};

Simulator::Simulator() {
}

Simulator::~Simulator() {
}

int
Simulator::nextevent() {
  SimBinHeap::Pix pix = eventheap_.find_top();
  SimTime etime = eventheap_.key(pix);
  SimEvent* event = eventheap_.data(pix);
  cursimtime_ = etime;
  event->go(&etime);
  eventheap_.deq();
  delete event;
  return eventheap_.size();
}

Simulator::SimEvent::~SimEvent() {}

class TestClickSimulator : public Simulator {
public:
  TestClickSimulator();
  virtual ~TestClickSimulator();

  int add_node(char* clickfile);
  void handle_packet_from_click(simclick_click node,int ifid,int ptype,
				const unsigned char* data,int len);
  void handle_schedule_from_click(simclick_click node,struct timeval* when);
  void add_lan_entry(simclick_click node,int ifid,int lanid);
  void add_lan_entry(int nodenum,int ifid,int lanid);
  simclick_click get_node(int nodenum);

  class PacketEvent : public Simulator::SimEvent {
  public:
    PacketEvent();
    virtual ~PacketEvent();
    virtual int go(SimTime* when);

    simclick_click clickinst_;
    simclick_sim siminst_;
    int ifid_;
    unsigned char* data_;
    int len_;
    int ptype_;
  };

  class ScheduledEvent : public Simulator::SimEvent {
  public:
    ScheduledEvent();
    virtual ~ScheduledEvent();
    virtual int go(SimTime* when);

    simclick_click clickinst_;
    simclick_sim siminst_;
  };
protected:
  simclick_simstate clickstate_;
  struct netif {
    netif(simclick_click n,int i) { node=n,ifid=i; }
    simclick_click node;
    int ifid;
    bool operator==(const netif& rhs) const {
      return((node == rhs.node) && (ifid == rhs.ifid));
    }
  };
  struct hash<netif> {
    size_t operator()(const netif& x) const {
      // Probably not much of a hash function, but it
      // should do for now.
      return ((int)x.node + x.ifid);
    }
  };

  vector<simclick_click> clickrouters_;
  hash_map<netif,int> netiftolanid_;
  hash_map< int,vector<netif> > lanidtonetif_;
};

TestClickSimulator::TestClickSimulator() {
  memset(&clickstate_,0,sizeof(simclick_simstate));
}

TestClickSimulator::~TestClickSimulator() {
}

int
TestClickSimulator::add_node(char* clickfile) {
  int result = -1;
  simclick_click newnode = simclick_click_create((simclick_sim)this,
						 clickfile,&clickstate_);
  if (newnode) {
    clickrouters_.push_back(newnode);
    result = clickrouters_.size();
  }
  
  return result;
}

void
TestClickSimulator::handle_packet_from_click(simclick_click node,int ifid,
					     int ptype,
					     const unsigned char* data,int len)
{
  // Use the node-ifid combo to find the lanid, and then use the lanid
  // to get the list of node-ifid combos attached to it.
  netif fromif(node,ifid);

  int onlan = netiftolanid_[fromif];
  int i = 0;
  int n = lanidtonetif_[onlan].size();

  for (i=0;i<n;i++) {
    // Load up the simulator queue with packets to send
    SimTime newtime = cursimtime_;
    // Assume overhead of 0.1ms (pulled out of air)
    newtime.tv_usec += 100;
    PacketEvent* pkt = new PacketEvent();
    pkt->clickinst_ = lanidtonetif_[onlan][i].node;
    pkt->siminst_ = (simclick_sim*)this;
    pkt->ifid_ = lanidtonetif_[onlan][i].ifid;
    pkt->data_ = new unsigned char[len];
    pkt->len_ = len;
    pkt->ptype_ = ptype;
    memcpy(pkt->data_,data,len);
    eventheap_.insert(newtime,pkt);
    fprintf(stderr,"Added send packet event: clickinst: %d ifid: %d time: %d %d\n",(int)(pkt->clickinst_),pkt->ifid_,newtime.tv_sec,newtime.tv_usec);
  }
}

void
TestClickSimulator::handle_schedule_from_click(simclick_click node,
					       struct timeval* when) {
  // Stuff a click trigger event into the simulator queue
  ScheduledEvent* sevent = new ScheduledEvent;
  SimTime newtime(*when);
  sevent->clickinst_ = node;
  sevent->siminst_ = (simclick_sim*)this;
  eventheap_.insert(newtime,sevent);
}

simclick_click
TestClickSimulator::get_node(int nodenum) {
  return clickrouters_[nodenum];
}

void
TestClickSimulator::add_lan_entry(int nodenum,int ifid,int lanid) {
  add_lan_entry(clickrouters_[nodenum],ifid,lanid);
}

void
TestClickSimulator::add_lan_entry(simclick_click node,int ifid,int lanid) {
  netif newif(node,ifid);
  netiftolanid_[newif] = lanid;
  lanidtonetif_[lanid].push_back(newif);
}

int
TestClickSimulator::PacketEvent::go(SimTime* when) {
  int result = 0;
  struct simclick_simpacketinfo pinfo;
  simclick_simstate curstate;

  curstate.curtime.tv_sec = when->tv_sec;
  curstate.curtime.tv_usec = when->tv_usec;
  pinfo.id = 2;
  pinfo.fid =2;
  fprintf(stderr,"Dispatching send packet event: clickinst: %d ifid: %d time: %d %d pid %d fid %d\n",(int)clickinst_,ifid_,when->tv_sec,when->tv_usec,pinfo.id,pinfo.fid);
  simclick_click_send(clickinst_,&curstate,ifid_,ptype_,data_,len_,&pinfo);
  
  return result;
}

TestClickSimulator::PacketEvent::PacketEvent() {
  clickinst_ = 0;
  ifid_ = -1;
  ptype_ = -1;
  data_ = 0;
  len_  = 0;
}

TestClickSimulator::PacketEvent::~PacketEvent() {
  if (data_) {
    delete[] data_;
  }
}

TestClickSimulator::ScheduledEvent::ScheduledEvent() {
}

TestClickSimulator::ScheduledEvent::~ScheduledEvent() {
}

int
TestClickSimulator::ScheduledEvent::go(SimTime* when) {
  int result = 0;

  simclick_simstate curstate;
  curstate.curtime.tv_sec = when->tv_sec;
  curstate.curtime.tv_usec = when->tv_usec;
  simclick_click_run(clickinst_,&curstate);
  
  return result;
}

static TestClickSimulator thesim;

int main(int argc,char** argv) {
  int result = 0;
  int i = 0;
  const int numclicks = 3;
  const unsigned char mypacket[] = "this is my bogus packet\n";

  printf("Testing the simclick interface...\n");

  char* scripts[numclicks] = {"../conf/test-simclick-udpgen.click", \
				"../conf/test-simclick-device.click", \
				"../conf/test-simclick-device.click"};

  for (i=0;i<numclicks;i++) {
    printf("Creating a SimClick click instance with %s\n",scripts[i]);
    thesim.add_node(scripts[i]);
  }

  // eth0 of the traffic source (node 0) goes on the same 
  // lan as eth0 of node 1
  thesim.add_lan_entry(0,1,1);
  thesim.add_lan_entry(1,1,1);

  // Put eth1 of node 0 and eth0 of node 1 on the same lan
  thesim.add_lan_entry(1,2,2);
  thesim.add_lan_entry(2,1,2);

  // Put eth1 of node1 and eth0 of node 1 on the same lan
  //thesim.add_lan_entry(1,1,3);
  //thesim.add_lan_entry(2,2,3);

  simclick_simstate startstate;
  startstate.curtime.tv_sec = 0;
  startstate.curtime.tv_usec = 0;
  // Send a packet out to eth0 of node 0.
  //printf("About to send out test packet on node 0, eth0...\n");
  //simclick_click_send(thesim.get_node(0),&startstate,TESTSIM_IFID_FIRSTIF,
  //		      SIMCLICK_PTYPE_ETHER,mypacket,sizeof(mypacket));

  // Prime the simulator pump
  Simulator::SimTime now;
  int endtime = 60;
  Simulator::SimTime tick(0,10000);

  printf("About to start pumping the simulator...\n");
  while (thesim.gettime().tv_sec < endtime) {
    // Insert a clock tick, then run the simulator for a step.
    thesim.handle_schedule_from_click(thesim.get_node(0),&now);
    thesim.nextevent();
    now = now + tick;
  }

  printf("Done.\n");

  return result;
}

int
simclick_sim_ifid_from_name(simclick_sim siminst,const char* ifname) {
  int ifid = -1;
  char* devname = NULL;

  fprintf(stderr,"Woo! Got a request for %s\n",ifname);
  /*
   * Provide a mapping between a textual interface name
   * and the id numbers used. This is so that click scripts
   * can still refer to an interface as, say, /dev/eth0.
   */
  if (strstr(ifname,"tap") || strstr(ifname,"tun")) {
    /*
     * A tapX or tunX interface goes to and from the kernel -
     * always TESTSIM_IFID_KERNELTAP
     */
    ifid = TESTSIM_IFID_KERNELTAP;
  }
  else if ((devname = strstr(ifname,"eth"))) {
    /*
     * Anything with an "eth" followed by a number is a
     * regular interface. Add the number to TESTSIM_IFID_FIRSTIF
     * to get the handle.
     */
    while (*devname && !isdigit(*devname)) {
      devname++;
    }
    if (*devname) {
      ifid = atoi(devname) + TESTSIM_IFID_FIRSTIF;
    }
  }
  fprintf(stderr,"Corresponds to simdev number %d\n",ifid);
  return ifid;
}

void
simclick_sim_ipaddr_from_name(simclick_sim siminst,const char* ifname,
			      char* buf,int len) {
}

void
simclick_sim_macaddr_from_name(simclick_sim siminst,const char* ifname,
			       char* buf,int len) {
}

int
simclick_sim_send_to_if(simclick_sim siminst,simclick_click clickinst,
			int ifid,int type,const unsigned char* data,int len,
			simclick_simpacketinfo* pinfo) {
  int result = 0;
  // XXX print pinfo data
  fprintf(stderr,"Packet incoming on clickinst %d ifid %d\n",(int)clickinst,ifid);
  thesim.handle_packet_from_click(clickinst,ifid,type,data,len);
  fprintf(stderr,"Exiting simclick_send_to_if...\n");
  return result;
}

int
simclick_sim_schedule(simclick_sim siminst,simclick_click clickinst,
		      struct timeval* when) {
  int result = 0;
  thesim.handle_schedule_from_click(clickinst,when);
  return result;
}
