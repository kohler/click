#ifndef IPSEC_ESP_HH
#define IPSEC_ESP_HH

/*
 * =c
 * IPsecESPEncap(SPI, BLOCKS [, SHA1])
 * =s encapsulation
 * apply IPSec encapsulation
 * =d
 * 
 * Adds IPsec ESP header to packet. assign SPI as the security parameters
 * index. Block size is set to BLOCKS number of bytes. The packet will be
 * padded to be multiples of BLOCKS number of bytes. Padding uses the default
 * padding scheme specified in RFC 2406: pad[0] = 1, pad[1] = 2, pad[2] = 3,
 * etc.
 *
 * The ESP header added to the packet includes the 32 bit SPI, 32 bit replay
 * counter, and 64 bit Integrity Vector (IV). The IV is not set, and an
 * element that does encryption, such as IPsecDES, is expected to set the IV.
 * Thus, IPsecESPEncap is usually followed by such an element.
 *
 * If SHA1 is true (false by default), SHA1 hash of the payload, including the
 * IV, will be computed. First 96 bits of the SHA1 hash will be appended to
 * the payload, after padding.
 *
 * =a IPsecESPUnencap, IPsecDES 
 */

#include <click/element.hh>
#include <click/glue.hh>
  
struct esp_new { 
  u_int32_t   esp_spi;        /* security parameter index */ 
  u_int32_t   esp_rpl;        /* sequence number, replay counter */ 
  u_int8_t    esp_iv[8];      /* data may start already at iv[0]! */ 
};

class IPsecESPEncap : public Element {

public:
  IPsecESPEncap();
  IPsecESPEncap(int spi, int blks);
  ~IPsecESPEncap();
  
  const char *class_name() const	{ return "IPsecESPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  IPsecESPEncap *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:

  bool _sha1;
  int _blks;
  int _spi;
  unsigned _rpl;
};

#endif



