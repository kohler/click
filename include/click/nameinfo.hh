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
	T_ICMP_TYPE = 0x04010000,
	T_ICMP_CODE = 0x04010100,
	T_TCP_PORT = 0x04060001,
	T_TCP_OPT = 0x04060002,
	T_UDP_PORT = 0x04110001,
	T_IP6_ADDR = 0x06000001,
	T_IP6_PREFIX = 0x06000002
    };

    static NameDB *getdb(uint32_t type, const Element *prefix, int data, bool create);
    static void installdb(NameDB *db, const Element *prefix);
    static void removedb(NameDB *db);

    static bool query(uint32_t type, const Element *prefix, const String &name, void *value_store, int data);
    static bool query_int(uint32_t type, const Element *prefix, const String &name, int32_t *value_store);
    static bool query_int(uint32_t type, const Element *prefix, const String &name, uint32_t *value_store);
    static String revquery(uint32_t type, const Element *prefix, const void *value, int data);
    static inline String revquery_int(uint32_t type, const Element *prefix, int32_t value);
    static inline void define(uint32_t type, const Element *prefix, const String &name, const void *value, int data);

#ifdef CLICK_NAMEDB_CHECK
    void check(ErrorHandler *);
    static void check(const Element *, ErrorHandler *);
#endif
    
  private:
    
    Vector<NameDB *> _namedb_roots;
    Vector<NameDB *> _namedbs;

    inline NameDB *install_dynamic_sentinel() { return (NameDB *) this; }
    NameDB *namedb(uint32_t type, int data, const String &prefix, NameDB *installer);
    
#ifdef CLICK_NAMEDB_CHECK
    uintptr_t _check_generation;
    void checkdb(NameDB *db, NameDB *parent, ErrorHandler *errh);
#endif
    
};


class NameDB { public:
    
    inline NameDB(uint32_t type, const String &prefix, int data);
    virtual ~NameDB()			{ }

    uint32_t type() const		{ return _type; }
    int data() const			{ return _data; }
    const String &prefix() const	{ return _prefix; }
    NameDB *prefix_parent() const	{ return _prefix_parent; }

    virtual void *find(const String &name, bool create) = 0;
    virtual String revfind(const void *value, int data) = 0;
    inline bool query(const String &name, void *value, int data);
    inline void define(const String &name, const void *value, int data);

#ifdef CLICK_NAMEDB_CHECK
    virtual void check(ErrorHandler *) = 0;
#endif
    
  private:
    
    uint32_t _type;
    String _prefix;
    int _data;
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

    void *find(const String &name, bool create);
    String revfind(const void *value, int data);

#ifdef CLICK_NAMEDB_CHECK
    void check(ErrorHandler *);
#endif
    
  private:

    const Entry *_entries;
    int _nentries;
	
};

class DynamicNameDB : public NameDB { public:

    inline DynamicNameDB(uint32_t type, const String &prefix, int data);

    void *find(const String &name, bool create);
    String revfind(const void *value, int data);
    
#ifdef CLICK_NAMEDB_CHECK
    void check(ErrorHandler *);
#endif
    
  private:

    Vector<String> _names;
    StringAccum _values;
    int _sorted;

    void sort();
    
};


inline
NameDB::NameDB(uint32_t type, const String &prefix, int data)
    : _type(type), _prefix(prefix), _data(data),
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
DynamicNameDB::DynamicNameDB(uint32_t type, const String &prefix, int data)
    : NameDB(type, prefix, data), _sorted(0)
{
}

inline bool
NameDB::query(const String &name, void *value, int data)
{
    assert(_data == data);
    if (const void *x = find(name, false)) {
	memcpy(value, x, data);
	return true;
    } else
	return false;
}

inline void
NameDB::define(const String &name, const void *value, int data)
{
    assert(_data == data);
    if (void *x = find(name, true))
	memcpy(x, value, data);
}

inline String
NameInfo::revquery_int(uint32_t type, const Element *e, int32_t value)
{
    return revquery(type, e, &value, 4);
}

inline void
NameInfo::define(uint32_t type, const Element *e, const String &name, const void *value, int data)
{
    if (NameDB *db = getdb(type, e, data, true))
	db->define(name, value, data);
}

CLICK_ENDDECLS
#endif
