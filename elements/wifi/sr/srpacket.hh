#ifndef CLICK_SRPAKCET_HH
#define CLICK_SRPAKCET_HH
#include <click/ipaddress.hh>
#include <elements/wifi/path.hh>
CLICK_DECLS

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


enum srpacket_type { 
	PT_QUERY = 0x01,
	PT_REPLY = 0x02,
	PT_DATA  = 0x04,
	PT_GATEWAY = 0x08,
};



enum srpacket_flags {
	FLAG_ERROR = (1<<0),
	FLAG_UPDATE = (1<<1),
};

static const uint8_t _sr_version = 0x0c;

/* srcr packet format */
CLICK_SIZE_PACKED_STRUCTURE(
struct srpacket {,
        uint8_t _version; /* see _srcr_version */
	uint8_t _type;    /* see srpacket_type */
	uint8_t _nlinks;
	uint8_t _next;    /* who should process this packet. */

	int    num_links()              { return _nlinks; }
	int    next()                   { return _next; }
	void   set_next(uint8_t n)      { _next = n; }
	void   set_num_links(uint8_t n) { _nlinks = n; }

	/* packet length functions */
	static size_t len_wo_data(int nlinks) {
		return sizeof(struct srpacket) + sizeof(uint32_t) + 
			(nlinks) * sizeof(uint32_t) * 5;
	}
	static size_t len_with_data(int nlinks, int dlen) {
		return len_wo_data(nlinks) + dlen;
	}
	size_t hlen_wo_data()   const { return len_wo_data(_nlinks); }
	size_t hlen_with_data() const { return len_with_data(_nlinks, ntohs(_dlen)); }

private:
	/* these are private and have access functions below so I
	 * don't have to remember about endianness
	 */
	uint16_t _ttl;
	uint16_t _cksum;
	uint16_t _flags; 
	uint16_t _dlen;
	
	uint32_t _qdst; /* query destination */
	uint32_t _seq;
public:  
	bool      flag(int f) { return ntohs(_flags) & f;  }
	uint16_t  data_len()  { return ntohs(_dlen); }
	IPAddress get_qdst()  { return _qdst; }
	uint32_t  seq()       { return ntohl(_seq); }
	uint32_t  data_seq()  { return ntohl(_qdst); }

	void      set_flag(uint16_t f)       { _flags = htons(ntohs(_flags) | f); }
	void      unset_flag(uint16_t f)     { _flags = htons(ntohs(_flags) & !f);  }
	void      set_data_len(uint16_t len) { _dlen = htons(len); }
	void      set_qdst(IPAddress ip)     { _qdst = ip; }
	void      set_seq(uint32_t n)        { _seq = htonl(n); }
	void      set_data_seq(uint32_t n)   { _qdst = htonl(n); }


	/* remember that if you call this you must have set the number
	 * of links in this packet!
	 */
	u_char *data() { return (((u_char *)this) + len_wo_data(num_links())); }

	void set_checksum() {
		unsigned int tlen = (_type & PT_DATA) ? hlen_with_data() : hlen_wo_data();
		_cksum = 0;
		_cksum = click_in_cksum((unsigned char *) this, tlen);
	}	
	bool check_checksum() {
		unsigned int tlen = (_type & PT_DATA) ? hlen_with_data() : hlen_wo_data();
		return click_in_cksum((unsigned char *) this, tlen) == 0;
	}
	/* the rest of the packet is variable length based on _nlinks.
	 * for each link, the following packet structure exists: 
	 * uint32_t ip
	 * uint32_t fwd
	 * uint32_t rev
	 * uint32_t seq
	 * uint32_t age
	 * uint32_t ip  
	 */
	void set_link(int link,
		      IPAddress a, IPAddress b, 
		      uint32_t fwd, uint32_t rev,
		      uint32_t seq,
		      uint32_t age) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		ndx[0] = a;
		ndx[1] = htonl(fwd);
		ndx[2] = htonl(rev);
		ndx[3] = htonl(seq);
		ndx[4] = htonl(age);
		ndx[5] = b;
	}	
	uint32_t get_link_fwd(int link) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		return ntohl(ndx[1]);
	}
	uint32_t get_link_rev(int link) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		return ntohl(ndx[2]);
	}	
	uint32_t get_link_seq(int link) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		return ntohl(ndx[3]);
	}
	
	uint32_t get_link_age(int link) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		return ntohl(ndx[4]);
	}	
	IPAddress get_link_node(int link) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		return ndx[0];
	}	
	void set_link_node(int link, IPAddress ip) {
		uint32_t *ndx = (uint32_t *) (this+1);
		ndx += link * 5;
		ndx[0] = ip;
	}
	Path get_path() {
		Path p;
		for (int x = 0; x <= num_links(); x++) {
			p.push_back(get_link_node(x));
		}
		return p;
	}
});

CLICK_ENDDECLS
#endif /* CLICK_SRPACKET_HH */
