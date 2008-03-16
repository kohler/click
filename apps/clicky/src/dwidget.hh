#ifndef CLICKY_DWIDGET_HH
#define CLICKY_DWIDGET_HH 1
#include <gtk/gtk.h>
#include <vector>
#include "rectangle.hh"
#include "rectsearch.hh"
#include "ref.hh"
#include <clicktool/elementt.hh>
class Bitvector;
class ProcessingT;
namespace clicky {
class wdiagram;
class wmain;
class handler_value;
class delt;
class dconn;
class dcss_set;
class delt_style;
class dport_style;
class dqueue_style;
class ddecor;

struct dcontext {
    wdiagram *d;
    PangoLayout *pl;
    String pl_font;
    unsigned generation;
    
    cairo_t *cr;
    int scale_step;

    operator cairo_t *() const {
	return cr;
    }
    operator PangoLayout *() const {
	return pl;
    }

    void set_font_description(const String &font);
};


enum { dw_elt = 0, dw_conn = 1 };

class dwidget : public rectangle { public:

    dwidget(int type, int z_index)
	: _type(type), _z_index(z_index) {
    }

    int type() const {
	return _type;
    }

    inline delt *cast_elt();
    inline const delt *cast_elt() const;
    inline dconn *cast_conn();
    inline const dconn *cast_conn() const;

    int z_index() const {
	return _z_index;
    }
    
    static bool z_index_less(const dwidget *a, const dwidget *b) {
	return a->_z_index < b->_z_index;
    }

    static bool z_index_greater(const dwidget *a, const dwidget *b) {
	return a->_z_index > b->_z_index;
    }

    inline void draw(dcontext &dx);
    
  private:

    int _type;
    int _z_index;
    
};


class dconn : public dwidget { public:

    dconn(delt *fe, int fp, delt *te, int tp, int z_index)
	: dwidget(dw_conn, z_index), _from_elt(fe), _from_port(fp),
	  _to_elt(te), _to_port(tp) {
    }

    bool visible() const;
    
    bool layout();
    void draw(dcontext &dcx);

  private:

    delt *_from_elt;
    int _from_port;
    delt *_to_elt;
    int _to_port;

};


enum {
    deg_none = -1,
    deg_main = 0,
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

    class layoutelt;

    delt(delt *parent, int z_index)
	: dwidget(dw_elt, z_index), _e(0), _decor(0), _parent(parent),
	  _split(0), _visible(false), _displayed(false), _layout(false),
	  _aligned(true), _handler_markup(false), _markup_changed(false),
	  _driver(false), _split_inputs(false), _orientation(0), _highlight(0),
	  _drawn_highlight(0), _depth(parent ? parent->_depth + 1 : 0),
	  _contents_width(0), _contents_height(0) {
	_portoff[0] = _portoff[1] = 0;
	_width = _height = 0;
    }
    ~delt();

    delt *parent() const {
	return _parent;
    }
    
    int orientation() const {
	return _orientation;
    }
    bool vertical() const {
	return side_vertical(_orientation);
    }
    bool displayed() const {
	return _displayed;
    }
    bool visible() const {
	return _visible;
    }
    void set_orientation(int orientation) {
	assert(orientation >= side_top && orientation <= side_left);
	_orientation = orientation;
    }
    delt *visible_split() const {
	return _split && _split->visible() ? _split : 0;
    }

