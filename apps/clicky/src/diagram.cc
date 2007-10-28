#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <clicktool/processingt.hh>
#include <clicktool/elementmap.hh>
#include <list>
#include <math.h>
#include "diagram.hh"
#include "dwidget.hh"
#include "wrouter.hh"
#include "whandler.hh"
extern "C" {
#include "support.h"
}
namespace clicky {

extern "C" {
static void diagram_map(GtkWidget *, gpointer);
static gboolean on_diagram_event(GtkWidget *, GdkEvent *, gpointer);
static gboolean diagram_expose(GtkWidget *, GdkEventExpose *, gpointer);
static void on_diagram_size_allocate(GtkWidget *, GtkAllocation *, gpointer);
}

wdiagram::wdiagram(wmain *rw)
    : _rw(rw), _scale_step(0), _scale(1), _origin_x(0), _origin_y(0), _relt(0),
      _drag_state(drag_none)
{
    _widget = lookup_widget(_rw->_window, "diagram");
    gtk_widget_realize(_widget);
    gtk_widget_add_events(_widget, GDK_POINTER_MOTION_MASK
			  | GDK_BUTTON_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK
			  | GDK_LEAVE_NOTIFY_MASK);

    GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(_widget->parent);
    _horiz_adjust = gtk_scrolled_window_get_hadjustment(sw);
    _vert_adjust = gtk_scrolled_window_get_vadjustment(sw);
    
    double portscale = 1.6;
    _style.port_offset = 4 * portscale;
    _style.min_port_offset = 3 * portscale;
    _style.port_separation = 3 * portscale;
    _style.min_port_distance = 7 * portscale;
    _style.port_layout_length = 6 * portscale;
    _style.port_layout_width = 4 * portscale;
    _style.port_length[0] = 7 * portscale;
    _style.port_width[0] = 4.5 * portscale;
    _style.port_length[1] = 6 * portscale;
    _style.port_width[1] = 3.8 * portscale;
    _style.agnostic_separation = 1.25 * portscale;
    _style.port_agnostic_separation[0] = sqrt(2.) * _style.agnostic_separation;
    _style.port_agnostic_separation[1] = _style.agnostic_separation;
    _style.inside_dx = 7.5 * portscale;
    _style.inside_dy = 4.5 * portscale;
    _style.inside_contents_dy = 3 * portscale;
    _style.min_preferred_height = 19 * portscale;
    _style.height_increment = 4;
    _style.element_dx = 8 * portscale;
    _style.element_dy = 8 * portscale;
    _style.min_dimen = MAX(_style.port_width[0], _style.port_width[1]) * 2.5;
    _style.min_dimen = MAX(_style.min_dimen, 2 *_style.min_port_offset + MAX(_style.port_length[0], _style.port_length[1]));
    _style.min_queue_width = 11 * portscale;
    _style.min_queue_height = 31 * portscale;
    _style.queue_line_sep = 8 * portscale;

    _style.name_attrs = 0;
    _style.class_attrs = pango_attr_list_new();
    PangoAttribute *a = pango_attr_scale_new(PANGO_SCALE_SMALL);
    a->start_index = 0;
    a->end_index = G_MAXUINT;
    pango_attr_list_insert(_style.class_attrs, a);
    
    g_signal_connect(G_OBJECT(_widget), "event",
		     G_CALLBACK(on_diagram_event), this);
    g_signal_connect(G_OBJECT(_widget), "expose-event",
		     G_CALLBACK(diagram_expose), this);
    g_signal_connect(G_OBJECT(_widget), "map",
		     G_CALLBACK(diagram_map), this);
    g_signal_connect(G_OBJECT(_widget), "size-allocate",
		     G_CALLBACK(on_diagram_size_allocate), this);

    for (int i = 0; i < 3; i++)
	_highlight[i].clear();
    static_assert(ncursors == deg_corner_lrt + 1);
    for (int i = c_main; i < ncursors; i++)
	_cursor[i] = 0;
    _last_cursorno = c_main;
}

wdiagram::~wdiagram()
{
    assert(!_style.name_attrs);
    pango_attr_list_unref(_style.class_attrs);
    delete _relt;
    _relt = 0;
    for (int i = c_main; i < ncursors; i++)
	if (_cursor[i])
	    gdk_cursor_unref(_cursor[i]);
}

void wdiagram::initialize()
{
    if (!_cursor[c_main]) {
	_cursor[c_main] = _rw->_normal_cursor;
	_cursor[deg_corner_ulft] = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
	_cursor[deg_border_top] = gdk_cursor_new(GDK_TOP_SIDE);
	_cursor[deg_corner_urt] = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
	_cursor[deg_border_rt] = gdk_cursor_new(GDK_RIGHT_SIDE);
	_cursor[deg_corner_lrt] = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
	_cursor[deg_border_bot] = gdk_cursor_new(GDK_BOTTOM_SIDE);
	_cursor[deg_corner_llft] = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
	_cursor[deg_border_lft] = gdk_cursor_new(GDK_LEFT_SIDE);
	for (int i = c_main; i < ncursors; i++)
	    gdk_cursor_ref(_cursor[i]);
    }
}


void wdiagram::display(const String &ename, bool scroll_to)
{
    if (delt *e = _elt_map[ename])
	if (!e->highlighted(dhlt_click))
	    highlight(e, dhlt_click, 0, scroll_to);
}

inline void wdiagram::find_rect_elts(const rectangle &r, std::vector<dwidget *> &result) const
{
    _rects.find_all(r, result);
    std::sort(result.begin(), result.end(), dwidget::z_index_less);
    std::vector<dwidget *>::iterator eltsi = std::unique(result.begin(), result.end());
    result.erase(eltsi, result.end());
}

void wdiagram::scroll_recenter(point old_ctr)
{
    if (old_ctr.x() < -1000000)
	old_ctr = scroll_center();
    
    gtk_layout_set_size(GTK_LAYOUT(_widget),
			MAX((gint) (_relt->_width * _scale),
			    _widget->allocation.width),
			MAX((gint) (_relt->_height * _scale),
			    _widget->allocation.height));
    
    GtkAdjustment *ha = _horiz_adjust, *va = _vert_adjust;
    double scaled_width = ha->page_size / _scale;
    if (scaled_width >= _relt->_width) {
	_origin_x = (int) ((_relt->center_x() - scaled_width / 2) * _scale);
	gtk_adjustment_set_value(ha, 0);
    } else {
	_origin_x = (int) (_relt->x() * _scale);
	if (old_ctr.x() - scaled_width / 2 < _relt->x())
	    gtk_adjustment_set_value(ha, 0);
	else if (old_ctr.x() + scaled_width / 2 > _relt->x2())
	    gtk_adjustment_set_value(ha, _relt->width() * _scale - ha->page_size);
	else
	    gtk_adjustment_set_value(ha, (old_ctr.x() - scaled_width / 2) * _scale - _origin_x);
    }
    
    double scaled_height = va->page_size / _scale;
    if (scaled_height >= _relt->_height) {
	_origin_y = (int) ((_relt->center_y() - scaled_height / 2) * _scale);
	gtk_adjustment_set_value(va, 0);
    } else {
	_origin_y = (int) (_relt->y() * _scale);
	if (old_ctr.y() - scaled_height / 2 < _relt->y())
	    gtk_adjustment_set_value(va, 0);
	else if (old_ctr.y() + scaled_height / 2 > _relt->y2())
	    gtk_adjustment_set_value(va, _relt->height() * _scale - va->page_size);
	else
	    gtk_adjustment_set_value(va, (old_ctr.y() - scaled_height / 2) * _scale - _origin_y);
    }

    redraw();
}

void wdiagram::zoom(bool incremental, int amount)
{
    _scale_step = (incremental ? _scale_step + amount : amount);

    if (_layout) {
	point old_ctr = scroll_center();
	_scale = pow(1.2, _scale_step);
	scroll_recenter(old_ctr);
    } else
	_scale = pow(1.2, _scale_step);	
}

void wdiagram::router_create(bool incremental, bool always)
{
    if (!incremental) {
	for (int i = 0; i < 3; ++i)
	    unhighlight(i, 0);
	delete _relt;
	_relt = 0;
	_rects.clear();
	_layout = false;
	_elt_map.clear();
    }
    // don't bother creating if widget not mapped
    if (!always && !GTK_WIDGET_VISIBLE(_widget))
	return;
    if (!_relt) {
	_relt = new delt(0, 0);
	if (_rw->_r) {
	    Vector<ElementT *> path;
	    int z_index = 0;
	    _relt->prepare_router(_rw->_r, _rw->_processing, _elt_map,
				  path, z_index);
	}
	if (!_cursor[0])
	    initialize();
    }
}


/*****
 *
 * Layout
 *
 */

void wdiagram::layout()
{
    //fprintf(stderr, "Layout\n");
    if (!_relt)
	router_create(true, true);
    if (!_layout && _rw->_r) {
	PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);
	ElementMap::push_default(_rw->element_map());
	_relt->layout_main(this, _rw->_r, pl);
	g_object_unref(G_OBJECT(pl));
	scroll_recenter(point(0, 0));
	ElementMap::pop_default();
	_layout = true;
    }
}

