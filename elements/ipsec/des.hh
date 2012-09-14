#ifndef CLICK_IPSECDES_HH
#define CLICK_IPSECDES_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * IPsecDES(ENCRYPT, KEY [, IGNORE])
 * =s ipsec
 * encrypt packet using DES-CBC
 * =d
 *
 * Encrypts or decrypts packet using DES-CBC. If the first argument is 0,
 * IPsecDES will decrypt. If the first argument is 1, IPsecDES will encrypt.
 * KEY is the DES secret key. Gets IV value from ESP header. IGNORE is the
 * number of bytes at the end of the payload to ignore. By default, IGNORE is
 * 12, which is the number of SHA1 authentication digest bytes for ESP or AH.
 *
 * =a IPsecESPEncap, IPsecESPUnencap, IPsecAuthSHA1
 */

typedef unsigned char des_cblock[8];
typedef struct des_ks_struct {
  union	{
    des_cblock _;
    /* make sure things are correct size on machines with 8 byte longs */
    unsigned long pad[2];
  } ks;
} des_key_schedule[16];

class Address;

class Des : public Element {
public:
  Des() CLICK_COLD;
  Des(int);
  ~Des() CLICK_COLD;

  const char *class_name() const	{ return "IPsecDES"; }
  const char *port_count() const	{ return PORTS_1_1; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  Packet *simple_action(Packet *);

  enum { DES_DECRYPT = 0, DES_ENCRYPT = 1 };

private:

  void des_set_key(des_cblock *key, des_key_schedule schedule);
  void des_encrypt(unsigned long *input,unsigned long *output,
		   des_key_schedule ks,int encrypt);
  int des_ecb_encrypt(des_cblock *input, des_cblock *output,
		      des_key_schedule ks, int encrypt);

  unsigned _op;
  int _ignore;
  des_key_schedule _ks;
};

CLICK_ENDDECLS
#endif
