#ifndef IPSEC_ESP_HH
#define IPSEC_ESP_HH

/*
 * IPsec_ESP
 * 
 * Encapsulate a packet using ESP per RFC2406.
 */


#include "element.hh"
#include "glue.hh"

struct esp_new
{
    u_int32_t   esp_spi;        /* Security Parameter Index */
    u_int32_t   esp_rpl;        /* Sequence Number, Replay Counter */
    u_int8_t    esp_iv[8];      /* Data may start already at iv[0]! */
};


class Address;

class Esp : public Element {
public:
  Esp();
  Esp(int spi, int blks);
  ~Esp();
  
  const char *class_name() const		{ return "IPsecESPEncap"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  Esp *clone() const;
  int configure(const String &, Router *, ErrorHandler *);
  int initialize(Router *, ErrorHandler *);

  Packet *simple_action(Packet *);
  
private:

  int _blks; // Number of blocks required by encryption algorithm

  int _spi; // Security Parameter Index.
  int _rpl; // sequence number
 


};

#endif



