#ifndef CLICKY_DWIDGET_HH
#define CLICKY_DWIDGET_HH 1
#include <gtk/gtk.h>
#include <vector>
#include "rectangle.hh"
#include "rectsearch.hh"
#include "ref.hh"
#include "dstyle.hh"
#include <click/integers.hh>
#include <clicktool/elementt.hh>
#include <clicktool/routert.hh>
class Bitvector;
class ProcessingT;
class ScopeChain;
namespace clicky {
class wdiagram;
class wmain;
class handler_value;
class delt;
class dconn;
class delt_style;
class delt_size_style;
class dport_style;
class dqueue_style;
class ddecor;
class crouter;

enum {
    desplit_outputs = 0,
    desplit_inputs = 1
};

struct dcontext {

    crouter *cr;
    PangoLayout *pl;
    cairo_t *cairo;

    unsigned generation;
    int scale_step;
    double scale;

    String pl_font;
    double penumbra;

    dcontext(crouter *cr, PangoLayout *pl, cairo_t *cairo,
	     unsigned generation, int scale_step, double scale);

    static unsigned step_generation();

    operator cairo_t *() const {
	return cairo;
    }
    operator PangoLayout *() const {
	return pl;
    }

    void set_font_description(const String &font);

};


enum { dw_elt = 0, dw_conn = 1 };

class dwidget : public rectangle { public:

    dwidget(int type, int z_index = 0)
	: _type(type), _z_index(z_index) {
    }

    int type() const {
	return _type;
    }

    String unparse() const;

    inline delt *cast_elt();
    inline const delt *cast_elt() const;
    inline dconn *cast_conn();
    inline const dconn *cast_conn() const;

    int z_index() const {
	return _z_index;
    }
    void set_z_index(int z_index) {
	_z_index = z_index;
    }

    static bool z_index_less(const dwidget *a, const dwidget *b) {
	return a->_z_index < b->_z_index;
    }

    static bool z_index_greater(const dwidget *a, const dwidget *b) {
	return a->_z_index > b->_z_index;
    }

    inline void draw(dcontext &dcx);

  private:

    int _type;
    int _z_index;

};


class dconn : public dwidget { public:

    inline dconn(delt *fe, int fp, delt *te, int tp);
    static inline dconn *make(delt *fe, int fp, delt *te, int tp);

    delt *elt(bool isoutput) const {
	return _elt[isoutput];
    }
    int port(bool isoutput) const {
	return _port[isoutput];
    }
    delt *from_elt() const {
	return _elt[1];
    }
    int from_port() const {
	return _port[1];
    }
    delt *to_elt() const {
	return _elt[0];
    }
    int to_port() const {
	return _port[0];
    }

    bool change_count(unsigned new_count);

    bool visible() const;

    bool layout();
    void draw(dcontext &dcx);

  private:

    delt *_elt[2];
    int _port[2];
    unsigned _count_last;
    unsigned _count_change;
    Vector<point> _route;

    static inline int change_display(unsigned change);

    friend class delt;

};


enum {
    deg_none = -1,
    deg_element = 0,
    deg_border_top = 1,
    deg_border_bot = 2,
    deg_border_lft = 3,
    deg_corner_ulft = deg_border_top + deg_border_lft,
    deg_corner_llft = deg_border_bot + deg_border_lft,
    deg_border_rt = 6,
    deg_corner_urt = deg_border_top + deg_border_rt,
    deg_corner_lrt = deg_border_bot + deg_border_rt
};

class delt : public dwidget { public:

    delt(delt *parent = 0, int z_index = 0)
	: dwidget(dw_elt, z_index), _e(0), _resolved_router(0),
	  _decor(0), _generation(0),
	  _parent(parent), _split(0),
	  _visible(false), _display(dedisp_placeholder),
	  _aligned(true), _primitive(false), _split_copy(false), _port_split(0),
	  _flow_split(false), _split_type(0),
	  _des_sensitivity(0), _dess_sensitivity(0), _dps_sensitivity(0),
	  _markup_sensitivity(0),
	  _highlight(0), _drawn_highlight(0),
	  _depth(parent ? parent->_depth + 1 : 0),
	  _markup_width(-1024), _markup_height(-1024),
	  _contents_width(0), _contents_height(0) {
	_portoff[0] = _portoff[1] = 0;
	_width = _height = 0;
    }
    ~delt();

