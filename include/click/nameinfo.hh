// -*- c-basic-offset: 4; related-file-name: "../../lib/nameinfo.cc" -*-
#ifndef CLICK_NAMEINFO_HH
#define CLICK_NAMEINFO_HH
#include <click/string.hh>
#include <click/vector.hh>
#include <click/straccum.hh>
CLICK_DECLS
class Element;
class NameDB;
class ErrorHandler;

class NameInfo { public:

    NameInfo();
    ~NameInfo();

    static void static_initialize();
    static void static_cleanup();

    enum {
	T_NONE = 0,
	T_SCHEDULEINFO = 0x00000001,
	T_ETHERNET_ADDR = 0x01000001,
	T_IP_ADDR = 0x04000001,
	T_IP_PREFIX = 0x04000002,
	T_IP_PROTO = 0x04000003,
	T_IPFILTER_TYPE = 0x04000004,
	T_TCP_OPT = 0x04000005,
	T_ICMP_TYPE = 0x04010000,
	T_ICMP_CODE = 0x04010100,
	T_IP_PORT = 0x04020000,
	T_TCP_PORT = 0x04020006,
	T_UDP_PORT = 0x04020011,
	T_IP_FIELD = 0x04030000,
	T_ICMP_FIELD = 0x04030001,
	T_TCP_FIELD = 0x04030006,
	T_UDP_FIELD = 0x04030011,
	T_IP6_ADDR = 0x06000001,
	T_IP6_PREFIX = 0x06000002
    };

    static NameDB *getdb(uint32_t type, const Element *prefix, int value_size, bool create);
    static void installdb(NameDB *db, const Element *prefix);
    static void removedb(NameDB *db);

    static bool query(uint32_t type, const Element *prefix, const String &name, void *value_store, int value_size);
    static bool query_int(uint32_t type, const Element *prefix, const String &name, int32_t *value_store);
    static bool query_int(uint32_t type, const Element *prefix, const String &name, uint32_t *value_store);
    static String revquery(uint32_t type, const Element *prefix, const void *value, int value_size);
    static inline String revquery_int(uint32_t type, const Element *prefix, int32_t value);
    static inline void define(uint32_t type, const Element *prefix, const String &name, const void *value, int value_size);

#ifdef CLICK_NAMEDB_CHECK
    void check(ErrorHandler *);
    static void check(const Element *, ErrorHandler *);
#endif
    
  private:
    
    Vector<NameDB *> _namedb_roots;
    Vector<NameDB *> _namedbs;

    inline NameDB *install_dynamic_sentinel() { return (NameDB *) this; }
    NameDB *namedb(uint32_t type, int value_size, const String &prefix, NameDB *installer);
    
#ifdef CLICK_NAMEDB_CHECK
    uintptr_t _check_generation;
    void checkdb(NameDB *db, NameDB *parent, ErrorHandler *errh);
#endif
    
};


class NameDB { public:
    
    inline NameDB(uint32_t type, const String &prefix, int value_size);
    virtual ~NameDB()			{ }

    uint32_t type() const		{ return _type; }
    int value_size() const		{ return _value_size; }
    const String &prefix() const	{ return _prefix; }
    NameDB *prefix_parent() const	{ return _prefix_parent; }

    virtual bool query(const String &name, void *value, int vsize) = 0;
    virtual void define(const String &name, const void *value, int vsize);
    virtual String revfind(const void *value, int vsize);

#ifdef CLICK_NAMEDB_CHECK
    virtual void check(ErrorHandler *);
#endif
    
  private:
    
    uint32_t _type;
    String _prefix;
    int _value_size;
    NameDB *_prefix_parent;
    NameDB *_prefix_sibling;
    NameDB *_prefix_child;
    NameInfo *_installed;

#ifdef CLICK_NAMEDB_CHECK
    uintptr_t _check_generation;
#endif
    
    friend class NameInfo;
    
};

class StaticNameDB : public NameDB { public:

    struct Entry {
	const char *name;
	uint32_t value;
    };

    inline StaticNameDB(uint32_t type, const String &prefix, const Entry *entry, int nentry);

    bool query(const String &name, void *value, int vsize);
    String revfind(const void *value, int vsize);

#ifdef CLICK_NAMEDB_CHECK
    void check(ErrorHandler *);
#endif
    
  private:

    const Entry *_entries;
    int _nentries;
	
};

class DynamicNameDB : public NameDB { public:

    inline DynamicNameDB(uint32_t type, const String &prefix, int vsize);

    bool query(const String &name, void *value, int vsize);
    void define(const String &name, const void *value, int vsize);
    String revfind(const void *value, int vsize);
    
#ifdef CLICK_NAMEDB_CHECK
    void check(ErrorHandler *);
#endif
    
  private:

    Vector<String> _names;
    StringAccum _values;
    int _sorted;

    void *find(const String &name, bool create);
    void sort();
    
};


inline
NameDB::NameDB(uint32_t type, const String &prefix, int vsize)
    : _type(type), _prefix(prefix), _value_size(vsize),
      _prefix_parent(0), _prefix_sibling(0), _prefix_child(0),
      _installed(0)
{
#ifdef CLICK_NAMEDB_CHECK
    _check_generation = 0;
#endif
}

inline
StaticNameDB::StaticNameDB(uint32_t type, const String &prefix, const Entry *entry, int nentry)
    : NameDB(type, prefix, 4), _entries(entry), _nentries(nentry)
{
}

inline
DynamicNameDB::DynamicNameDB(uint32_t type, const String &prefix, int vsize)
    : NameDB(type, prefix, vsize), _sorted(0)
{
}

inline String
NameInfo::revquery_int(uint32_t type, const Element *e, int32_t value)
{
    return revquery(type, e, &value, 4);
}

inline void
NameInfo::define(uint32_t type, const Element *e, const String &name, const void *value, int vsize)
{
    if (NameDB *db = getdb(type, e, vsize, true))
	db->define(name, value, vsize);
}

CLICK_ENDDECLS
#endif
