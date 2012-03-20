// -*- c-basic-offset: 4 -*-
#ifndef CLICK_DRR_HH
#define CLICK_DRR_HH
#include <click/element.hh>
#include <click/notifier.hh>
CLICK_DECLS

/*
 * =c
 * DRRSched([QUANTUM])
 * =s scheduling
 * pulls from inputs with deficit round robin scheduling
 * =io
 * one output, zero or more inputs
 * =d
 * Schedules packets with deficit round robin scheduling, from
 * Shreedhar and Varghese's SIGCOMM 1995 paper "Efficient Fair
 * Queuing using Deficit Round Robin."
 *
 * The inputs usually come from Queues or other pull schedulers.
 * DRRSched uses notification to avoid pulling from empty inputs.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item QUANTUM
 *
 * Integer. Quantum (in bytes) added to each round. Defaults to 500.
 *
 * =back
 *
 * =n
 *
 * DRRSched is a notifier signal, active iff any of the upstream notifiers
 * are active.
 *
 * =a PrioSched, StrideSched, RoundRobinSched
 */

class DRRSched : public Element { public:

    DRRSched();

    const char *class_name() const		{ return "DRRSched"; }
    const char *port_count() const		{ return "-/1"; }
    const char *processing() const		{ return PULL; }
    const char *flags() const			{ return "S0"; }
    void *cast(const char *);

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    Packet *pull(int port);

  private:

    struct portinfo {
	Packet *head;
	unsigned deficit;
	NotifierSignal signal;
    };

    int _quantum;   // Number of bytes to send per round.
    portinfo *_pi;
    Notifier _notifier;
    int _next;      // Next input to consider.

};

CLICK_ENDDECLS
#endif
