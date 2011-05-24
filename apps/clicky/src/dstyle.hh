#ifndef CLICKY_DSTYLE_HH
#define CLICKY_DSTYLE_HH 1
#include <gtk/gtk.h>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include "permstr.hh"
#include "ref.hh"
#include <string.h>
class ErrorHandler;
namespace clicky {
class dcss_set;
class delt;
class crouter;
class handler_value;

enum {
    dedisp_none = 0,
    dedisp_normal = 1,
    dedisp_closed = 2,
    dedisp_passthrough = -1,
    dedisp_expanded = -2,
    dedisp_placeholder = -99
};

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
    dshadow_outline = 2,
    dshadow_unscaled_outline = 3
};

enum {
    destyle_normal = 0,
    destyle_queue = 1
};

static inline bool dedisp_visible(int dedisp) {
    // see also dconn::visible
    return dedisp > 0;
}

static inline bool dedisp_children_visible(int dedisp) {
    return dedisp == dedisp_normal || dedisp == dedisp_expanded;
}

enum {
    dpdisp_none = 0,
    dpdisp_inputs = 1,
    dpdisp_outputs = 2,
    dpdisp_both = 3
};

struct dport_style : public enable_ref_ptr {
    int display;
    int shape;
    double length;
    double width;
    double color[4];
    int border_style;
    double border_width;
    double border_color[4];
    double margin[4];
    double edge_padding;
    String text;
    String font;
};

struct delt_size_style : public enable_ref_ptr {
    double border_width;
    double padding[4];
    double margin[4];
    double min_width;
    double min_height;
    double min_length;
    double height_step;
    double scale;
    double queue_stripe_spacing;
    int orientation;
};

struct delt_style : public enable_ref_ptr {
    double color[4];
    double background_color[4];
    int border_style;
    double border_color[4];
    int shadow_style;
    double shadow_width;
    double shadow_color[4];
    double queue_stripe_color[4];
    int queue_stripe_style;
    double queue_stripe_width;
    int style;
    String text;
    String font;
    int display;
    int port_split;
    String flow_split;
    String decorations;
};

struct dhandler_style : public enable_ref_ptr {
    int flags_mask;
    int flags;
    int autorefresh_period;
};

struct dfullness_style : public enable_ref_ptr {
    String length;
    String capacity;
    double color[4];
    int autorefresh;
    int autorefresh_period;
};

enum {
    dactivity_absolute = 0,
    dactivity_rate = 1
};

struct dactivity_style : public enable_ref_ptr {
    String handler;
    Vector<double> colors;
    int type;
    double min_value;
    double max_value;
    double decay;
    int autorefresh;
    int autorefresh_period;
};

enum {
    dhlt_hover = 0,
    dhlt_click = 1,
    dhlt_pressed = 2,
    dhlt_rect_click = 3
};

enum {
    dsense_highlight = 1,
    dsense_handler = 2,
    dsense_always = 4
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

    void unparse(StringAccum &sa) const;
    String unparse() const;

    bool match(crouter *cr, const delt *e, int *sensitivity = 0) const;

    bool match_port(bool isoutput, int port, int processing) const;

    bool match(const handler_value *hv) const;

    bool match_decor(PermString decor) const {
	return !_klasses.size() && !_name && _type
	    && (_type_glob ? type_glob_match(decor) : _type == decor);
    }

    bool generic_port() const {
	return !_name;
    }
    bool generic_elt() const {
	return !_type && !_name && !_klasses.size();
    }
    bool generic_handler() const {
	return !_type && !_name && !_klasses.size();
    }
    bool generic_decor() const {
	return !_type_glob && !_name && !_klasses.size();
    }
    bool is_media() const;

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

    bool type_glob_match(PermString decor) const;

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
	t_seconds,
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
    double vpixel(crouter *cr, PermString relative_to, const delt *e) const;
    double vrelative() const {
	return (change_type(t_relative) ? _v.d : 0);
    }
    double vseconds() const {
	return (change_type(t_seconds) ? _v.d : 0);
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
    static const double transparent_color[4];

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

};

struct dcss_propmatch {
    PermString name;
    const dcss_property *property;