void wdiagram::on_expose(const GdkRectangle *area)
{
    if (!_layout)
	layout();

    cairo_t *cr = gdk_cairo_create(GTK_LAYOUT(_widget)->bin_window);
    cairo_rectangle(cr, area->x, area->y, area->width, area->height);
    cairo_clip(cr);

    // background
    cairo_rectangle(cr, area->x, area->y, area->width, area->height);
    const GdkColor &bgcolor = _widget->style->bg[GTK_STATE_NORMAL];
    cairo_set_source_rgb(cr, bgcolor.red / 65535., bgcolor.green / 65535., bgcolor.blue / 65535.);
    cairo_fill(cr);

    // highlight rectangle
    if (_drag_state == drag_rect_dragging) {
	cairo_set_line_width(cr, 2);
	const GdkColor &bgcolor = _widget->style->bg[GTK_STATE_ACTIVE];
	rectangle r = canvas_to_window(_dragr).normalize();
	if (r.width() > 4 && r.height() > 4) {
	    cairo_set_source_rgb(cr, bgcolor.red / 65535., bgcolor.green / 65535., bgcolor.blue / 65535.);
	    cairo_rectangle(cr, r.x() + 2, r.y() + 2, r.width() - 4, r.height() - 4);
	    cairo_fill(cr);
	}
	cairo_set_source_rgb(cr, bgcolor.red / 80000., bgcolor.green / 80000., bgcolor.blue / 80000.);
	if (r.width() <= 4 || r.height() <= 4) {
	    cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
	    cairo_fill(cr);
	} else {
	    cairo_rectangle(cr, r.x() + 1, r.y() + 1, r.width() - 2, r.height() - 2);
	    cairo_stroke(cr);
	}
    }
    
    PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);

    rectangle r(area->x + _origin_x, area->y + _origin_y,
		area->width, area->height);
    r.scale(1 / _scale);
    r.expand(elt_expand);
    std::vector<dwidget *> elts;
    find_rect_elts(r, elts);

    cairo_translate(cr, -_origin_x, -_origin_y);
    cairo_scale(cr, _scale, _scale);
    for (std::vector<dwidget *>::iterator eltsi = elts.begin();
	 eltsi != elts.end(); ++eltsi)
	(*eltsi)->draw(this, cr, pl);
    
    g_object_unref(G_OBJECT(pl));
    cairo_destroy(cr);
}

