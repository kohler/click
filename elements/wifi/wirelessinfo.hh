#ifndef CLICK_WIRELESSINFO_HH
#define CLICK_WIRELESSINFO_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
WirelessInfo([I<keywords CHANNEL, SSID, BSSID, INTERVAL])

=s Wifi, Wireless AccessPoint, Wireless Station, information

Tracks 802.11 configuration information (ssid, etc).

=d 

Tracks 80.211 configuration. Similar to what you can specify with 'iwconfig' in linux. Does not process packets.

Keyword arguments are:

=over 8

=item CHANNEL

Argument is an integer for what channel this configuration is operating on.

=item SSID

Argument is a string.

=item BSSID

Argument is an Ethernet Address. 

=item INTERVAL

Beacon interval for access points, in milliseconds.

=back 

=h ssid read/write

Same as 'SSID'.

=h channel read/write

Same as 'CHANNEL'.

=h bssid read/write

Same as 'BSSID'.

=h interval read/write

Same as 'INTERVAL'.

*/


class WirelessInfo : public Element { public:
  
  WirelessInfo();
  ~WirelessInfo();
  
  const char *class_name() const		{ return "WirelessInfo"; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();
  
  static String read_param(Element *, void *);
  static int write_param(const String &in_s, Element *, void *, ErrorHandler *);

  String _ssid;
  EtherAddress _bssid;
  int _channel;
  int _interval;
};

CLICK_ENDDECLS
#endif