    const String &vstring(const char *n) const {
	assert(name == n);
	return property->vstring();
    }
    double vnumeric(const char *n) const {
	assert(name == n);
	return property->vnumeric();
    }
    double vpixel(const char *n) const {
	assert(name == n);
	return property->vpixel();
    }
    double vpixel(const char *n, double relative_to) const {
	assert(name == n);
	return property->vpixel(relative_to);
    }
    inline double vpixel(const char *n, crouter *cr, const delt *relative_elt) const;
    double vpixel(const char *n, crouter *cr, PermString relative_name,
		  const delt *relative_elt) const {
	assert(name == n);
	return property->vpixel(cr, relative_name, relative_elt);
    }
    double vrelative(const char *n) const {
	assert(name == n);
	return property->vrelative();
    }
    double vseconds(const char *n) const {
	assert(name == n);
	return property->vseconds();
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
    bool match_context(crouter *cr, const delt *e, int *sensitivity = 0) const {
	return !_context.size() || hard_match_context(cr, e, sensitivity, false);
    }
    bool strict_match_context(crouter *cr, const delt *e, int *sensitivity = 0) const {
	return !_context.size() || hard_match_context(cr, e, sensitivity, true);
    }
    unsigned pflags() const {
	return _pflags;
    }

    const char *parse(const String &str, const String &media, const char *s);

    void unparse_selector(StringAccum &sa) const;
    String unparse_selector() const;
    void unparse(StringAccum &sa) const;
    String unparse() const;

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
    unsigned _pflags;
    mutable Vector<dcss_property> _de;
    mutable bool _sorted;
    dcss *_next;

    void sort() const;
    const dcss_property *find(PermString name) const;
    inline dcss_property *find(PermString name);
    void add(PermString name, const String &value);
    bool hard_match_context(crouter *cr, const delt *e, int *sensitivity, bool strict) const;
    void parse_border(const String &str, const char *s, const char *send, const String &prefix);
    void parse_shadow(const String &str, const char *s, const char *send);
    void parse_background(const String &str, const char *s, const char *send);
    void parse_box(const String &str, const char *s, const char *send, const String &prefix);
    void parse_split(const String &str, const char *s, const char *send);

    friend class dcss_set;

};


class dcss_set { public:

    dcss_set(const String &text, const String &media);
    dcss_set(dcss_set *below);
    ~dcss_set();

    const String &media() const {
	return _media;
    }
    const String &text() const {
	return _text;
    }
    dcss_set *below() const {
	return _below;
    }

    dcss_set *remedia(const String &media);

    static String expand_imports(const String &text, const String &filename, ErrorHandler *errh = 0);

    void parse(const String &text);
    void add(dcss *s);

    ref_ptr<delt_style> elt_style(crouter *cr, const delt *e, int *sensitivity = 0);
    ref_ptr<delt_size_style> elt_size_style(crouter *cr, const delt *e, int *sensitivity = 0);
    ref_ptr<dport_style> port_style(crouter *cr, const delt *e, bool isoutput, int port, int processing);
    ref_ptr<dhandler_style> handler_style(crouter *cr, const delt *e, const handler_value *hv);
    ref_ptr<dfullness_style> fullness_style(PermString decor, crouter *cr, const delt *e);
    ref_ptr<dactivity_style> activity_style(PermString decor, crouter *cr, const delt *e);

    double vpixel(PermString name, crouter *cr, const delt *e) const;
    String vstring(PermString name, PermString decor, crouter *cr, const delt *e) const;

    static dcss_set *default_set(const String &media);

  private:

    String _text;
    String _media;
    dcss_set *_media_next;

    dcss_set *_below;
    unsigned _selector_index;
    Vector<dcss *> _s;

    bool _frozen;

    // XXX cache eviction
    HashTable<String, ref_ptr<dport_style> > _ptable;
    HashTable<String, ref_ptr<delt_size_style> > _estable;
    HashTable<String, ref_ptr<delt_style> > _etable;
    HashTable<String, ref_ptr<dhandler_style> > _htable;
    HashTable<String, ref_ptr<dfullness_style> > _ftable;
    HashTable<String, ref_ptr<dactivity_style> > _atable;

    void mark_change();
    dcss *ccss_list(const String &str) const;
    void collect_port_styles(crouter *cr, const delt *e, bool isoutput,
			     int port, int processing, Vector<dcss *> &result);
    void collect_elt_styles(crouter *cr, const delt *e, int pflag,
			    Vector<dcss *> &result, int *sensitivity) const;
    void collect_handler_styles(crouter *cr, const handler_value *hv,
				const delt *e, Vector<dcss *> &result,
				bool &generic) const;
    void collect_decor_styles(PermString decor, crouter *cr, const delt *e,
			      Vector<dcss *> &result, bool &generic) const;

};



extern const double white_color[4];

inline dcss_property *dcss::find(PermString name)
{
    const dcss *ds = this;
    return const_cast<dcss_property *>(ds->find(name));
}

inline bool operator<(const dcss &a, const dcss &b)
{
    int as = a.selector().specificity(), bs = b.selector().specificity();
    return (as < bs || (as == bs && a.selector_index() < b.selector_index()));
}

}
#endif
