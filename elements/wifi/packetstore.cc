#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/standard/scheduleinfo.hh>
#include "packetstore.hh"
#include <click/router.hh>
CLICK_DECLS

PacketStore::PacketStore() : _dirty(0), _task(this)
{
}

PacketStore::~PacketStore()
{
	while (_packets.size()) {
		_packets.pop_front();
	}
}

void *
PacketStore::cast(const char *n)
{
    if (strcmp(n, "PacketStore") == 0)
	    return (PacketStore *)this;
    else
	return 0;
}

int
PacketStore::configure(Vector<String> &, ErrorHandler *)
{
	return 0;
}

int
PacketStore::initialize(ErrorHandler *errh)
{
	ScheduleInfo::initialize_task(this, &_task, errh);
	return 0;
}

Packet *
PacketStore::simple_action(Packet *p_in)
{
	store s;
	s.timestamp = p_in->timestamp_anno();
	s.len = WIFI_MIN(p_in->length(), 80);
	memcpy(s.data, p_in->data(), s.len);
	_packets.push_back(s);
	return p_in;
}

bool
PacketStore::run_task(Task *)
{
	return false;
}
enum {H_RESET, H_LEN, H_POP, H_DIRTY};

static String
read_param(Element *e, void *thunk)
{
	PacketStore *td = (PacketStore *)e;
	switch ((uintptr_t) thunk) {
	case H_LEN: return String(td->_packets.size());
	case H_DIRTY: return String(td->_dirty);
	case H_POP: {
		if( !td->_packets.size()) {
			return String();
		}
		PacketStore::store s = td->_packets[0];
		StringAccum sap(s.len*2 + 20);

		sap << s.timestamp << " | ";

		char *buf = sap.data() + sap.length();
		for (int x = 0; x < s.len; x++) {
			sprintf(buf + 2*x, "%02x", s.data[x] & 0xff);
		}
		sap.adjust_length(s.len *2);
		sap << "\n";
		td->_packets.pop_front();
		return sap.take_string();
	}
	default:
		return String();
	}
}

static int
write_param(const String &in_s, Element *e, void *vparam,
	    ErrorHandler *errh)
{
	PacketStore *td = (PacketStore *)e;
	String s = cp_uncomment(in_s);
	switch((intptr_t)vparam) {
	case H_RESET: {
		bool active;
		if (!BoolArg().parse(s, active))
			return errh->error("reset parameter must be boolean");
		if (active) {
			while (td->_packets.size()) {
				td->_packets.pop_front();
			}
		}
	}

	}
	return 0;
}

void
PacketStore::add_handlers()
{
	add_read_handler("length", read_param, H_LEN);
	add_read_handler("pop", read_param, H_POP, Handler::RAW);
	add_read_handler("dirty", read_param, H_DIRTY);
	add_write_handler("reset", write_param, H_RESET);
	add_task_handlers(&_task);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketStore)