    delt *parent() const {
	return _parent;
    }

    int orientation() const {
	return _dess->orientation;
    }
    bool vertical() const {
	return side_vertical(orientation());
    }
    bool horizontal() const {
	return side_horizontal(orientation());
    }
    int display() const {
	return _display;
    }
    bool visible() const {
	return _visible;
    }
    delt *split() const {
	return _split;
    }
    char split_type() const {
	return _split_type;
    }
    delt *next_split(delt *base) const {
	return _split != base ? _split : 0;
    }
    delt *find_split(int split_type) {
	delt *d = this;
	do {
	    if (d->_split_type == split_type)
		return d;
	    d = d->_split;
	} while (d && d != this);
	return 0;
    }
    const delt *find_split(int split_type) const {
	const delt *d = this;
	do {
	    if (d->_split_type == split_type)
		return d;
	    d = d->_split;
	} while (d && d != this);
	return 0;
    }
    inline delt *find_port_container(bool isoutput, int port) {
	if (_flow_split)
	    return find_flow_split(isoutput, port);
	else if (_port_split)
	    return find_split(isoutput ? desplit_outputs : desplit_inputs);
	else
	    return this;
    }
    dconn *find_connection(bool isoutput, int port);

    bool root() const {
	return !_parent;
    }
    bool fake() const {
	return !_e;
    }
    String display_name() const;
    const String &name() const {
	return (_e ? _e->name() : String::make_empty());
    }
    String type_name() const {
	return (_e ? _e->type_name() : String::make_empty());
    }
    const String &flat_name() const {
	return _flat_name;
    }
    int ninputs() const {
	return _e->ninputs();
    }
    int noutputs() const {
	return _e->noutputs();
    }
    int eindex() const {
	return _e->eindex();
    }
    RouterT *router() const {
	return _e->router();
    }
    bool primitive() const {
	return _primitive;
    }
    bool passthrough() const {
	return _resolved_router && _resolved_router->nelements() == 2;
    }

    double contents_width() const {
	return _contents_width;
    }
    double contents_height() const {
	return _contents_height;
    }

    double min_width() const;
    double min_height() const;

    double shadow(double scale, int side) const;

    int highlights() const {
	return _highlight;
    }
    bool highlighted(int htype) const {
	return (_highlight & (1 << htype)) != 0;
    }
    void highlight(int htype) {
	_highlight |= 1 << htype;
    }
    void unhighlight(int htype) {
	_highlight &= ~(1 << htype);
    }

    // creating
    void create(RouterT *router, ProcessingT *processing,
		HashTable<String, delt *> &collector, ScopeChain &chain);

    // gadgets
    void notify_read(wdiagram *d, handler_value *hv);

    int find_gadget(wdiagram *d, double window_x, double window_y) const;

    void layout_main(dcontext &dcx);
    void layout_recompute_bounds();

    void remove(rect_search<dwidget> &rects, rectangle &bounds);
    void insert(rect_search<dwidget> &rects, crouter *cr, rectangle &bounds);
    void insert_all(rect_search<dwidget> &rects);

    // dragging
    enum { drag_threshold = 8 };// amount after which recalculate layout
    void drag_prepare();
    void drag_shift(wdiagram *d, const point &delta);
    void drag_size(wdiagram *d, const point &delta, int direction);
    bool drag_canvas_changed(const rectangle &canvas) const;

    // drawing
    point input_position(int port, dport_style *dps, bool here = false) const;
    point output_position(int port, dport_style *dps, bool here = false) const;
    void draw(dcontext &dcx);

