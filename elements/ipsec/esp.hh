#ifndef IPSEC_ESP_HH
#define IPSEC_ESP_HH

/*
 * =c
 * IPsecESPEncap(SPI, BLOCKS)
 * =s encapsulation
 * apply IPSec encapsulation
 * =d
 * 
 * adds IPsec ESP header to packet using the given SPI number. block size is
 * set to BLOCKS number of bytes. the packet will be padded to be multiples of
 * BLOCKS number of bytes.
 *
 * should be used with IPsecDES
 *
 * =a IPsecESPUnencap, IPsecDES */

#include <click/element.hh>
#include <click/glue.hh>
  
struct esp_new { 
  u_int32_t   esp_spi;        /* Security Parameter Index */ 
  u_int32_t   esp_rpl;        /* Sequence Number, Replay Counter */ 
  u_int8_t    esp_iv[8];      /* Data may start already at iv[0]! */ 
};

class Esp : public Element {

public:
  Esp();
  Esp(int spi, int blks);
  ~Esp();
  
  const char *class_name() const	{ return "IPsecESPEncap"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  Esp *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:

  int _blks;
  int _spi;
  unsigned _rpl;
};

#endif