extern "C" {
static gboolean diagram_expose(GtkWidget *, GdkEventExpose *e, gpointer user_data)
{
    wdiagram *cd = reinterpret_cast<wdiagram *>(user_data);
    cd->on_expose(&e->area);
    return FALSE;
}

static void diagram_map(GtkWidget *, gpointer user_data)
{
    reinterpret_cast<wdiagram *>(user_data)->router_create(true, true);
}

static void on_diagram_size_allocate(GtkWidget *, GtkAllocation *, gpointer user_data)
{
    wdiagram *cd = reinterpret_cast<wdiagram *>(user_data);
    cd->scroll_recenter(point(-1000001, -1000001));
}
}


/*****
 *
 * handlers
 *
 */

extern "C" {
static void on_hpref_diagram_toggled(GtkToggleButton *, gpointer);
}

void wdiagram::hpref_widgets(handler_value *hv, GtkWidget *box)
{
    if (hv->special() || !hv->readable())
	return;

    // for now, only know how to display lengths on Queue elements
    if (hv->handler_name() == "length") {
	handler_value *ohv = main()->hvalues().find(hv->element_name() + ".capacity");
	if (ohv && ohv->readable()) {
	    GtkWidget *w = gtk_check_button_new_with_label(_("Diagram Fullness"));
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), hv->notify_delt());
	    gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	    g_signal_connect(w, "toggled", G_CALLBACK(on_hpref_diagram_toggled), _rw->handlers());
	}
    }
}

