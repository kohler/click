#ifndef IPSEC_DES_HH
#define IPSEC_DES_HH

/*
 * =c
 * IPsecDES(ENCRYPT/DECRYPT, IV, KEY)
 * =s encryption
 * encrypt packet using DES
 * =d
 * 
 * encrypts or decrypts packet using DES. DES key is set to KEY, IV as the
 * integrity value. see RFC 2406.
 *
 * IPsecDES should NOT be used with multiple sources, since it uses integrity
 * value from one packet in the encryption process of the next packet.
 * Therefore packets going into IPsecDES must be one stream, in the right
 * order.
 *
 * =a IPsecESPEncap, IPsecESPUnencap */

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

#define DES_ENCRYPT	1
#define DES_DECRYPT	0

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
  
private:

  void des_set_key(des_cblock *key, des_key_schedule schedule);
  void des_encrypt(unsigned long *input,unsigned long *output,
		   des_key_schedule ks,int encrypt);
  int des_ecb_encrypt(des_cblock *input, des_cblock *output,
		      des_key_schedule ks, int encrypt);

  int _decrypt;
  des_cblock _iv; 
  des_cblock _key;
  des_key_schedule _ks;
};

#endif
