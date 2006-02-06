#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include "packetstore.hh"
CLICK_DECLS

PacketStore::PacketStore()
{
	_active = false;
}

PacketStore::~PacketStore()
{
	while (_packets.size()) {
		_packets[0]->kill();
		_packets.pop_front();
	}
}

void *
PacketStore::cast(const char *n)
{
    if (strcmp(n, "PacketStore") == 0)
	    return (PacketStore *)this;
    else if (strcmp(n, Notifier::EMPTY_NOTIFIER) == 0)
	return static_cast<Notifier *>(&_empty_note);
    else
	return 0;
}

int
PacketStore::configure(Vector<String> &, ErrorHandler *)
{
	_empty_note.initialize(router());
	return 0;
}

void
PacketStore::push(int, Packet *p_in)
{

	WritablePacket *p = Packet::make(p_in->length());
	if (p) {
		memcpy(p->data(), p_in->data(), p_in->length());
		_packets.push_back(p);

		if (!_empty_note.active() && _active)
			_empty_note.wake();
	}
	p_in->kill();
	return;
}


Packet *
PacketStore::pull(int)
{
	if (!_packets.size() || !_active) {
		_empty_note.sleep();
		return 0;
	}

	Packet *p =_packets[_packets.size()-1];
	_packets.pop_back();
	return p;
}

enum {H_ACTIVE, H_LEN, H_POP};

static String
read_param(Element *e, void *thunk)
{
	PacketStore *td = (PacketStore *)e;
	switch ((uintptr_t) thunk) {
	case H_LEN: return String(td->_packets.size()) + "\n";
	case H_ACTIVE: return String(td->_active) + "\n";
	case H_POP: {
		if (!td->_packets.size()) {
			return String("\n");
		}
		Packet *p = td->_packets[0];
		td->_packets.pop_front();
		StringAccum sa(p->length() * 2);
		char *buf = sa.data();
		for (unsigned x = 0; x < p->length(); x++) {
			sprintf(buf + 2*x, "%02x", p->data()[x] & 0xff);
		}
		sa.forward(p->length() * 2);
		sa << "\n";
		p->kill();
		return sa.take_string();
	}
	default:
		return String();
	}
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
	    ErrorHandler *errh)
{
	PacketStore *f = (PacketStore *)e;
	String s = cp_uncomment(in_s);
	switch((int)vparam) {
	case H_ACTIVE: {
		bool active;
		if (!cp_bool(s, &active))
			return errh->error("active parameter must be boolean");
		f->_active = active;
		if (active) {
			f->_empty_note.wake();
		} else {
			f->_empty_note.sleep();
		}
		break;
	}
	}
	return 0;
}

void
PacketStore::add_handlers()
{
	add_read_handler("length", read_param, (void *) H_LEN);
	add_read_handler("pop", read_param, (void *) H_POP);
	add_write_handler("active", write_param, (void *) H_ACTIVE);
	add_read_handler("active", read_param, (void *) H_ACTIVE);
}

#include <click/dequeue.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class DEQueue<struct click_wifi_extra>;
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(PacketStore)