void wdiagram::hpref_apply(handler_value *hv)
{
    if (hv->handler_name() == "length") {
	if (!_relt)
	    router_create(true, true);
	if (delt *e = _elt_map[hv->element_name()])
	    if (hv->notify_delt())
		e->add_gadget(this, esflag_fullness);
	    else
		e->remove_gadget(this, esflag_fullness);
    }
}

void wdiagram::notify_read(handler_value *hv)
{
    if (delt *e = _elt_map[hv->element_name()])
	e->notify_read(this, hv);
}

extern "C" {
static void on_hpref_diagram_toggled(GtkToggleButton *button, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(button));
    gboolean on = gtk_toggle_button_get_active(button);
    wh->set_hinfo_flags(hname, hflag_notify_delt, on ? hflag_notify_delt : 0);
}
}


/*****
 *
 * motion exposure
 *
 */

void wdiagram::expose(delt *e, rectangle *expose_rect)
{
    if (_layout && !expose_rect)
	redraw(*e);
    else if (_layout)
	*expose_rect |= *e;
}

void wdiagram::unhighlight(uint8_t htype, rectangle *expose_rect)
{
    assert(htype <= dhlt_pressed);
    while (!_highlight[htype].empty()) {
	delt *e = _highlight[htype].front();
	_highlight[htype].pop_front();
	e->unhighlight(htype);
	expose(e, expose_rect);
    }
}

void wdiagram::highlight(delt *e, uint8_t htype, rectangle *expose_rect, bool scroll_to)
{
    if (!e) {
	unhighlight(htype, expose_rect);
	return;
    }

    std::list<delt *>::iterator iter = _highlight[htype].begin();
    if (!e->highlighted(htype) || (++iter, iter != _highlight[htype].end())) {
	unhighlight(htype, expose_rect);
	_highlight[htype].push_front(e);
	e->highlight(htype);
	expose(e, expose_rect);
    }

    if (scroll_to && _layout) {
	GtkAdjustment *ha = _horiz_adjust, *va = _vert_adjust;
	point e_tl = canvas_to_window(e->x(), e->y());
	point e_br = canvas_to_window(e->x2() + elt_shadow, e->y2() + elt_shadow);
	
	if (e_br.x() >= ha->value + ha->page_size
	    && e_tl.x() >= floor(e_br.x() - ha->page_size))
	    gtk_adjustment_set_value(ha, floor(e_br.x() - ha->page_size));
	else if (e_br.x() >= ha->value + ha->page_size
		 || e_tl.x() < ha->value)
	    gtk_adjustment_set_value(ha, floor(e_tl.x() - 4));
	
	if (e_br.y() >= va->value + va->page_size
	    && e_tl.y() >= floor(e_br.y() - va->page_size))
	    gtk_adjustment_set_value(va, floor(e_br.y() - va->page_size));
	else if (e_br.y() >= va->value + va->page_size
		 || e_tl.y() < va->value)
	    gtk_adjustment_set_value(va, floor(e_tl.y() - 4));
    }
}

