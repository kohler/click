#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "packetshaper.hh"

PacketShaper::PacketShaper()
{
}

PacketShaper *
PacketShaper::clone() const
{
  return new PacketShaper;
}

Packet *
PacketShaper::pull(int)
{
  _rate.update_time();
  
  int r = _rate.average();
  if (r >= _meter1) {
    if (_puller1)
      _puller1->schedule_tail();
    else {
      int n = _pullers.size();
      for (int i = 0; i < n; i++)
	_pullers[i]->schedule_tail();
    }
    return 0;
  } else {
    Packet *p = input(0).pull();
    if (p) _rate.update_now(1);	// packets, not bytes
    return p;
  }
}

EXPORT_ELEMENT(PacketShaper)
