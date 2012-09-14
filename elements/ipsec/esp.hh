#ifndef CLICK_IPSEC_ESP_HH
#define CLICK_IPSEC_ESP_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * IPsecESPEncap(SPI)
 * =s ipsec
 * apply IPSec encapsulation
 * =d
 *
 * Adds IPsec ESP header to packet. assign SPI as the security parameters
 * index. Block size is set to 8 bytes. The packet will be padded to be
 * multiples of 8 bytes. Padding uses the default padding scheme specified in
 * RFC 2406: pad[0] = 1, pad[1] = 2, pad[2] = 3, etc.
 *
 * The ESP header added to the packet includes the 32 bit SPI, 32 bit replay
 * counter, and 64 bit Integrity Vector (IV).
 *
 * =a IPsecESPUnencap, IPsecAuthSHA1, IPsecDES
 */

struct esp_new {
  uint32_t esp_spi;
  uint32_t esp_rpl;
  uint8_t esp_iv[8];
};

class IPsecESPEncap : public Element {

public:
  IPsecESPEncap() CLICK_COLD;
  IPsecESPEncap(int spi);
  ~IPsecESPEncap() CLICK_COLD;

  const char *class_name() const	{ return "IPsecESPEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

private:

  enum { BLKS = 8 };
};

CLICK_ENDDECLS
#endif
