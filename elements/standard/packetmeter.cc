#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "packetmeter.hh"

PacketMeter::PacketMeter()
{
}

PacketMeter *
PacketMeter::clone() const
{
  return new PacketMeter;
}

void
PacketMeter::push(int, Packet *p)
{
  _rate.update(1);		// packets, not bytes

  int r = _rate.average();
  if (_nmeters < 2) {
    int n = (r >= _meter1);
    output(n).push(p);
  } else {
    int *meters = _meters;
    int nmeters = _nmeters;
    for (int i = 0; i < nmeters; i++)
      if (r < meters[i]) {
	output(i).push(p);
	return;
      }
    output(nmeters).push(p);
  }
}

EXPORT_ELEMENT(PacketMeter)
