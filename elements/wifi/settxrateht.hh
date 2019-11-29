#ifndef CLICK_SETTXRATEHT_HH
#define CLICK_SETTXRATEHT_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
SetTXRateHT([I<KEYWORDS>])

=s Wifi

Sets the bit-rate for a packet.

=d

Sets the Wifi TXRate Annotation on a packet.

Regular Arguments:
=over 8

=item MCS
Unsigned integer. MCS index.

=back 8

=h rate read/write
Same as RATE Argument

=a AutoRateFallback, MadwifiRate, ProbeRate, ExtraEncap
*/

class SetTXRateHT: public Element {
public:

	SetTXRateHT() CLICK_COLD;
	~SetTXRateHT() CLICK_COLD;

	const char *class_name() const { return "SetTXRateHT"; }
	const char *port_count() const { return PORTS_1_1; }
	const char *processing() const { return AGNOSTIC; }

	int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
	bool can_live_reconfigure() const { return true; }

	Packet *simple_action(Packet *);

	void add_handlers() CLICK_COLD;

	static String read_handler(Element *e, void *) CLICK_COLD;
	static int write_handler(const String &, Element *, void *, ErrorHandler *);

private:

	int _mcs;
	int _mcs1;
	int _mcs2;
	int _mcs3;

	unsigned _max_tries;
	unsigned _max_tries1;
	unsigned _max_tries2;
	unsigned _max_tries3;

	uint16_t _et;
	unsigned _offset;
	bool _sgi;
	bool _bw_40;

};

CLICK_ENDDECLS
#endif