    // handlers
    handler_value *handler_interest(crouter *cr, const String &hname,
				    bool autorefresh = false, int autorefresh_period = 0, bool always = false);

    void fill(crouter *cr, const String &name, int z_index,
	      HashTable<String, delt *> &collector);
    void create_elements(crouter *cr, RouterT *router, ProcessingT *processing,
			 HashTable<String, delt *> *collector,
			 ScopeChain &chain);
    void create_connections(crouter *cr);
    int assign_z_indexes(int z);

    // element contents
    class iterator {
	iterator()
	    : _e(0), _ep(), _parent(0) {
	}
	typedef delt *(iterator::*unspecified_bool_type)() const;
	operator unspecified_bool_type() const {
	    return _e ? &iterator::operator-> : 0;
	}
	bool operator!() const {
	    return !_e;
	}
	delt *operator->() const {
	    return _e;
	}
	delt &operator*() const {
	    return *_e;
	}
	void operator++() {
	    assert(_e);
	    if (!(_e = _e->next_split(*_ep)))
		if (++_ep != _parent->_elt.end())
		    _e = *_ep;
	}
	bool operator==(const iterator &x) const {
	    return _e == x._e;
	}
	bool operator!=(const iterator &x) const {
	    return _e != x._e;
	}
      private:
	delt *_e;
	std::vector<delt *>::const_iterator _ep;
	const delt *_parent;

	iterator(const delt *parent)
	    : _ep(parent->_elt.begin()), _parent(parent) {
	    _e = (_ep != parent->_elt.end() ? *_ep : 0);
	}
	friend class delt;
    };
    iterator begin_contents() const {
	return iterator(this);
    }
    iterator end_contents() const {
	return iterator();
    }

  private:

    ElementT *_e;
    String _processing_code;
    String _flow_code;
    RouterT *_resolved_router;
    std::vector<delt *> _elt;
    std::vector<dconn *> _conn;
    ref_ptr<delt_style> _des;
    ref_ptr<delt_size_style> _dess;
    ddecor *_decor;
    unsigned _generation;
    double *_portoff[2];
    double _ports_length[2];
    double _ports_width[2];
    delt *_parent;
    delt *_split;

    String _flat_name;
    String _flat_config;
    String _markup;

    bool _visible;
    int8_t _display;
    bool _aligned : 1;
    bool _primitive : 1;
    bool _split_copy : 1;
    unsigned _port_split : 2;
    bool _flow_split : 1;
    uint8_t _split_type;
    unsigned _des_sensitivity : 2;
    unsigned _dess_sensitivity : 2;
    unsigned _dps_sensitivity : 2;
    unsigned _markup_sensitivity : 2;
    uint8_t _highlight;
    uint8_t _drawn_highlight;
    uint16_t _depth;

    rectangle _xrect;

    double _markup_width;
    double _markup_height;

    double _contents_width;
    double _contents_height;

    struct delt_conn {
	delt *from;
	int from_port;
	delt *to;
	int to_port;
	delt_conn(delt *f, int fp, delt *t, int tp)
	    : from(f), from_port(fp), to(t), to_port(tp) {
	    assert(f && t);
	}
    };

    delt(const delt &);
    delt &operator=(const delt &);

    static delt *create(ElementT *e, delt *parent,
			crouter *cr, ProcessingT *processing,
			HashTable<String, delt *> *collector,
			ScopeChain &chain);
    delt *create_split(crouter *cr, int split_type);
    void create_connections(std::vector<delt_conn> &cc, crouter *cr) const;

    void unparse_contents_dot(StringAccum &sa, crouter *cr, HashTable<int, delt *> &z_index_lookup) const;
    void position_contents_dot(crouter *cr, ErrorHandler *errh);
    const char *parse_connection_dot(delt *e1, const HashTable<int, delt *> &z_index_lookup, const char *s, const char *end);
    void create_bbox_contents(double bbox[4], double mbbox[4], bool include_compound_ports) const;
    void shift_contents(double dx, double dy) const;
    int flow_split_char(bool isoutput, int port) const;
    delt *find_flow_split(bool isoutput, int port) {
	return find_split(flow_split_char(isoutput, port));
    }
    const delt *find_flow_split(bool isoutput, int port) const {
	return find_split(flow_split_char(isoutput, port));
    }

