#ifndef CLICK_IPSECAES_HH
#define CLICK_IPSECAES_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * IPsecAES(ENCRYPT)
 * =s ipsec
 * encrypt packet using DES-CBC
 * =d
 *
 * Encrypts or decrypts packet using DES-CBC. If the first argument is 0,
 * IPsecAES will decrypt. If the first argument is 1, IPsecAES will encrypt.
 * KEY is the DES secret key. Gets IV value from ESP header. IGNORE is the
 * number of bytes at the end of the payload to ignore. By default, IGNORE is
 * 12, which is the number of SHA1 authentication digest bytes for ESP or AH.
 *
 * =a IPsecESPEncap, IPsecESPUnencap, IPsecAuthSHA1
 */

# define GETU32(pt) (((unsigned long)(pt)[0] << 24) ^ ((unsigned long)(pt)[1] << 16) ^ ((unsigned long)(pt)[2] <<  8) ^ ((unsigned long)(pt)[3]))
# define PUTU32(ct, st) { (ct)[0] = (char)((st) >> 24); (ct)[1] = (char)((st) >> 16); (ct)[2] = (char)((st) >>  8); (ct)[3] = (char)(st); }
/*#endif*/

#define AES_MAXNR 14
#define AES_BLOCK_SIZE 16

struct aes_key_st {
    unsigned long rd_key[4 *(AES_MAXNR + 1)];
    int rounds;
};
typedef struct aes_key_st AES_KEY;


class Address;


class Aes : public Element {
 public:
   Aes() CLICK_COLD;
   Aes(int);
   ~Aes() CLICK_COLD;

   const char *class_name() const	{ return "IPsecAES"; }
   const char *port_count() const	{ return PORTS_1_1; }

   int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
   int initialize(ErrorHandler *) CLICK_COLD;

   Packet *simple_action(Packet *);

   enum { AES_DECRYPT = 0, AES_ENCRYPT = 1 };

 private:
   int AES_set_encrypt_key(const unsigned char *userKey, const int bits, AES_KEY *key);
   int AES_set_decrypt_key(const unsigned char *userKey, const int bits, AES_KEY *key);
   void AES_encrypt(const unsigned char *in, unsigned char *out,const AES_KEY *key);
   void AES_decrypt(const unsigned char *in, unsigned char *out,const AES_KEY *key);
   unsigned _op;
   int _ignore;
   AES_KEY _key;
};

CLICK_ENDDECLS
#endif
