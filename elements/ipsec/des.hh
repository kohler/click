#ifndef IPSEC_DES_HH
#define IPSEC_DES_HH

/*
 * =c
 * IPsecDES(DECRYPT/ENCRYPT/REENCRYPT, IV, KEY)
 * =s encryption
 * encrypt packet using DES-CBC
 * =d
 * 
 * Encrypts or decrypts packet using DES-CBC. If the first argument is 0,
 * IPsecDES will decrypt. If the first argument is 1, IPsecDES will encrypt,
 * using IV as the initial integrity value and DES as the key. If the first
 * argument is 2, IPsecDES will re-encrypt, using existing IV value.
 *
 * =a IPsecESPEncap, IPsecESPUnencap 
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
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  Packet *simple_action(Packet *);

  static const unsigned DES_DECRYPT = 0;
  static const unsigned DES_ENCRYPT = 1;
  static const unsigned DES_REENCRYPT = 2;
  
private:

  void des_set_key(des_cblock *key, des_key_schedule schedule);
  void des_encrypt(unsigned long *input,unsigned long *output,
		   des_key_schedule ks,int encrypt);
  int des_ecb_encrypt(des_cblock *input, des_cblock *output,
		      des_key_schedule ks, int encrypt);

  int _op;
  des_cblock _iv; 
  des_cblock _key;
  des_key_schedule _ks;
};

#endif
