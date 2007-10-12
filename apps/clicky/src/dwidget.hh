#ifndef CLICKY_DWIDGET_HH
#define CLICKY_DWIDGET_HH 1
#include <gtk/gtk.h>
#include <vector>
#include "rectangle.hh"
#include "rectsearch.hh"
#include <clicktool/elementt.hh>
class Bitvector;
class ProcessingT;
namespace clicky {
class wdiagram;
class wmain;
class handler_value;
class dstyle;
class delt;
class dconn;

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
    
    static bool z_index_less(const dwidget *a, const dwidget *b) {
	return a->_z_index < b->_z_index;
    }

    static bool z_index_greater(const dwidget *a, const dwidget *b) {
	return a->_z_index > b->_z_index;
    }

    inline void draw(wdiagram *d, cairo_t *cr, PangoLayout *pl);
    
  private:

    int _type;
    int _z_index;
    
};


class dconn : public dwidget { public:

    dconn(delt *fe, int fp, delt *te, int tp, int z_index)
	: dwidget(dw_conn, z_index), _from_elt(fe), _from_port(fp),
	  _to_elt(te), _to_port(tp) {
    }

    void layout(const dstyle &style);
    void draw(wdiagram *d, cairo_t *cr);

  private:

    delt *_from_elt;
    int _from_port;
    delt *_to_elt;
    int _to_port;

};

enum { esflag_queue = 1,
       esflag_fullness = 2 };

enum { deg_none = -1,
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
	: dwidget(dw_elt, z_index), _e(0), _parent(parent), _style(0),
	  _visible(true), _layout(false), _expanded(true),
	  _show_class(true), _vertical(true), _highlight(0),
	  _depth(parent ? parent->_depth + 1 : 0),
	  _contents_width(0), _contents_height(0) {
    }
    ~delt();

    bool vertical() const {
	return _vertical;
    }
    bool visible() const {
	return _visible;
    }

    const String &flat_name() const {
	return _flat_name;
    }

    double contents_width() const {
	return _contents_width;
    }
    double contents_height() const {
	return _contents_height;
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

    // gadgets
    void add_gadget(wdiagram *d, int gadget);
    void remove_gadget(wdiagram *d, int gadget);
    void notify_read(wdiagram *d, handler_value *hv);
    
    int find_gadget(wdiagram *d, double window_x, double window_y) const;

    enum { lay_border = 4 };	// border around layout
    void layout_main(wdiagram *d, RouterT *router, PangoLayout *pl);
    void layout_recompute_bounds();

    void remove(rect_search<dwidget> &rects, rectangle &rect);
    void insert(rect_search<dwidget> &rects, const dstyle &style, rectangle &rect);

    // dragging
    enum { drag_threshold = 8 };// amount after which recalculate layout
    void drag_prepare();
    void drag_shift(wdiagram *d, const point &delta);
    void drag_size(wdiagram *d, const point &delta, int direction);
    bool drag_canvas_changed(const rectangle &canvas) const;
    
    static void port_offsets(double side_length, int nports, const dstyle &style, double &offset0, double &separation);
    inline double port_position(int port, int nports, double side, const dstyle &style) const;
    inline void input_position(int port, const dstyle &style, double &x_result, double &y_result) const;
    inline void output_position(int port, const dstyle &style, double &x_result, double &y_result) const;
    void draw_input_port(cairo_t *, const dstyle &, double, double, int processing);
    void draw_output_port(cairo_t *, const dstyle &, double, double, int processing);
    void clip_to_border(cairo_t *cr, double shift) const;
    void draw_outline(wdiagram *cd, cairo_t *cr, PangoLayout *pl, double shift);
    void draw_text(wdiagram *cd, cairo_t *cr, PangoLayout *pl, double shift);
    void draw(wdiagram *cd, cairo_t *cr, PangoLayout *pl);
    
    void prepare_router(RouterT *router, ProcessingT *processing,
			HashMap<String, delt *> &collector,
			Vector<ElementT *> &path, int &z_index);
    
  private:
    
    ElementT *_e;
    String _processing_code;
    std::vector<delt *> _elt;
    std::vector<dconn *> _conn;
    delt *_parent;
    String _flat_name;
    String _flat_config;
    int _style;

    bool _visible;
    bool _layout;
    bool _expanded;
    bool _show_class;
    bool _vertical;
    uint8_t _highlight;
    uint16_t _depth;
    int _row;
    int _rowpos;

    rectangle _xrect;

    double _name_raw_width;
    double _name_raw_height;
    double _class_raw_width;
    double _class_raw_height;
    double _contents_width;
    double _contents_height;

    double _hvalue_fullness;
    
    delt(const delt &);
    delt &operator=(const delt &);

    void gadget_fullness(wdiagram *d);
    bool expand_handlers(wmain *w);

    void prepare(ElementT *e, ProcessingT *processing,
		 HashMap<String, delt *> &collector, Vector<ElementT *> &path,
		 int &z_index);

    void layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc);
    void position_contents_scc(RouterT *);
    void position_contents_dot(RouterT *, const dstyle &, ErrorHandler *);
    void position_contents_first_heuristic(RouterT *, const dstyle &);

    void layout_contents(wdiagram *d, RouterT *router, PangoLayout *pl);
    void layout(wdiagram *d, PangoLayout *pl);
    void layout_complete(wdiagram *d, double dx, double dy);
    void layout_compound_ports(const dstyle &es);
    void union_bounds(rectangle &r, bool self) const;
        
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

inline void dwidget::draw(wdiagram *d, cairo_t *cr, PangoLayout *pl) {
    assert(_type == dw_elt || _type == dw_conn);
    if (_type == dw_elt)
	static_cast<delt *>(this)->draw(d, cr, pl);
    else
	static_cast<dconn *>(this)->draw(d, cr);
}
    
inline double delt::port_position(int port, int nports, double side, const dstyle &style) const
{
    if (port >= nports)
	return side;
    else {
	double offset0, separation;
	port_offsets(side, nports, style, offset0, separation);
	return offset0 + separation * port;
    }
}

inline void delt::input_position(int port, const dstyle &style, double &x_result, double &y_result) const
{
    if (_vertical) {
	x_result = x1() + port_position(port, _e->ninputs(), width(), style);
	y_result = y1() + 0.5;
    } else {
	x_result = x1() + 0.5;
	y_result = y1() + port_position(port, _e->ninputs(), height(), style);
    }
}

inline void delt::output_position(int port, const dstyle &style, double &x_result, double &y_result) const
{
    if (_vertical) {
	x_result = x1() + port_position(port, _e->noutputs(), width(), style);
	y_result = y2() + 0.5;
    } else {
	x_result = x2() + 0.5;
	y_result = y1() + port_position(port, _e->noutputs(), height(), style);
    }
}

}
#endif
