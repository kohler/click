// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ELEMENTMAP_HH
#define CLICK_ELEMENTMAP_HH

class ElementMap { public:

    struct Elt {
	int uid;
	String name;
	String cxx;
	String header_file;
	String source_file;
	String processing_code;
	String flow_code;
	String flags;
	String requirements;
	String provisions;
	int def_index;
	int driver_mask;
	int uid_next;

	Elt();
	
	bool allows_driver(int d) const	{ return (driver_mask&(1<<d)) != 0; }

	bool requires(const String &) const;
	bool provides(const String &) const;
	int flag_value(int) const;
	
	String *component(int);
    };

    static const int DRIVER_LINUXMODULE = 0;
    static const int DRIVER_USERLEVEL = 1;
    static const int DRIVER_BSDMODULE = 2;
    static const int ALL_DRIVERS = 0x7;
    static const int NDRIVERS = 3;
    static const char *driver_name(int);
    static const char *driver_requirement(int);

    ElementMap();
    ElementMap(const String &);

    int size() const				{ return _e.size(); }
    bool empty() const				{ return _e.size() == 1; }

    const Elt &elt(const ElementClassT *) const;
    const Elt &elt(const ElementT &) const;
    const Elt &elt(const String &) const;
    const Elt &elt(int i) const			{ return _e[i]; }
    int elt_index(const ElementClassT *) const;
    int elt_index(const String &) const;
    bool has_elt(const ElementClassT *) const;
    bool has_elt(const String &) const;
    
    const String &name(const ElementClassT *) const;
    const String &cxx(const ElementClassT *) const;
    const String &header_file(const ElementClassT *) const;
    const String &source_file(const ElementClassT *) const;
    const String &source_directory(const Elt &) const;
    const String &source_directory(const ElementClassT *) const;
    const String &processing_code(const ElementClassT *) const;
    const String &flow_code(const ElementClassT *) const;
    const String &flags(const ElementClassT *) const;
    const String &requirements(const ElementClassT *) const;
    bool requires(const ElementClassT *, const String &) const;
    const String &provisions(const ElementClassT *) const;
    bool provides(const ElementClassT *, const String &) const;
    const String &package(const ElementClassT *) const;
    const String &package(const String &) const;
    int definition_index(int i) const		{ return _e[i].def_index; }

    typedef HashMap<int, int>::Iterator IndexIterator;
    IndexIterator first() const		{ return _uid_map.first(); }

    int add(const Elt &);
    int add(const String &name, const String &cxx, const String &header_file,
	    const String &processing_code, const String &flow_code,
	    const String &flags,
	    const String &requirements, const String &provisions);
    int add(const String &name, const String &cxx, const String &header_file,
	    const String &processing_code, const String &flow_code);
  private:void remove(int);public:
    //void remove(const String &n)		{ remove(find(n)); }

    const String &def_source_directory(int i) const { return _def_srcdir[i]; }
    const String &def_compile_flags(int i) const { return _def_compile_flags[i];}

    void parse(const String &data);
    void parse(const String &data, const String &package_name);
    bool parse_default_file(const String &default_path, ErrorHandler *);
    bool parse_requirement_files(RouterT *, const String &default_path, ErrorHandler *, String *not_found = 0);
    bool parse_all_files(RouterT *, const String &default_path, ErrorHandler *);
    static void report_file_not_found(String default_path, bool found_default, ErrorHandler *);
    String unparse() const;

    int check_completeness(const RouterT *, ErrorHandler *) const;
    bool driver_indifferent(const RouterT *, int driver_mask = ALL_DRIVERS, ErrorHandler * = 0) const;
    bool driver_compatible(const RouterT *, int driver, ErrorHandler * = 0) const;
    void limit_driver(int driver);

  private:

    Vector<Elt> _e;
    HashMap<int, int> _uid_map;

    Vector<String> _def_srcdir;
    Vector<String> _def_compile_flags;
    Vector<String> _def_package;

    int get_driver_mask(const String &);

    enum {
	D_NONE,
	D_CLASS, D_CXX_CLASS, D_HEADER_FILE, D_PROCESSING,
	D_FLOW_CODE, D_FLAGS, D_REQUIREMENTS, D_PROVISIONS,
	D_SOURCE_FILE,
	D_FIRST_DEFAULT = D_CLASS, D_LAST_DEFAULT = D_PROVISIONS
    };
    static int parse_component(const String &);

    void collect_indexes(const RouterT *, Vector<int> &, ErrorHandler *) const;

};

inline
ElementMap::Elt::Elt()
  : uid(-1), def_index(0), driver_mask(ALL_DRIVERS), uid_next(0)
{
}

inline const ElementMap::Elt &
ElementMap::elt(const ElementClassT *c) const
{
    return _e[c ? _uid_map[c->uid()] : 0];
}

inline const ElementMap::Elt &
ElementMap::elt(const ElementT &e) const
{
    return elt(e.type());
}

inline const ElementMap::Elt &
ElementMap::elt(const String &name) const
{
    return elt(ElementClassT::default_class(name));
}

inline int
ElementMap::elt_index(const ElementClassT *c) const
{
    return (c ? _uid_map[c->uid()] : 0);
}

inline int
ElementMap::elt_index(const String &name) const
{
    return elt_index(ElementClassT::default_class(name));
}

inline bool
ElementMap::has_elt(const ElementClassT *c) const
{
    return elt_index(c) > 0;
}

inline bool
ElementMap::has_elt(const String &name) const
{
    return elt_index(name) > 0;
}

inline const String &
ElementMap::name(const ElementClassT *c) const
{
    return elt(c).name;
}

inline const String &
ElementMap::cxx(const ElementClassT *c) const
{
    return elt(c).cxx;
}

inline const String &
ElementMap::header_file(const ElementClassT *c) const
{
    return elt(c).header_file;
}

inline const String &
ElementMap::source_file(const ElementClassT *c) const
{
    return elt(c).source_file;
}

inline const String &
ElementMap::source_directory(const Elt &e) const
{
    return _def_srcdir[e.def_index];
}

inline const String &
ElementMap::processing_code(const ElementClassT *c) const
{
    return elt(c).processing_code;
}

inline const String &
ElementMap::flow_code(const ElementClassT *c) const
{
    return elt(c).flow_code;
}

inline const String &
ElementMap::flags(const ElementClassT *c) const
{
    return elt(c).flags;
}

inline const String &
ElementMap::requirements(const ElementClassT *c) const
{
    return elt(c).requirements;
}

inline bool
ElementMap::requires(const ElementClassT *c, const String &what) const
{
    return elt(c).requires(what);
}

inline const String &
ElementMap::provisions(const ElementClassT *c) const
{
    return elt(c).provisions;
}

inline bool
ElementMap::provides(const ElementClassT *c, const String &what) const
{
    return elt(c).provides(what);
}

inline const String &
ElementMap::package(const String &what) const
{
    return package(ElementClassT::default_class(what));
}

#endif