delt *wdiagram::point_elt(const point &p) const
{
    std::vector<dwidget *> elts;
    _rects.find_all(p.x(), p.y(), elts);
    std::sort(elts.begin(), elts.end(), dwidget::z_index_greater);
    std::vector<dwidget *>::iterator eltsi = std::unique(elts.begin(), elts.end());
    elts.erase(eltsi, elts.end());
    for (eltsi = elts.begin(); eltsi != elts.end(); ++eltsi)
	if ((*eltsi)->contains(p))
	    if (delt *e = (*eltsi)->cast_elt())
		if (e->visible())
		    return e;
    return 0;
}

void wdiagram::on_drag_motion(const point &p)
{
    point delta = p - _dragr.origin();

    if (_drag_state == drag_start
	&& (fabs(delta.x()) * _scale >= 3
	    || fabs(delta.y()) * _scale >= 3)) {
	for (std::list<delt *>::iterator iter = _highlight[dhlt_click].begin();
	     iter != _highlight[dhlt_click].end(); ++iter)
	    (*iter)->drag_prepare();
	_drag_state = drag_dragging;
    }
    
    if (_drag_state == drag_dragging && _last_cursorno == c_main) {
	for (std::list<delt *>::iterator iter = _highlight[dhlt_click].begin();
	     iter != _highlight[dhlt_click].end(); ++iter)
	    (*iter)->drag_shift(this, delta);
    } else if (_drag_state == drag_dragging) {
	delt *e = _highlight[dhlt_hover].front();
	e->drag_size(this, delta, _last_cursorno);
    }
}

void wdiagram::on_drag_rect_motion(const point &p)
{
    if (_drag_state == drag_rect_start
	&& (fabs(p.x() - _dragr.x()) * _scale >= 3
	    || fabs(p.y() - _dragr.y()) * _scale >= 3))
	_drag_state = drag_rect_dragging;

    if (_drag_state == drag_rect_dragging) {
	rectangle to_redraw = _dragr.normalize();

	for (std::list<delt *>::iterator iter = _highlight[dhlt_click].begin();
	     iter != _highlight[dhlt_click].end(); )
	    if ((*iter)->highlighted(dhlt_rect_click)) {
		(*iter)->unhighlight(dhlt_rect_click);
		(*iter)->unhighlight(dhlt_click);
		to_redraw |= **iter;
		iter = _highlight[dhlt_click].erase(iter);
	    } else
		++iter;
	
	_dragr._width = p.x() - _dragr.x();
	_dragr._height = p.y() - _dragr.y();

	std::vector<dwidget *> elts;
	find_rect_elts(_dragr.normalize(), elts);
	for (std::vector<dwidget *>::iterator iter = elts.begin();
	     iter != elts.end(); ++iter)
	    if (delt *e = (*iter)->cast_elt())
		if (!e->highlighted(dhlt_click)) {
		    e->highlight(dhlt_rect_click);
		    e->highlight(dhlt_click);
		    _highlight[dhlt_click].push_front(e);
		    to_redraw |= *e;
		}

	to_redraw |= _dragr.normalize();
	redraw(to_redraw);
    }
}

void wdiagram::on_drag_complete()
{
    bool changed = false;
    for (std::list<delt *>::iterator iter = _highlight[dhlt_click].begin();
	 iter != _highlight[dhlt_click].end() && !changed; ++iter)
	changed = (*iter)->drag_canvas_changed(*_relt);

    if (changed) {
	point old_ctr = scroll_center();
	_relt->layout_recompute_bounds();
	scroll_recenter(old_ctr);
    }
}

