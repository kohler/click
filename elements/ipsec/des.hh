#ifndef IPSEC_DES_HH
#define IPSEC_DES_HH

/*
 * =c
 * IPsecDES(DECRYPT/ENCRYPT, KEY [, IGNORE])
 * =s Encryption
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

#include <click/element.hh>
#include <click/glue.hh>

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
  Des();
  Des(int, unsigned char *);
  ~Des();
  
  const char *class_name() const	{ return "IPsecDES"; }
  const char *processing() const	{ return AGNOSTIC; }
  
  Des *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

  static const unsigned DES_DECRYPT = 0;
  static const unsigned DES_ENCRYPT = 1;
  
private:

  void des_set_key(des_cblock *key, des_key_schedule schedule);
  void des_encrypt(unsigned long *input,unsigned long *output,
		   des_key_schedule ks,int encrypt);
  int des_ecb_encrypt(des_cblock *input, des_cblock *output,
		      des_key_schedule ks, int encrypt);

  unsigned _op;
  int _ignore;
  des_cblock _key;
  des_key_schedule _ks;
};

#endif
