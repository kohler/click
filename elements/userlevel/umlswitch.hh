// -*- mode: c++; c-basic-offset: 2 -*-
#ifndef CLICK_UMLSWITCH_HH
#define CLICK_UMLSWITCH_HH
#include <click/element.hh>
#include <click/string.hh>
#include <sys/un.h>
#include "socket.hh"
CLICK_DECLS

/*
=c

UMLSwitch([FILENAME])

=s devices

Connects to a UML switch daemon

=d

Transports packets to and from a User Mode Linux switch daemon
instance. Packets do not flow through UMLSwitch elements (i.e.,
UMLSwitch is an "x/y" element). Instead, input packets are sent to the
UML switch, and packets received from the UML switch are emitted on
the output.

If FILENAME is not specified, "/tmp/uml.ctl" will be used as the path
to the UML control socket.

=a Socket */

#define SWITCH_VERSION 3

enum request_type { REQ_NEW_CONTROL };

#define SWITCH_MAGIC 0xfeedface

struct request_v3 {
	uint32_t magic;
	uint32_t version;
	enum request_type type;
	struct sockaddr_un sock;
};

class UMLSwitch : public Socket { public:

  UMLSwitch() : _ctl_path("/tmp/uml.ctl") {};
  ~UMLSwitch() {};

  const char *class_name() const	{ return "UMLSwitch"; }
  const char *processing() const	{ return "l/h"; }
  const char *flow_code() const		{ return "x/y"; }

  int configure(Vector<String> &conf, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;

private:
  struct sockaddr_un _ctl_addr;
  struct sockaddr_un _local_addr;
  struct sockaddr_un _data_addr;

  String _ctl_path;		// path to uml_switch control socket
  int _control;			// uml_switch control socket
  int _snaplen;			// maximum received packet length

  int initialize_socket_error(ErrorHandler *, const char *);

};

CLICK_ENDDECLS
#endif
