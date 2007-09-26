#ifndef CLICKY_DIAGRAM_HH
#define CLICKY_DIAGRAM_HH 1
#include <gtk/gtk.h>
#include <vector>
#include "rectangle.hh"
#include "rectsearch.hh"
#include <clicktool/elementt.hh>
class RouterWindow;
class Bitvector;

class ClickyDiagram { public:

    ClickyDiagram(RouterWindow *rw);
    ~ClickyDiagram();

    void router_create(bool incremental, bool always);

    double scale() const {
	return _scale;
    }
    
    void layout();
    
    inline void redraw();
    inline void redraw(rectangle r);

    void display(const String &ename, bool scroll_to);
    void zoom(bool incremental, int amount);

    void on_expose(const GdkRectangle *r, bool clip);
    gboolean on_event(GdkEvent *event);
    
    struct eltstyle {
	double port_layout_length;
	double port_length[2];
	double port_width[2];
	double port_separation;
	double min_port_distance;
	double port_offset;
	double min_port_offset;
	double agnostic_separation;
	double port_agnostic_separation[2];
	double inside_dx;
	double inside_dy;
	double inside_contents_dy;
	double min_height;
	double height_increment;
	double element_dx;
	double element_dy;
    };

  private:

    enum { elt_expand = 4,	// max dimension of drawing outside width-height
	   elt_shadow = 3 };	// shadow width on bottom and right

    enum { htype_hover = 0, htype_click = 1, htype_pressed = 2 };

    enum { i_elt = 0, i_conn = 1 };
    class elt;
    class conn;
    
    class ink : public rectangle { public:
	int _type;
	int _z_index;
	ink(int type, int z_index)
	    : _type(type), _z_index(z_index) {
	}

	int type() const {
	    return _type;
	}

	inline elt *cast_elt();
	inline const elt *cast_elt() const;
	inline conn *cast_conn();
	inline const conn *cast_conn() const;
	
	static bool z_index_less(const ink *e1, const ink *e2) {
	    return e1->_z_index < e2->_z_index;
	}

	static bool z_index_greater(const ink *e1, const ink *e2) {
	    return e1->_z_index > e2->_z_index;
	}
    };

    class conn : public ink { public:
	elt *_from_elt;
	int _from_port;
	elt *_to_elt;
	int _to_port;
	conn(elt *fe, int fp, elt *te, int tp, int z_index)
	    : ink(i_conn, z_index), _from_elt(fe), _from_port(fp),
	      _to_elt(te), _to_port(tp) {
	}

	void finish(const eltstyle &es);
	void draw(ClickyDiagram *cd, cairo_t *cr);
    };
    
    class elt : public ink { public:
	ElementT *_e;
	String _processing_code;
	std::vector<elt *> _elt;
	std::vector<conn *> _conn;
	elt *_parent;
	String _flat_name;
	String _flat_config;

	bool _visible;
	bool _layout;
	bool _expanded;
	bool _show_class;
	uint8_t _highlight;
	uint16_t _depth;
	elt *_next_htype_click;
	int _row;
	int _rowpos;

	rectangle _xrect;

	double _name_raw_width;
	double _name_raw_height;
	double _class_raw_width;
	double _class_raw_height;
	double _contents_width;
	double _contents_height;

	class layoutelt;

	elt(elt *parent, int z_index)
	    : ink(i_elt, z_index), _e(0), _parent(parent), _visible(true),
	      _layout(false), _expanded(true), _show_class(true), _highlight(0),
	      _depth(parent ? parent->_depth + 1 : 0),
	      _next_htype_click(0), _contents_width(0), _contents_height(0) {
	}
	~elt();

	void layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc);
	void position_contents_scc(RouterT *);
	void position_contents_dot(RouterT *, const eltstyle &, ErrorHandler *);
	void position_contents_first_heuristic(RouterT *, const eltstyle &);
	void layout_contents(RouterT *, ClickyDiagram *, PangoLayout *);
	void layout(ClickyDiagram *, PangoLayout *);

	void finish(const eltstyle &es, double dx, double dy, rect_search<ink> &rects);
	void finish_compound(const eltstyle &es);
	
