#ifndef IPSEC_AUTH_SHA1_HH
#define IPSEC_AUTH_SHA1_HH

/*
 * =c
 * IPsecAuthSHA1(COMPUTE/VERIFY)
 * =s Authentication
 * verify SHA1 authentication digest.
 * =d
 * 
 * If first argument is 0, computes SHA1 authentication digest for ESP packet
 * per RFC 2404, 2406. If first argument is 1, verify SHA1 digest and remove
 * authentication bits.
 *
 * =a IPsecESPEncap, IPsecDES 
 */

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/glue.hh>
  
class IPsecAuthSHA1 : public Element {

public:
  IPsecAuthSHA1();
  ~IPsecAuthSHA1();
  
  const char *class_name() const	{ return "IPsecAuthSHA1"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  IPsecAuthSHA1 *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void notify_noutputs(int n);

  Packet *simple_action(Packet *);
  void add_handlers();
  
  static String drop_handler(Element *e, void *thunk);
  
private:

  int _op;
  u_atomic32_t _drops;

  static const int COMPUTE_AUTH = 0;
  static const int VERIFY_AUTH = 1;
};

#endif

