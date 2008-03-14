#ifndef CLICKY_DSTYLE_HH
#define CLICKY_DSTYLE_HH 1
#include <gtk/gtk.h>
#include <click/hashmap.hh>
#include <click/vector.hh>
#include "permstr.hh"
#include "ref.hh"
#include <string.h>
namespace clicky {
class dcss_set;
class delt;

enum {
    dpshape_rectangle = 0,
    dpshape_triangle = 1
};

enum {
    dborder_none = 0,
    dborder_solid = 1,
    dborder_inset = 2,
    dborder_dashed = 3,
    dborder_dotted = 4
};

enum {
    dshadow_none = 0,
    dshadow_drop = 1,
    dshadow_halo = 2
};

enum {
    destyle_normal = 0,
    destyle_queue = 1
};

struct dport_style : public enable_ref_ptr {
    int shape;
    double length;
    double width;
    double color[4];
    int border_style;
    double border_width;
    double border_color[4];
    double margin[4];
    bool uniform_style;
};

struct delt_style : public enable_ref_ptr {
    double color[4];
    double background_color[4];
    int border_style;
    double border_width;
    double border_color[4];
    int shadow_style;
    double shadow_width;
    double shadow_color[4];
    double padding[4];
    double margin[4];
    double orientation_padding;
    double min_width;
    double min_height;
    double height_step;
    double ports_padding;
    bool orientation;
    int style;
    String text;
};

struct dqueue_style : public enable_ref_ptr {
    double queue_stripe_color[4];
    int queue_stripe_style;
    double queue_stripe_width;
    double queue_stripe_spacing;
};

enum {
    dhlt_hover = 0,
    dhlt_click = 1,
    dhlt_pressed = 2,
    dhlt_rect_click = 3
};

class dcss_selector { public:

    dcss_selector()
	: _type_glob(0), _name_glob(0) {
    }

    const String &type() const {
	return _type;
    }
    const String &name() const {
	return _name;
    }
    const Vector<String> &klasses() const {
	return _klasses;
    }
    bool type_glob() const {
	return _type_glob;
    }

    String unparse() const;

    bool match(const delt *e) const;

    bool match_port(bool isoutput, int port, int processing) const {
	if ((_klasses.size() || _name)
	    && !klasses_match_port(isoutput, port, processing))
	    return false;
	return true;
    }

    bool generic_port() const {
	return !_name;
    }
    bool generic_elt() const {
	return !_type && !_name && !_klasses.size();
    }

    int specificity() const {
	return (_name ? 40000 : 0)
	    + (_type ? 20000 : 0)
	    + (_highlight_match ? __builtin_popcount(_highlight_match) : 0)
	    + _klasses.size();
    }

    const char *parse(const String &str, const char *s);
    
  private:

    String _type;
    String _name;
    Vector<String> _klasses;
    char _type_glob;
    char _name_glob;
    char _highlight;
    char _highlight_match;

    bool klasses_match(const Vector<String> &klasses) const;
    bool klasses_match_port(bool isoutput, int port, int processing) const;
    
};


struct dcss_property {

    dcss_property(PermString name, const String &vstr)
	: _name(name), _vstr(vstr), _t(t_none) {
    }

    ~dcss_property() {
	if (_t == t_color)
	    delete[] _v.dp;
    }

    PermString name() const {
	return _name;
    }
    const String &vstring() const {
	return _vstr;
    }

    enum {
	t_none = 0,
	t_numeric,
	t_pixel,
	t_relative,
	t_color,
	t_border_style,
	t_shadow_style,
	t_string
    };
    int type() const {
	return _t;
    }

    bool change_type(int t) const {
	return _t == t || hard_change_type(t);
    }
    bool change_relative_pixel() const {
	return _t == t_pixel || _t == t_relative || hard_change_relative_pixel();
    }
    
    double vnumeric() const {
	return (change_type(t_numeric) ? _v.d : 0);
    }
    double vpixel() const {
	return (change_type(t_pixel) ? _v.d : 0);
    }
    double vpixel(double relative_to) const {
	change_relative_pixel();
	if (_t == t_pixel)
	    return _v.d;
	else if (_t == t_relative)
	    return _v.d * relative_to;
	else
	    return 0;
    }
    inline double vpixel(const dcss_set *dcs, PermString relative_to,
			 const delt *e) const;
    double vrelative() const {
	return (change_type(t_relative) ? _v.d : 0);
    }
    void vcolor(double *r, double *g, double *b, double *a) const {
	const double *c = (change_type(t_color) ? _v.dp : transparent_color);
	*r = c[0];
	*g = c[1];
	*b = c[2];
	if (a)
	    *a = c[3];
    }
    void vcolor(double color[4]) const {
	const double *c = (change_type(t_color) ? _v.dp : transparent_color);
	memcpy(color, c, sizeof(double) * 4);
    }
    int vborder_style() const {
	return (change_type(t_border_style) ? _v.i : dborder_solid);
    }
    int vshadow_style() const {
	return (change_type(t_shadow_style) ? _v.i : dshadow_none);
    }

    static const dcss_property null_property;
    
  private:
    
    PermString _name;
    String _vstr;
    mutable int _t;
    mutable union {
	double d;
	int i;
	double *dp;
    } _v;

