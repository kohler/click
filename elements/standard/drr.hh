#ifndef CLICK_DRR_HH
#define CLICK_DRR_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * DRRSched
 * =s packet scheduling
 * pulls from inputs with deficit round robin scheduling
 * =io
 * One output, zero or more inputs
 * =d
 * Schedules packets with deficit round robin scheduling, from
 * Shreedhar and Varghese's SIGCOMM 1995 paper "Efficient Fair
 * Queuing using Deficit Round Robin."
 *
 * =a PrioSched, StrideSched, RoundRobinSched
 */

class DRRSched : public Element { public:
  
  DRRSched();
  ~DRRSched();
  
  const char *class_name() const		{ return "DRRSched"; }
  const char *processing() const		{ return PULL; }
  DRRSched *clone() const			{ return new DRRSched; }
  
  void notify_ninputs(int);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  
  Packet *pull(int port);

 private:

  int _quantum;   // Number of bytes to send per round.

  Packet **_head; // First packet from each queue.
  unsigned *_deficit;  // Each queue's deficit.
  int _next;      // Next input to consider.
  
};

CLICK_ENDDECLS
#endif
