#ifndef CLICKY_DIAGRAM_HH
#define CLICKY_DIAGRAM_HH 1
#include <vector>
#include "rectangle.hh"
#include "rectsearch.hh"
#include "dstyle.hh"
#include <clicktool/elementt.hh>
class Bitvector;
namespace clicky {
class wmain;
class handler_value;
class dwidget;
class delt;

class wdiagram { public:

    wdiagram(wmain *rw);
    ~wdiagram();

    wmain *main() const {
	return _rw;
    }
    
    void router_create(bool incremental, bool always);

    int scale_step() const {
	return _scale_step;
    }
    double scale() const {
	return _scale;
    }

    const dstyle &style() const {
	return _style;
    }

    rect_search<dwidget> &rects() {
	return _rects;
    }
    
    void layout();
    
    inline void redraw();
    inline void redraw(rectangle r);

    void display(const String &ename, bool scroll_to);
    void zoom(bool incremental, int amount);
    void scroll_recenter(point p);

    void on_expose(const GdkRectangle *r);
    gboolean on_event(GdkEvent *event);

    // handlers
    void hpref_widgets(handler_value *hv, GtkWidget *box);
    void hpref_apply(handler_value *hv);
    void notify_read(handler_value *hv);

    point window_to_canvas(double x, double y) const;
    point canvas_to_window(double x, double y) const;
    rectangle canvas_to_window(const rectangle &r) const;

    enum { c_main = 0, ncursors = 9 };
    
  private:

    enum { elt_expand = 4,	// max dimension of drawing outside width-height
	   elt_shadow = 3 };	// shadow width on bottom and right

    wmain *_rw;
    GtkWidget *_widget;
    GtkAdjustment *_horiz_adjust;
    GtkAdjustment *_vert_adjust;
    dstyle _style;
    
    int _scale_step;
    double _scale;
    int _origin_x;
    int _origin_y;

    delt *_relt;
    rect_search<dwidget> _rects;
    HashMap<String, delt *> _elt_map;
    bool _layout;

    std::list<delt *> _highlight[3];

    enum { drag_none, drag_start, drag_dragging, drag_rect_start,
	   drag_rect_dragging };
    
    rectangle _dragr;
    int _drag_state;

    GdkCursor *_cursor[9];

    int _last_cursorno;
    
    void initialize();

    void expose(delt *e, rectangle *expose_rect);
    void unhighlight(uint8_t htype, rectangle *expose_rect);
    void highlight(delt *e, uint8_t htype, rectangle *expose_rect, bool incremental);

    delt *point_elt(const point &p) const;
    void set_cursor(delt *e, double x, double y);
    point scroll_center() const;
    inline void find_rect_elts(const rectangle &r, std::vector<dwidget *> &result) const;
    void on_drag_motion(const point &p);
    void on_drag_rect_motion(const point &p);
    void on_drag_complete();
    void on_drag_rect_complete();
    
    friend class delt;
    
};


inline void wdiagram::redraw()
{
    gtk_widget_queue_draw(_widget);
}

inline void wdiagram::redraw(rectangle r)
{
    r.expand(elt_expand);
    r.scale(_scale);
    r.integer_align();
    gtk_widget_queue_draw_area(_widget, (gint) (r.x() - _horiz_adjust->value - _origin_x), (gint) (r.y() - _vert_adjust->value - _origin_y), (gint) r.width(), (gint) r.height());
}

inline point wdiagram::window_to_canvas(double x, double y) const
{
    return point((x + _origin_x) / _scale, (y + _origin_y) / _scale);
}

inline point wdiagram::canvas_to_window(double x, double y) const
{
    return point(x * _scale - _origin_x, y * _scale - _origin_y);
}

inline rectangle wdiagram::canvas_to_window(const rectangle &r) const
{
    return rectangle(r.x() * _scale - _origin_x, r.y() * _scale - _origin_y,
		     r.width() * _scale, r.height() * _scale);
}

inline point wdiagram::scroll_center() const
{
    return window_to_canvas(_horiz_adjust->value + _horiz_adjust->page_size / 2,
			    _vert_adjust->value + _vert_adjust->page_size / 2);
}

}
#endif
