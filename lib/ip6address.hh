#ifndef IP6ADDRESS_HH
#define IP6ADDRESS_HH
#include "string.hh"
#include "glue.hh"
#include <in.h>
//#include </usr/include/netinet/in.h>
//#include <linux/in6.h>

/* IPv6 address , same as from /usr/include/netinet/in.h  */
struct in6my_addr
  {
    union
      {
	uint8_t		u6_addr8[16];
	uint16_t	u6_addr16[8];
	uint32_t	u6_addr32[4];
#if ULONG_MAX > 0xffffffff
	uint64_t	u6_addr64[2];
#endif
      } in6_u;
#define s6_addr			in6_u.u6_addr8
#define s6_addr16		in6_u.u6_addr16
#define s6_addr32		in6_u.u6_addr32
#define s6_addr64		in6_u.u6_addr64
  };


 /*IPv6 address class for storage 16 bytes ipv6 address*/
class ip6_addr 
 {
   public:
   unsigned char add[16];
   static const int size = 16;
  
   //constructors and deconstructor
   ip6_addr()  {
      for (int i=0; i<size; i++)
	add[i]=0;
    }


   ip6_addr(ip6_addr * o_add)  {
      for (int i=0; i<size; i++)
	add[i]=o_add->add[i];
    }

    ip6_addr(unsigned char  *pp)  {
       for (int i=0; i<size; i++)
	add[i]=pp[i] & 0xff;
    }

  ip6_addr(struct in6my_addr o_add) {
      for (int i=0; i<size; i++)
	add[i]=o_add.s6_addr[i];
    }


    ~ip6_addr() { }
  
   
   inline bool
   isZero()
   {
     bool result = true;
     for (int i= 0; i<size; i++)
       {
	 result = result && (add[i]==0);
       }
     return result;
   }

};

class IP6Address {
 
   ip6_addr _addr;
   static const int size = 16;
  
 public:
  IP6Address();
  explicit IP6Address(ip6_addr *);
  explicit IP6Address(ip6_addr);
  explicit IP6Address(const unsigned char *);
  explicit IP6Address(const String &);	// "fec0:0:0:1::1"
  explicit IP6Address(struct in6my_addr);

  operator bool() const	{ 
    int x = 0;
    for (int i=0; i<size; i++) {
	x= _addr.add[i];
      }
	return x!=0;
  }
  
  ip6_addr addr() const 	{ return _addr;	} 	

  operator struct in6my_addr() const;
  struct in6my_addr in6my_addr() const;

  unsigned char  *data() const	{return (unsigned char *)&(_addr.add); };
  unsigned  *hashcode() const	{ return (unsigned *)&(_addr); }
  operator String() const	{ return s(); }
  String s() const;
  void print();
  
};

inline
IP6Address::IP6Address() {
	_addr =new ip6_addr();
}

inline
IP6Address::IP6Address(ip6_addr * pp) {
	_addr=new ip6_addr(pp);
}

inline
IP6Address::IP6Address(ip6_addr pp) {
        for (int i=0; i<16; i++)
	  	_addr.add[i]=pp.add[i] & 0xff;
}


inline
IP6Address::IP6Address(struct in6my_addr ina) {
	_addr=new ip6_addr(ina);
}

inline IP6Address
operator&(IP6Address a, IP6Address b) {
  ip6_addr result=new ip6_addr();
  int size =16;
  for (int i=0; i<size; i++)
    result.add[i] = (a.addr().add[i] & b.addr().add[i]);
  return IP6Address(result);
}

inline bool
operator==(IP6Address a, IP6Address b) {  
  bool result= true;
  int size =16;
  for (int i=0; i<size; i++)
    result = result && (a.addr().add[i] == b.addr().add[i]);
  return result;
}

inline bool
operator!=(IP6Address a, IP6Address b) {  
  bool result= true;
  int size =16;
  for (int i=0; i<size; i++)
    result = result || (a.addr().add[i] != b.addr().add[i]);
  return result;
}

inline struct in6my_addr
IP6Address::in6my_addr() const {
  struct in6my_addr ia;
  int size =16;
  for (int i=0; i<size; i++)
	ia.s6_addr[i]=_addr.add[i];
  return ia;
}

inline
IP6Address::operator struct in6my_addr() const {
  return in6my_addr();
}

#endif