    bool root() const {
	return !_e;
    }
    const String &name() const {
	return _e->name();
    }
    String type_name() const {
	return _e->type_name();
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
    bool primitive() const {
	return _elt.size() == 0;
    }
    bool driver() const {
	return _driver;
    }

    double contents_width() const {
	return _contents_width;
    }
    double contents_height() const {
	return _contents_height;
    }

    double min_width() const;
    double min_height() const;

    double shadow(int side) const;

    int highlights() const {
	return _highlight;
    }
    bool highlighted(int htype) const {
	return (_highlight & (1 << htype)) != 0;
    }
    void highlight(int htype) {
	_highlight |= 1 << htype;
	if (_split)
	    _split->_highlight |= 1 << htype;
    }
    void unhighlight(int htype) {
	_highlight &= ~(1 << htype);
	if (_split)
	    _split->_highlight &= ~(1 << htype);
    }

    // creating
    void create(RouterT *router, ProcessingT *processing,
		HashMap<String, delt *> &collector, Vector<ElementT *> &epath,
		int &z_index);
    
    // gadgets
    void notify_read(wdiagram *d, handler_value *hv);
    
    int find_gadget(wdiagram *d, double window_x, double window_y) const;

    void layout_main(dcontext &dx, RouterT *router);
    void layout_recompute_bounds();

    void remove(rect_search<dwidget> &rects, rectangle &bounds);
    void insert(rect_search<dwidget> &rects,
		dcss_set *dcs, rectangle &bounds);

    // dragging
    enum { drag_threshold = 8 };// amount after which recalculate layout
    void drag_prepare();
    void drag_shift(wdiagram *d, const point &delta);
    void drag_size(wdiagram *d, const point &delta, int direction);
    bool drag_canvas_changed(const rectangle &canvas) const;

    // drawing
    point input_position(int port, dport_style *dps) const;
    point output_position(int port, dport_style *dps) const;
    void draw(dcontext &dx);

    // handlers
    bool expand_handlers(wmain *w);
    handler_value *handler_interest(wmain *w, const String &hname,
				    bool autorefresh = false, int autorefresh_period = 0, bool always = false);
    
    void prepare_router(wdiagram *d, RouterT *router, ProcessingT *processing,
			HashMap<String, delt *> &collector,
			Vector<ElementT *> &path, int &z_index);
    
  private:
    
    ElementT *_e;
    String _processing_code;
    std::vector<delt *> _elt;
    std::vector<dconn *> _conn;
    ref_ptr<delt_style> _des;
    ref_ptr<dqueue_style> _dqs;
    ddecor *_decor;
    unsigned _generation;
    double *_portoff[2];
    double _ports_length[2];
    delt *_parent;
    delt *_split;
    
    String _flat_name;
    String _flat_config;
    String _markup;

    bool _visible;
    bool _displayed;
    bool _layout;
    bool _aligned;
    bool _handler_markup;
    bool _markup_changed;
    bool _driver;
    bool _split_inputs;
    int _orientation;
    uint8_t _highlight;
    uint8_t _drawn_highlight;
    uint16_t _depth;
    int _row;
    int _rowpos;

    rectangle _xrect;

    double _markup_width;
    double _markup_height;
    
    double _contents_width;
    double _contents_height;
    
    delt(const delt &);
    delt &operator=(const delt &);

    void prepare(wdiagram *d, ElementT *e, ProcessingT *processing,
		 HashMap<String, delt *> &collector, Vector<ElementT *> &path,
		 int &z_index);

    void layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc);
    void position_contents_scc(RouterT *);
    void position_contents_dot(RouterT *, dcss_set *dcs, ErrorHandler *);
    void position_contents_first_heuristic(RouterT *r);

    void layout_contents(dcontext &dcx, RouterT *router);
    void layout_ports(dcss_set *dcs);
    void layout(dcontext &dcx);
    bool parse_markup(wmain *w);
    void restyle(dcontext &dcx, bool do_markup = true);
    void redecorate(dcontext &dcx);
    void layout_complete(dcontext &dcx, double dx, double dy);
    void layout_compound_ports(dcss_set *dcs);
    void union_bounds(rectangle &r, bool self) const;

    inline double port_position(bool isoutput, int port,
				double side_length) const;
    double hard_port_position(bool isoutput, int port,
			      double side_length) const;
    void draw_port(dcontext &dx, dport_style *dps, point p, bool isoutput);
    void border_path(dcontext &dx, bool closed) const;
    void clip_to_border(dcontext &dx) const;

    void draw_background(dcontext &dx);
    void draw_text(dcontext &dx);
    void draw_ports(dcontext &dx);
    void draw_outline(dcontext &dx);
    void draw_drop_shadow(dcontext &dx);
        
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

inline void dwidget::draw(dcontext &dx) {
    assert(_type == dw_elt || _type == dw_conn);
    if (_type == dw_elt)
	static_cast<delt *>(this)->draw(dx);
    else
	static_cast<dconn *>(this)->draw(dx);
}


inline bool dconn::visible() const {
    return _from_elt->displayed() && _to_elt->displayed();
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

}
#endif
