#ifndef CLICKY_WDIAGRAM_HH
#define CLICKY_WDIAGRAM_HH 1
#include <gtk/gtk.h>
#include <vector>
#include "rectangle.hh"
#include "rectsearch.hh"
#include <click/bitvector.hh>
#include <clicktool/elementt.hh>
class Bitvector;
namespace clicky {
class wmain;
class handler_value;
class dwidget;
class delt;
class dconn;
class crouter;

class cdiagram : public rectangle { public:

    cdiagram(crouter *cr, PangoLayout *pl, unsigned generation);
    ~cdiagram();

    delt *elt(const String &ename) const {
	return _elt_map[ename];
    }
    rect_search<dwidget> &rects() {
	return _rects;
    }

    void layout_recompute_bounds();
    delt *point_elt(const point &p) const;
    void find_rect_elts(const rectangle &r, std::vector<dwidget *> &result) const;

    static void export_to_file(const char *filename, crouter *cr,
			   point page_size, point margin, double scale,
			   bool multipage);

    void export_to_file(const char *filename, bool eps,
		    crouter *cr, unsigned generation,
		    point page_size, point margin, double scale,
		    bool multipage);

  private:

    delt *_relt;
    rect_search<dwidget> _rects;
    HashTable<String, delt *> _elt_map;

};

class wdiagram { public:

    wdiagram(wmain *rw);
    ~wdiagram();

    wmain *main() const {
	return _rw;
    }

    delt *elt(const String &ename) const {
	return (_cdiagram ? _cdiagram->elt(ename) : 0);
    }
    rect_search<dwidget> &rects() {
	return _cdiagram->rects();
    }

    int scale_step() const {
	return _scale_step;
    }
    double scale() const {
	return _scale;
    }

    void router_create(bool incremental, bool always);

    bool visible() const {
	return gtk_widget_get_visible(_widget);
    }

    inline void redraw();
    inline void redraw(rectangle r);

    void element_show(const String &ename, bool scroll_to);
    void zoom(bool incremental, int amount);
    void scroll_recenter(point p);

    void on_expose(const GdkRectangle *r);
    gboolean on_event(GdkEvent *event);
    void on_ccss_changed();

    // handlers
    void notify_read(handler_value *hv);

    point window_to_canvas(double x, double y) const;
    point canvas_to_window(double x, double y) const;
    rectangle canvas_to_window(const rectangle &r) const;

    void export_diagram(const char *filename, bool eps);

    enum { c_element = 0, c_main = 9, c_hand = 10, ncursors = 11 };

  private:

    wmain *_rw;
    GtkWidget *_widget;
    GtkAdjustment *_horiz_adjust;
    GtkAdjustment *_vert_adjust;

    cdiagram *_cdiagram;
    unsigned _generation;
    int _scale_step;
    double _scale;
    double _penumbra;

    int _origin_x;
    int _origin_y;

    std::list<delt *> _highlight[3];

    std::vector<dconn *> _active_conns;
    std::vector<int> _active_offsets;
    std::vector<int> _active_ports;
    int _active_nports;

    enum {
	drag_none,
	drag_start,
	drag_dragging,
	drag_rect_start,
	drag_rect_dragging,
	drag_hand_start,
	drag_hand_dragging
    };

    rectangle _dragr;
    int _drag_state;

    GdkCursor *_cursor[ncursors];

    int _last_cursorno;

    void initialize();
    void layout();

    void expose(const delt *e, rectangle *expose_rect);
    void highlight(delt *e, uint8_t htype,
		   bool scroll_to = false, bool all_splits = false);

    void set_cursor(delt *e, double x, double y, int state);
    point scroll_center() const;
    void on_drag_motion(const point &p);
    void on_drag_rect_motion(const point &p);
    void on_drag_hand_motion(double x_root, double y_root);
    void on_drag_complete();
    void on_drag_rect_complete();

    void notify_active_ports(String value);
    void notify_active_port_stats(String value);

    friend class delt;

};


inline void wdiagram::redraw()
{
    gtk_widget_queue_draw(_widget);
}

inline void wdiagram::redraw(rectangle r)
{
    r.expand(_penumbra);
    r.scale(_scale);
    r.shift(-_horiz_adjust->value - _origin_x, -_vert_adjust->value - _origin_y);
    r.integer_align();
    gtk_widget_queue_draw_area(_widget, (gint) r.x(), (gint) r.y(), (gint) r.width(), (gint) r.height());
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
