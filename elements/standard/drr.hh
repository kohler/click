#ifndef DRR_HH
#define DRR_HH
#include <click/element.hh>

/*
 * =c
 * DRR
 * =s packet scheduling
 * pulls from inputs with deficit round robin scheduling
 * =io
 * One output, zero or more inputs
 * =d
 * Schedules packets with deficit round robin scheduling, from
 * Shreedhar and Varghese's SIGCOMM 1995 paper "Efficient Fair
 * Queuing using Deficit Round Robin."
 *
 * =a PrioSched, StrideSched, RoundRobinSwitch, RoundRobinSched
 */

class DRR : public Element {
  
 public:
  
  DRR();
  ~DRR();
  
  const char *class_name() const		{ return "DRR"; }
  const char *processing() const		{ return PULL; }
  void notify_ninputs(int);
  
  DRR *clone() const			{ return new DRR; }
  
  Packet *pull(int port);

 private:

  int _quantum;   // Number of bytes to send per round.

  Packet **_head; // First packet from each queue.
  unsigned *_deficit;  // Each queue's deficit.
  int _next;      // Next input to consider.
};

#endif DRR_HH