	void remove(rect_search<ink> &rects, rectangle &rect);
	void insert(rect_search<ink> &rects, const eltstyle &style, rectangle &rect);

	void drag_prepare();
	void drag_shift(double dx, double dy, ClickyDiagram *cd);
	
	static void port_position(double side_length, int port_type, int nports, const eltstyle &style, double &offset0, double &separation);
	inline void input_position(int port, const eltstyle &style, double &x, double &y);
	inline void output_position(int port, const eltstyle &style, double &x, double &y);
	void draw_input_port(cairo_t *, const eltstyle &, double, double, int processing);
	void draw_output_port(cairo_t *, const eltstyle &, double, double, int processing);
	void draw(ClickyDiagram *, cairo_t *, PangoLayout *);

	void fill(RouterT *, ProcessingT *, HashMap<String, elt *> &collector, Vector<ElementT *> &, int &z_index);
	
      private:
	elt(const elt &);
	elt &operator=(const elt &);
    };
    
    RouterWindow *_rw;
    GtkWidget *_widget;
    GtkAdjustment *_horiz_adjust;
    GtkAdjustment *_vert_adjust;
    eltstyle _eltstyle;
    int _scale_step;
    double _scale;

    elt *_relt;
    rect_search<ink> _rects;
    HashMap<String, elt *> _elt_map;
    bool _layout;

    elt *_highlight[3];

    double _drag_first_x;
    double _drag_first_y;
    int _drag_state;
    
    PangoAttrList *_name_attrs;
    PangoAttrList *_class_attrs;

    enum { c_c = 0, c_top = 1, c_bot = 2,
	   c_lft = 3 + c_c, c_ulft = c_lft + c_top, c_llft = c_lft + c_bot,
	   c_rt = 6 + c_c, c_urt = c_rt + c_top, c_lrt = c_rt + c_bot };
    GdkCursor *_dir_cursor[9];

    int _last_cursorno;
    
    void initialize();
    void unhighlight(uint8_t htype, rectangle *expose);
    elt *point_elt(double x, double y) const;
    void highlight(elt *e, uint8_t htype, rectangle *expose, bool incremental);
    void set_cursor(elt *e, double x, double y);
    void on_drag_motion(double x, double y);
    
    friend class elt;
    
};


inline void ClickyDiagram::redraw()
{
    gtk_widget_queue_draw(_widget);
}

inline void ClickyDiagram::redraw(rectangle r)
{
    r.expand(elt_expand);
    r.scale(_scale);
    r.integer_align();
    gtk_widget_queue_draw_area(_widget, (gint) (r.x() - _horiz_adjust->value), (gint) (r.y() - _vert_adjust->value), (gint) r.width(), (gint) r.height());
}

inline ClickyDiagram::elt *ClickyDiagram::ink::cast_elt() {
    return (_type == i_elt ? static_cast<elt *>(this) : 0);
}

inline const ClickyDiagram::elt *ClickyDiagram::ink::cast_elt() const {
    return (_type == i_elt ? static_cast<const elt *>(this) : 0);
}

inline ClickyDiagram::conn *ClickyDiagram::ink::cast_conn() {
    return (_type == i_conn ? static_cast<conn *>(this) : 0);
}

inline const ClickyDiagram::conn *ClickyDiagram::ink::cast_conn() const {
    return (_type == i_conn ? static_cast<const conn *>(this) : 0);
}

inline void ClickyDiagram::elt::input_position(int port, const eltstyle &style, double &x_result, double &y_result)
{
    if (port >= _e->ninputs()) {
	x_result = x2();
	y_result = y1();
    } else {
	double offset0, separation;
	port_position(width(), 0, _e->ninputs(), style, offset0, separation);
	x_result = x1() + offset0 + separation * port;
	y_result = y1() + 0.5;
    }
}

inline void ClickyDiagram::elt::output_position(int port, const eltstyle &style, double &x_result, double &y_result)
{
    if (port >= _e->noutputs()) {
	x_result = x2();
	y_result = y2();
    } else {
	double offset0, separation;
	port_position(width(), 1, _e->noutputs(), style, offset0, separation);
	x_result = x1() + offset0 + separation * port;
	y_result = y2() - 0.5;
    }
}

#endif