void wdiagram::on_drag_rect_complete()
{
    std::vector<dwidget *> elts;
    find_rect_elts(_dragr, elts);
    for (std::vector<dwidget *>::iterator iter = elts.begin();
	 iter != elts.end(); ++iter)
	if (delt *e = (*iter)->cast_elt())
	    e->unhighlight(dhlt_rect_click);
    redraw(_dragr.normalize());
}

void wdiagram::set_cursor(delt *h, double x, double y)
{
    int gnum = (h ? h->find_gadget(this, x, y) : deg_none);
    int cnum = (gnum >= c_main && gnum < ncursors ? gnum : c_main);
    if (_last_cursorno != cnum) {
	_last_cursorno = cnum;
	gdk_window_set_cursor(_widget->window, _cursor[_last_cursorno]);
    }
}

gboolean wdiagram::on_event(GdkEvent *event)
{
    if (event->type == GDK_MOTION_NOTIFY) {
	point p = window_to_canvas(event->motion.x, event->motion.y);
	if (!(event->motion.state & GDK_BUTTON1_MASK)) {
	    delt *e = point_elt(p);
	    highlight(e, dhlt_hover, 0, false);
	    set_cursor(e, event->motion.x, event->motion.y);
	} else if (_drag_state == drag_start || _drag_state == drag_dragging)
	    on_drag_motion(p);
	else if (_drag_state == drag_rect_start || _drag_state == drag_rect_dragging)
	    on_drag_rect_motion(p);

	// Getting pointer position tells GTK to give us more motion events
	GdkModifierType mod;
	gint mx, my;
	(void) gdk_window_get_pointer(_widget->window, &mx, &my, &mod);

    } else if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
	_dragr.set_origin(window_to_canvas(event->button.x, event->button.y));
	delt *e = point_elt(_dragr.origin());
	_drag_state = (e ? drag_start : drag_rect_start);

	if (!(event->button.state & GDK_SHIFT_MASK)) {
	    if (!e || !e->highlighted(dhlt_click))
		highlight(e, dhlt_click, 0, false);
	    if (e)
		_rw->element_show(e->flat_name(), 0, true);
	} else if (e && e->highlighted(dhlt_click)) {
	    std::list<delt *>::iterator iter = std::find(_highlight[dhlt_click].begin(), _highlight[dhlt_click].end(), e);
	    _highlight[dhlt_click].erase(iter);
	    e->unhighlight(dhlt_click);
	    _drag_state = drag_none;
	} else if (e) {
	    e->highlight(dhlt_click);
	    _highlight[dhlt_click].push_front(e);
	}

	highlight(e, dhlt_pressed, 0, false);
	
    } else if ((event->type == GDK_BUTTON_RELEASE && event->button.button == 1)
	       || (event->type == GDK_FOCUS_CHANGE && !event->focus_change.in)) {
	if (_drag_state == drag_dragging)
	    on_drag_complete();
	else if (_drag_state == drag_rect_dragging)
	    on_drag_rect_complete();
	_drag_state = drag_none;
	unhighlight(dhlt_pressed, 0);
	
    } else if (event->type == GDK_2BUTTON_PRESS && event->button.button == 1) {
	delt *e = point_elt(window_to_canvas(event->button.x, event->button.y));
	highlight(e, dhlt_click, 0, true);
	if (e) {
	    _rw->element_show(e->flat_name(), 1, true);
	    _drag_state = drag_start;
	}
	
    } else if (event->type == GDK_BUTTON_RELEASE && event->button.button == 3) {
	delt *e = point_elt(window_to_canvas(event->button.x, event->button.y));
	if (e) {
	    rectangle bounds = *e;
	    e->remove(_rects, bounds);
	    e->set_vertical(!e->vertical());
	    e->insert(_rects, _style, bounds);
	    redraw(bounds);
	}
    }
    
    return FALSE;
}

extern "C" {
static gboolean on_diagram_event(GtkWidget *, GdkEvent *event, gpointer user_data)
{
    wdiagram *cd = reinterpret_cast<wdiagram *>(user_data);
    return cd->on_event(event);
}
}

}
