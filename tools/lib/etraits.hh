// -*- c-basic-offset: 4 -*-
#ifndef CLICK_ETRAITS_HH
#define CLICK_ETRAITS_HH
#include <click/string.hh>

struct Driver {
    enum {
	USERLEVEL = 0, LINUXMODULE = 1, BSDMODULE = 2, NSMODULE = 3,
	ALLMASK = 0xF, COUNT = 4, MULTITHREAD = COUNT
    };
    static const char *name(int);
    static const char *multithread_name(int);
    static const char *requirement(int);
    static int driver(const String&);
    static int driver_mask(const String&);
};


struct ElementTraits {

    String name;
    String cxx;
    String documentation_name;
    String header_file;
    String source_file;
    String port_count_code;
    String processing_code;
    String flow_code;
    String flags;
    String methods;
    String requirements;
    String provisions;
    String libs;
    String noexport;
    int def_index;
    int driver_mask;
    int name_next;

    ElementTraits();

    static const ElementTraits &null_traits()	{ return the_null_traits; }

    bool allows_driver(int d) const	{ return (driver_mask&(1<<d)) != 0; }

    bool requires(const String &str) const;
    bool provides(const String &str) const;
    inline int flag_value(const String &str) const;
    int hard_flag_value(const String &str) const;

    String *component(int);
    String *component(const String &);

    void calculate_driver_mask();

    enum {
	D_NONE,
	D_CLASS, D_CXX_CLASS, D_HEADER_FILE, D_PORT_COUNT, D_PROCESSING,
	D_FLOW_CODE, D_FLAGS, D_METHODS, D_REQUIREMENTS, D_PROVISIONS, D_LIBS,
	D_SOURCE_FILE, D_DOC_NAME, D_NOEXPORT,
	D_FIRST_DEFAULT = D_CLASS, D_LAST_DEFAULT = D_LIBS
    };
    static int parse_component(const String &);
    static ElementTraits make(int, ...);

  private:

    static ElementTraits the_null_traits;

    friend class ElementMap;

};

typedef ElementTraits Traits;


inline
ElementTraits::ElementTraits()
    : def_index(0), driver_mask(Driver::ALLMASK), name_next(0)
{
}

inline String *
ElementTraits::component(const String &s)
{
    return component(parse_component(s));
}

inline int
ElementTraits::flag_value(const String &str) const
{
    return flags ? hard_flag_value(str) : -1;
}

#endif