    bool hard_change_type(int t) const;
    bool hard_change_relative_pixel() const;
    static const double transparent_color[4];
    
};

struct dcss_propmatch {
    PermString name;
    const dcss_property *property;

    const String &vstring(const char *n) const {
	assert(name == n);
	return property->vstring();
    }
    double vpixel(const char *n) const {
	assert(name == n);
	return property->vpixel();
    }
    double vpixel(const char *n, double relative_to) const {
	assert(name == n);
	return property->vpixel(relative_to);
    }
    double vpixel(const char *n, const dcss_set *dcs,
		  const delt *relative_elt) const {
	assert(name == n);
	return property->vpixel(dcs, name, relative_elt->parent());
    }
    double vpixel(const char *n, const dcss_set *dcs, PermString relative_name,
		  const delt *relative_elt) const {
	assert(name == n);
	return property->vpixel(dcs, relative_name, relative_elt);
    }
    double vrelative(const char *n) const {
	assert(name == n);
	return property->vrelative();
    }
    void vcolor(double color[4], const char *n) const {
	assert(name == n);
	property->vcolor(color);
    }
    int vborder_style(const char *n) const {
	assert(name == n);
	return property->vborder_style();
    }
    int vshadow_style(const char *n) const {
	assert(name == n);
	return property->vshadow_style();
    }
};


class dcss { public:

    dcss();

    const dcss_selector &selector() const {
	return _selector;
    }
    const String &type() const {
	return _selector.type();
    }
    unsigned selector_index() const {
	return _selector_index;
    }
    bool has_context() const {
	return _context.size() > 0;
    }
    bool match_context(const delt *e) const {
	return !_context.size() || hard_match_context(e);
    }

    const char *parse(const String &str, const char *s);

    String unparse_selector() const;

    int assign(dcss_propmatch **begin, dcss_propmatch **end) const;
    static void assign_all(dcss_propmatch *pbegin, dcss_propmatch *pend,
			   dcss **begin, dcss **end);
    static void assign_all(dcss_propmatch *props, dcss_propmatch **pp, int n,
			   dcss **begin, dcss **end);
    
    static void sort(dcss **begin, dcss **end);
    
  private:
    
    dcss_selector _selector;
    Vector<dcss_selector> _context;
    unsigned _selector_index;
    mutable Vector<dcss_property> _de;
    mutable bool _sorted;
    dcss *_next;

    void sort() const;
    const dcss_property *find(PermString name) const;
    inline dcss_property *find(PermString name);
    void add(PermString name, const String &value);
    bool hard_match_context(const delt *e) const;
    void parse_border(const String &str, const char *s, const char *send, const String &prefix);
    void parse_shadow(const String &str, const char *s, const char *send);
    void parse_background(const String &str, const char *s, const char *send);
    void parse_box(const String &str, const char *s, const char *send, const String &prefix);
    void parse_spacing(const String &str, const char *s, const char *send);

    friend class dcss_set;
    
};


class dcss_set { public:

    dcss_set(dcss_set *below);
    ~dcss_set();

    void parse(const String &str);
    void add(dcss *s);

    ref_ptr<delt_style> elt_style(const delt *e);
    inline ref_ptr<dport_style> port_style(const delt *e, bool isoutput, int port, int processing);
    ref_ptr<dqueue_style> queue_style(const delt *e);
    double vpixel(PermString name, const delt *e) const;

    static dcss_set *default_set();
    
  private:

    dcss_set *_below;
    unsigned _selector_index;
    Vector<dcss *> _s;
    
    bool _frozen;

    ref_ptr<dport_style> _generic_port_styles[14];
    bool _all_generic_ports;

    // XXX cache eviction
    HashMap<String, ref_ptr<delt_style> > _etable;
    ref_ptr<delt_style> _generic_elt_styles[16];

    HashMap<String, ref_ptr<dqueue_style> > _qtable;

    void mark_change();
    void collect_port_styles(const delt *e, bool isoutput, int port,
			     int processing, Vector<dcss *> &result,
			     int &generic);
    void collect_elt_styles(const delt *e, Vector<dcss *> &result,
			    bool &generic) const;
    ref_ptr<dport_style> hard_port_style(const delt *e, bool isoutput, int port,
					 int processing);
    
};



extern const double white_color[4];

inline dcss_property *dcss::find(PermString name)
{
    const dcss *ds = this;
    return const_cast<dcss_property *>(ds->find(name));
}

inline ref_ptr<dport_style> dcss_set::port_style(const delt *e, bool isoutput,
						 int port, int processing)
{
    if (_all_generic_ports && _generic_port_styles[7*isoutput + processing])
	return _generic_port_styles[7*isoutput + processing];
    return hard_port_style(e, isoutput, port, processing);
}

inline bool operator<(const dcss &a, const dcss &b)
{
    int as = a.selector().specificity(), bs = b.selector().specificity();
    return (as < bs || (as == bs && a.selector_index() < b.selector_index()));
}

inline double dcss_property::vpixel(const dcss_set *dcs, PermString relative_to,
				    const delt *e) const
{
    change_relative_pixel();
    if (_t == t_pixel)
	return _v.d;
    else if (_t == t_relative)
	return _v.d * dcs->vpixel(relative_to, e);
    else
	return 0;
}

}
#endif
