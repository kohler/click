#ifndef IPSEC_DES_HH
#define IPSEC_DES_HH

/*
 * IPsec_Des
 * 
 * Encrypt an ESP packet using DES
 */


#include "element.hh"
#include "glue.hh"

typedef unsigned char des_cblock[8];
typedef struct des_ks_struct
	{
	union	{
		des_cblock _;
		/* make sure things are correct size on machines with
		 * 8 byte longs */
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
  
  const char *class_name() const		{ return "IPsecDES"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  
  Des *clone() const;
  int configure(const String &, ErrorHandler *);
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