    bool reccss(crouter *cr, int change);
    void layout_contents(dcontext &dcx);
    void layout_ports(dcontext &dcx);
    void layout(dcontext &dcx);
    String parse_markup(const String &text, crouter *cr, int port, int *sensitivity);
    void dimension_markup(dcontext &dcx);
    void redecorate(dcontext &dcx);
    void layout_complete(dcontext &dcx, double dx, double dy);
    void layout_compound_ports_copy(delt *e, bool isoutput);
    void layout_compound_ports(crouter *cr);
    void union_bounds(rectangle &r, bool self) const;

    inline double port_position(bool isoutput, int port,
				double side_length) const;
    double hard_port_position(bool isoutput, int port,
			      double side_length) const;
    void draw_port(dcontext &dcx, dport_style *dps, point p,
		   int port, bool isoutput, double opacity);
    void border_path(dcontext &dcx, bool closed) const;
    void clip_to_border(dcontext &dcx) const;

    void draw_background(dcontext &dcx);
    void draw_text(dcontext &dcx);
    void draw_ports(dcontext &dcx);
    void draw_outline(dcontext &dcx);
    void draw_drop_shadow(dcontext &dcx);

};


inline delt *dwidget::cast_elt() {
    return (_type == dw_elt ? static_cast<delt *>(this) : 0);
}

inline const delt *dwidget::cast_elt() const {
    return (_type == dw_elt ? static_cast<const delt *>(this) : 0);
}

inline dconn *dwidget::cast_conn() {
    return (_type == dw_conn ? static_cast<dconn *>(this) : 0);
}

inline const dconn *dwidget::cast_conn() const {
    return (_type == dw_conn ? static_cast<const dconn *>(this) : 0);
}

inline void dwidget::draw(dcontext &dcx) {
    assert(_type == dw_elt || _type == dw_conn);
    if (_type == dw_elt)
	static_cast<delt *>(this)->draw(dcx);
    else
	static_cast<dconn *>(this)->draw(dcx);
}


inline bool dconn::visible() const {
    // see also dedisp_visible
    return _elt[0]->display() > 0 && _elt[1]->display() > 0;
}

inline double delt::port_position(bool isoutput, int port,
				  double side_length) const
{
    int nports = _e->nports(isoutput);
    if (port < 0 || port >= nports)
	return side_length;
    else if (nports <= 1)
	return side_length / 2;
    else
	return hard_port_position(isoutput, port, side_length);
}

inline dconn::dconn(delt *fe, int fp, delt *te, int tp)
    : dwidget(dw_conn, std::max(fe->z_index(), te->z_index()) + 1),
      _count_last(~0U), _count_change(~0U)
{
    assert(fe->display() != dedisp_placeholder
	   && te->display() != dedisp_placeholder);
    _elt[1] = fe;
    _elt[0] = te;
    assert(_elt[0] && _elt[1]);
    _port[1] = fp;
    _port[0] = tp;
}

inline dconn *dconn::make(delt *fe, int fp, delt *te, int tp)
{
    if ((fe = fe->find_port_container(true, fp))
	&& (te = te->find_port_container(false, tp)))
	return new dconn(fe, fp, te, tp);
    else
	return 0;
}

inline int dconn::change_display(unsigned change)
{
    if (change == ~0U || (change >= 1 && change < 4))
	return 1;
    else if (change == 0)
	return 0;
    else
	return sizeof(unsigned) * 8 - ffs_msb(change);
}

inline double dcss_propmatch::vpixel(const char *n, crouter *cr,
				     const delt *relative_elt) const
{
    assert(name == n);
    return property->vpixel(cr, name, relative_elt->parent());
}

}
#endif
