#ifndef CLICK_IPFIELDINFO_HH
#define CLICK_IPFIELDINFO_HH
#include <click/nameinfo.hh>
#include <click/element.hh>
CLICK_DECLS

class IPField { public:

    IPField()			: _val(-1) { }
    IPField(int32_t f)		: _val(f) { }
    IPField(int proto, int bit_offset, int bit_length);

    inline bool ok() const	{ return _val >= 0; }
    inline int32_t value() const { return _val; }

    inline int proto() const	{ return (_val & PROTO_MASK) >> PROTO_SHIFT; }

    inline int bit_offset() const;
    inline int bit_length() const;

    inline int byte_offset() const;
    inline int byte_length() const;

    enum {
	PROTO_SHIFT = 20,
	MAX_PROTO = 0x1FF,
	PROTO_MASK = MAX_PROTO << PROTO_SHIFT,

	OFFSET_SHIFT = 6,
	MAX_OFFSET = 0x1FFF,
	OFFSET_MASK = MAX_OFFSET << OFFSET_SHIFT,

	LENGTH_SHIFT = 0,
	MAX_LENGTH = 0x3F,
	LENGTH_MASK = MAX_LENGTH << LENGTH_SHIFT,

	BYTES = 0x00080000,
	MARKER = 0x40000000
    };

    enum {
	F_IP_TOS = (8 << OFFSET_SHIFT) | (7 << LENGTH_SHIFT),
	F_ICMP_TYPE = (IP_PROTO_ICMP << PROTO_SHIFT) | (7 << LENGTH_SHIFT)
    };

    static const char *parse(const char *begin, const char *end, int proto, IPField *result, ErrorHandler *errh, Element *elt = 0);
    String unparse(Element *elt = 0, bool tcpdump_rules = false);

  private:

    int32_t _val;

};

class IPFieldInfo : public Element { public:

    IPFieldInfo()			{ }

    const char *class_name() const	{ return "IPFieldInfo"; }

    static void static_initialize();
    static void static_cleanup();

};


inline int
IPField::bit_offset() const
{
    int v = (_val & OFFSET_MASK) >> OFFSET_SHIFT;
    return (_val & BYTES ? v << 3 : v);
}

inline int
IPField::byte_offset() const
{
    int v = (_val & OFFSET_MASK) >> OFFSET_SHIFT;
    return (_val & BYTES ? v : v >> 3);
}

inline int
IPField::bit_length() const
{
    int l = ((_val & LENGTH_MASK) >> LENGTH_SHIFT) + 1;
    return (_val & BYTES ? l << 3 : l);
}

inline int
IPField::byte_length() const
{
    int l = ((_val & LENGTH_MASK) >> LENGTH_SHIFT) + 1;
    return (_val & BYTES ? l : l >> 3);
}

CLICK_ENDDECLS
#endif
