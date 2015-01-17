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
#include <cairo-ps.h>
#include <cairo-pdf.h>
#include <cairo-svg.h>
#include "wdiagram.hh"
#include "dwidget.hh"
#include "crouter.hh"
#include "whandler.hh"
#include "scopechain.hh"
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

cdiagram::cdiagram(crouter *cr, PangoLayout *pl, unsigned generation)
    : _relt(new delt)
{
    if (cr->router()) {
	ScopeChain chain(cr->router());
	_relt->create_elements(cr, cr->router(), cr->processing(),
			       &_elt_map, chain);
	_relt->assign_z_indexes(0);
	_relt->create_connections(cr);

	dcontext dcx(cr, pl, 0, generation, 0, 1);
	ElementMap::push_default(cr->element_map());
	_relt->layout_main(dcx);
	ElementMap::pop_default();
    }

    _relt->insert_all(_rects);

    assign(*_relt);
}

cdiagram::~cdiagram()
{
    delete _relt;
}

void cdiagram::layout_recompute_bounds()
{
    _relt->layout_recompute_bounds();
    assign(*_relt);
}

void cdiagram::find_rect_elts(const rectangle &r,
			      std::vector<dwidget *> &result) const
{
    _rects.find_all(r, result);
    std::sort(result.begin(), result.end(), dwidget::z_index_less);
    std::vector<dwidget *>::iterator eltsi = std::unique(result.begin(), result.end());
    result.erase(eltsi, result.end());
}

delt *cdiagram::point_elt(const point &p) const
{
    std::vector<dwidget *> elts;
    _rects.find_all(p.x(), p.y(), elts);
    std::sort(elts.begin(), elts.end(), dwidget::z_index_greater);
    std::vector<dwidget *>::iterator eltsi = std::unique(elts.begin(), elts.end());
    elts.erase(eltsi, elts.end());
    for (eltsi = elts.begin(); eltsi != elts.end(); ++eltsi)
	if ((*eltsi)->contains(p))
	    if (delt *e = (*eltsi)->cast_elt())
		if (dedisp_visible(e->display()))
		    return e;
    return 0;
}

extern "C" {
static cairo_status_t cairo_surface_ignore_write(void *, const unsigned char *, unsigned)
{
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t cairo_surface_stdout_write(void *, const unsigned char *str, unsigned len)
{
    ignore_result(fwrite(str, 1, len, stdout));
    return CAIRO_STATUS_SUCCESS;
}
}

void cdiagram::export_to_file(const char *filename, crouter *cr,
			  point page_size, point margin, double scale,
			  bool multipage)
{
    cr->set_ccss_media("print");
    cairo_surface_t *crs = cairo_pdf_surface_create_for_stream(cairo_surface_ignore_write, 0, 612, 792);
    cairo_t *cairo = cairo_create(crs);
    PangoLayout *pl = pango_cairo_create_layout(cairo);
    unsigned generation = dcontext::step_generation();

    cdiagram *cd = new cdiagram(cr, pl, generation);

    g_object_unref(G_OBJECT(pl));
    cairo_destroy(cairo);
    cairo_surface_destroy(crs);

    cd->export_to_file(filename, false, cr, generation, page_size, margin, scale, multipage);

    delete cd;
}

void cdiagram::export_to_file(const char *filename, bool eps,
			  crouter *cr, unsigned generation,
			  point page_size, point margin, double scale,
			  bool multipage)
{
    char *filename_save = strdup(filename);
    char *filename_ext = strrchr(filename_save, '.');
    if (filename_ext)
        filename_ext++;
    if (eps || !page_size)
	page_size = point(width() / scale + 2 * margin.x(),
			  height() / scale + 2 * margin.y());

    cairo_surface_t *crs;
    if (eps) {
	crs = cairo_ps_surface_create(filename, page_size.x(), page_size.y());
#if CAIRO_VERSION_MINOR >= 6 || (CAIRO_VERSION_MINOR == 5 && CAIRO_VERSION_MICRO >= 2)
	cairo_ps_surface_set_eps(crs, TRUE);
#endif
     } else if (!filename || strcmp(filename, "") == 0 || strcmp(filename, "-") == 0) // PDF stream on stdout
        crs = cairo_pdf_surface_create_for_stream(cairo_surface_stdout_write, 0, page_size.x(), page_size.y());
    else if (filename_ext && strncmp("svg", filename_ext, 4) == 0)
        crs = cairo_svg_surface_create(filename, page_size.x(), page_size.y());
    else // PDF by default
        crs = cairo_pdf_surface_create(filename, page_size.x(), page_size.y());

    cairo_t *cairo = cairo_create(crs);
    dcontext dcx(cr, pango_cairo_create_layout(cairo), cairo,
		 generation, 1 /* position precisely */, 1);
    cr->set_ccss_media("print");

    point visible_area = point(page_size.x() - 2 * margin.x(),
			       page_size.y() - 2 * margin.y());
    for (int jj = 0; scale * jj * visible_area.y() < height(); ++jj)
	for (int ii = 0; scale * ii * visible_area.x() < width(); ++ii) {
	    cairo_save(cairo);
	    cairo_translate(cairo, -x() - ii * visible_area.x() + margin.x(),
			    -y() - jj * visible_area.y() + margin.y());
	    cairo_rectangle(cairo, x() + ii * visible_area.x() - 5,
			    y() + jj * visible_area.y() - 5,
			    visible_area.x() + 10, visible_area.y() + 10);
	    cairo_scale(cairo, 1./scale, 1./scale);
	    cairo_clip(cairo);

	    rectangle rect(-x() + scale * ii * visible_area.x() - scale * 5,
			   -y() + scale * jj * visible_area.y() - scale * 5,
			   scale * visible_area.x() + scale * 10,
			   scale * visible_area.y() + scale * 10);
	    std::vector<dwidget *> elts;
	    find_rect_elts(rect, elts);

	    for (std::vector<dwidget *>::iterator eltsi = elts.begin();
		 eltsi != elts.end(); ++eltsi)
		(*eltsi)->draw(dcx);

	    cairo_restore(cairo);
	    if (multipage)
		cairo_show_page(cairo);
	}

    cr->set_ccss_media("screen");
    g_object_unref(G_OBJECT(dcx.pl));
    cairo_destroy(dcx.cairo);
    cairo_surface_destroy(crs);
    free(filename_save);
}


wdiagram::wdiagram(wmain *rw)
    : _rw(rw), _cdiagram(0), _generation(dcontext::step_generation()),
      _scale_step(0), _scale(1), _penumbra(2), _origin_x(0), _origin_y(0),
      _drag_state(drag_none)
{
    _widget = lookup_widget(_rw->_window, "diagram");
    gtk_widget_realize(_widget);
    gtk_widget_add_events(_widget, GDK_POINTER_MOTION_MASK
			  | GDK_BUTTON_PRESS_MASK
			  | GDK_BUTTON_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK
			  | GDK_LEAVE_NOTIFY_MASK);

    GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(_widget->parent);
    _horiz_adjust = gtk_scrolled_window_get_hadjustment(sw);
    _vert_adjust = gtk_scrolled_window_get_vadjustment(sw);

#if 0
    PangoFontMap *fm = pango_cairo_font_map_get_default();
    PangoFontFamily **fms;
    int nfms;
    pango_font_map_list_families(fm, &fms, &nfms);
    for (int i = 0; i < nfms; i++)
	fprintf(stderr, "  %s\n", pango_font_family_get_name(fms[i]));
    g_free(fms);
#endif

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
    static_assert((int) ncursors > (int) deg_corner_lrt
		  && (int) c_element == (int) deg_element,
		  "Corner constants screwup.");
    for (int i = 0; i < ncursors; i++)
	_cursor[i] = 0;
    _last_cursorno = c_main;
}

wdiagram::~wdiagram()
{
    delete _cdiagram;
    for (int i = c_main; i < ncursors; i++)
	if (_cursor[i])
	    gdk_cursor_unref(_cursor[i]);
}

void wdiagram::initialize()
{
    if (!_cursor[c_main]) {
	_cursor[c_main] = _rw->_normal_cursor;
	_cursor[deg_element] = gdk_cursor_new(GDK_HAND1);
	_cursor[deg_corner_ulft] = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
	_cursor[deg_border_top] = gdk_cursor_new(GDK_TOP_SIDE);
	_cursor[deg_corner_urt] = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
	_cursor[deg_border_rt] = gdk_cursor_new(GDK_RIGHT_SIDE);
	_cursor[deg_corner_lrt] = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
	_cursor[deg_border_bot] = gdk_cursor_new(GDK_BOTTOM_SIDE);
	_cursor[deg_corner_llft] = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
	_cursor[deg_border_lft] = gdk_cursor_new(GDK_LEFT_SIDE);
	_cursor[c_hand] = gdk_cursor_new(GDK_FLEUR);
	for (int i = c_main; i < ncursors; i++)
	    gdk_cursor_ref(_cursor[i]);
    }
}

void wdiagram::on_ccss_changed()
{
    _generation = dcontext::step_generation();
}

void wdiagram::element_show(const String &ename, bool scroll_to)
{
    if (_cdiagram)
	if (delt *e = _cdiagram->elt(ename)) {
	    while (!e->root() && !e->visible())
		e = e->parent();
	    if (!e->root() && (!e->highlighted(dhlt_click) || scroll_to))
		highlight(e, dhlt_click, scroll_to, true);
	}
}

void wdiagram::scroll_recenter(point old_ctr)
{
    if (!_cdiagram)
	return;
    if (old_ctr.x() < -1000000)
	old_ctr = scroll_center();

    gtk_layout_set_size(GTK_LAYOUT(_widget),
			MAX((gint) (_cdiagram->width() * scale() + 0.5),
			    _widget->allocation.width),
			MAX((gint) (_cdiagram->height() * scale() + 0.5),
			    _widget->allocation.height));

    GtkAdjustment *ha = _horiz_adjust;
    double scaled_width = ha->page_size / scale();
    if (scaled_width >= _cdiagram->width()) {
	_origin_x = (int) ((_cdiagram->center_x() - scaled_width / 2) * scale() + 0.5);
	gtk_adjustment_set_value(ha, 0);
    } else {
	_origin_x = (int) (_cdiagram->x() * scale() + 0.5);
	if (old_ctr.x() - scaled_width / 2 < _cdiagram->x())
	    gtk_adjustment_set_value(ha, 0);
	else if (old_ctr.x() + scaled_width / 2 > _cdiagram->x2())
	    gtk_adjustment_set_value(ha, _cdiagram->width() * scale() - ha->page_size);
	else
	    gtk_adjustment_set_value(ha, (old_ctr.x() - scaled_width / 2) * scale() - _origin_x);
    }

    GtkAdjustment *va = _vert_adjust;
    double scaled_height = va->page_size / scale();
    if (scaled_height >= _cdiagram->height()) {
	_origin_y = (int) ((_cdiagram->center_y() - scaled_height / 2) * scale() + 0.5);
	gtk_adjustment_set_value(va, 0);
    } else {
	_origin_y = (int) (_cdiagram->y() * scale() + 0.5);
	if (old_ctr.y() - scaled_height / 2 < _cdiagram->y())
	    gtk_adjustment_set_value(va, 0);
	else if (old_ctr.y() + scaled_height / 2 > _cdiagram->y2())
	    gtk_adjustment_set_value(va, _cdiagram->height() * scale() - va->page_size);
	else
	    gtk_adjustment_set_value(va, (old_ctr.y() - scaled_height / 2) * scale() - _origin_y);
    }

    redraw();
}

void wdiagram::zoom(bool incremental, int amount)
{
    if (!incremental && amount <= -10000 && _cdiagram) { // best fit
	// window_width / 3**(scale_step/2) <= contents_width
	// && window_height / 3**(scale_step/2) <= contents_height
	double ssw = 3 * log2(_horiz_adjust->page_size / _cdiagram->width());
	double ssh = 3 * log2(_vert_adjust->page_size / _cdiagram->height());
	_scale_step = (int) floor(std::min(ssw, ssh));
    } else
	_scale_step = (incremental ? _scale_step + amount : amount);
    double new_scale = pow(2.0, _scale_step / 3.0);

    if (_cdiagram) {
	point old_ctr = scroll_center();
	_scale = new_scale;
	_penumbra = 2;
	scroll_recenter(old_ctr);
    } else
	_scale = new_scale;
}

void wdiagram::router_create(bool incremental, bool always)
{
    if (!incremental) {
	for (int i = 0; i < 3; ++i)
	    highlight(0, i);
	delete _cdiagram;
	_cdiagram = 0;
    }
    // don't bother creating if widget not mapped
    if (!always && !GTK_WIDGET_VISIBLE(_widget))
	return;
    if (!_cursor[0])
	initialize();
    if (!_cdiagram) {
	PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);
	_cdiagram = new cdiagram(_rw, pl, _generation);
	g_object_unref(G_OBJECT(pl));
	scroll_recenter(point(0, 0));
    }
    if (handler_value *hv = _rw->hvalues().find_placeholder("active_ports", hflag_r | hflag_notify_delt))
	hv->refresh(_rw, true);
    if (!incremental)
	redraw();
}


/*****
 *
 * Layout
 *
 */

void wdiagram::on_expose(const GdkRectangle *area)
{
    if (!_cdiagram)
	router_create(true, true);

    cairo_t *cr = gdk_cairo_create(GTK_LAYOUT(_widget)->bin_window);
    cairo_rectangle(cr, area->x, area->y, area->width, area->height);
    cairo_clip(cr);

    // background
    cairo_rectangle(cr, area->x, area->y, area->width, area->height);
    //const GdkColor &bgcolor = _widget->style->bg[GTK_STATE_NORMAL];
    //cairo_set_source_rgb(cr, bgcolor.red / 65535., bgcolor.green / 65535., bgcolor.blue / 65535.);
    cairo_set_source_rgb(cr, 1, 1, 1);
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

    cairo_translate(cr, -_origin_x, -_origin_y);
    cairo_scale(cr, scale(), scale());

    dcontext dcx(main(), gtk_widget_create_pango_layout(_widget, NULL), cr,
		 _generation, _scale_step, _scale);

    rectangle r(area->x + _origin_x, area->y + _origin_y,
		area->width, area->height);
    r.scale(1 / scale());
    r.expand(_penumbra);

    std::vector<dwidget *> elts;
    _cdiagram->find_rect_elts(r, elts);
    for (std::vector<dwidget *>::iterator eltsi = elts.begin();
	 eltsi != elts.end(); ++eltsi)
	(*eltsi)->draw(dcx);

    g_object_unref(G_OBJECT(dcx.pl));
    _penumbra = std::max(_penumbra, dcx.penumbra);
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
 * export
 *
 */

void wdiagram::export_diagram(const char *filename, bool eps)
{
    if (!_cdiagram)
	router_create(true, true);
    _cdiagram->export_to_file(filename, eps, main(), dcontext::step_generation(),
			  point(0, 0), point(0, 0), 1, false);
}


/*****
 *
 * handlers
 *
 */

void wdiagram::notify_active_ports(String value)
{
    std::vector<std::pair<dconn *, int> > actives;
    int lineno = 0;

    while (value) {
	const char *nl = find(value, '\n');
	if (nl < value.end())
	    ++nl;
	String line = value.substring(value.begin(), nl);
	value = value.substring(nl, value.end());

	String enamestr = cp_shift_spacevec(line);
	String portstr = cp_shift_spacevec(line);
	ScopeChain chain(_rw->router());

	ElementT *element;
	int port;
	if (!enamestr || !portstr
	    || (portstr[0] != 'i' && portstr[0] != 'o')
	    || !(element = chain.push_element(enamestr))
	    || !cp_integer(portstr.substring(1), &port)
	    || port < 0 || port >= element->nports(portstr[0] == 'o')) {
	    // odd line, explode
	    actives.clear();
	    break;
	}
	bool isoutput = (portstr[0] == 'o');

	while (1) {
	    String ename = chain.flat_name(element->name());
	    if (delt *e = _cdiagram->elt(ename))
		if (dconn *c = e->find_connection(isoutput, port))
		    actives.push_back(std::make_pair(c, lineno));

	    RouterT::conn_iterator cit = chain.back_router()->find_connections_touching(PortT(element, port), isoutput);
	    assert(cit.is_back());

	    PortT oport = cit->end(!isoutput);
	    ElementClassT *oclass = chain.resolved_type(oport.element);
	    if (RouterT *subr = oclass->cast_router()) {
		chain.enter_element(oport.element);
		element = subr->element(!isoutput);
		port = oport.port;
	    } else if (oport.element == chain.back_router()->element(isoutput)
		       && oport.element->router() != _rw->router()) {
		String back_component = chain.back_component();
		chain.pop_element();
		element = chain.back_router()->element(back_component);
		port = oport.port;
	    } else
		break;
	}

	++lineno;
    }

    // compile 'actives' into actual arrays
    std::sort(actives.begin(), actives.end());
    _active_conns.clear();
    _active_offsets.clear();
    _active_ports.clear();
    for (std::vector<std::pair<dconn *, int> >::iterator it = actives.begin();
	 it != actives.end(); ) {
	std::vector<std::pair<dconn *, int> >::iterator fit = it;
	_active_conns.push_back(fit->first);
	_active_offsets.push_back(_active_ports.size());
	for (; it != actives.end() && it->first == fit->first; ++it)
	    _active_ports.push_back(it->second);
    }
    if (actives.size())
	_active_offsets.push_back(_active_ports.size());
    _active_nports = (actives.size() ? lineno : -1);

    // inquire into port statistics
    handler_value *pstats = _rw->hvalues().find_placeholder("active_port_stats", hflag_r | hflag_notify_delt, 3000);
    if (!pstats)
	/* we may be in the middle of a reconfigure */;
    else if (_active_offsets.size()) {
	pstats->set_flags(_rw, pstats->flags() | hflag_autorefresh);
	pstats->refresh(_rw);
    } else
	pstats->set_flags(_rw, pstats->flags() & ~hflag_autorefresh);
}

void wdiagram::notify_active_port_stats(String value)
{
    int lineno = 0;
    std::vector<unsigned> counts;
    while (value) {
	const char *nl = find(value, '\n');
	if (nl < value.end())
	    ++nl;
	String line = value.substring(value.begin(), nl);
	value = value.substring(nl, value.end());

	unsigned value;
	if (cp_integer(cp_shift_spacevec(line), &value)
	    && lineno < _active_nports)
	    counts.push_back(value);

	++lineno;
    }

    if (lineno == _active_nports) {
	for (size_t i = 0; i < _active_conns.size(); ++i) {
	    unsigned x = 0;
	    for (int o = _active_offsets[i]; o < _active_offsets[i+1]; ++o)
		x += counts[_active_ports[o]];
	    if (_active_conns[i]->change_count(x))
		redraw(*_active_conns[i]);
	}
    } else
	_rw->hvalues().find_force("active_ports")->refresh(_rw);
}

void wdiagram::notify_read(handler_value *hv)
{
    if (hv->element_name()) {
	if (delt *e = elt(hv->element_name()))
	    e->notify_read(this, hv);
    } else if (hv->handler_name() == "active_ports")
	notify_active_ports(hv->hvalue());
    else if (hv->handler_name() == "active_port_stats")
	notify_active_port_stats(hv->hvalue());
}


/*****
 *
 * motion exposure
 *
 */

void wdiagram::expose(const delt *e, rectangle *expose_rect)
{
    if (!expose_rect)
	redraw(*e);
    else
	*expose_rect |= *e;
}

void wdiagram::highlight(delt *e, uint8_t htype,
			 bool scroll_to, bool all_split)
{
    assert(htype <= dhlt_pressed);

    if (!e) {
	while (!_highlight[htype].empty()) {
	    delt *e = _highlight[htype].front();
	    _highlight[htype].pop_front();
	    e->unhighlight(htype);
	    expose(e, 0);
	}
	return;
    }

    std::list<delt *>::iterator iter = _highlight[htype].begin();
    if (!e->highlighted(htype) || (++iter, iter != _highlight[htype].end())) {
	highlight(0, htype, false);
	delt *ee = e;
	do {
	    _highlight[htype].push_front(ee);
	    ee->highlight(htype);
	    expose(ee, 0);
	} while (all_split ? (ee = ee->next_split(e)) : 0);
    }

    if (scroll_to && _cdiagram) {
	GtkAdjustment *ha = _horiz_adjust, *va = _vert_adjust;
	rectangle ex = *e;
	ex.expand(e->shadow(scale(), 0), e->shadow(scale(), 1),
		  e->shadow(scale(), 2), e->shadow(scale(), 3));
	ex = canvas_to_window(ex);
	for (delt *o = e->next_split(e); all_split && o; o = o->next_split(e)) {
	    rectangle ox = *o;
	    ox.expand(o->shadow(scale(), 0), o->shadow(scale(), 1),
		      o->shadow(scale(), 2), o->shadow(scale(), 3));
	    ox = canvas_to_window(ox);
	    rectangle windowrect(ha->value, va->value, ha->page_size, va->page_size);
	    if (ox.intersect(windowrect).area() > ex.intersect(windowrect).area())
		ex = ox;
	}

	if (ex.x2() >= ha->value + ha->page_size
	    && ex.x() >= floor(ex.x2() - ha->page_size))
	    gtk_adjustment_set_value(ha, floor(ex.x2() - ha->page_size));
	else if (ex.x2() >= ha->value + ha->page_size
		 || ex.x() < ha->value)
	    gtk_adjustment_set_value(ha, floor(ex.x() - 4));

	if (ex.y2() >= va->value + va->page_size
	    && ex.y() >= floor(ex.y2() - va->page_size))
	    gtk_adjustment_set_value(va, floor(ex.y2() - va->page_size));
	else if (ex.y2() >= va->value + va->page_size
		 || ex.y() < va->value)
	    gtk_adjustment_set_value(va, floor(ex.y() - 4));
    }
}

void wdiagram::on_drag_motion(const point &p)
{
    point delta = p - _dragr.origin();

    if (_drag_state == drag_start
	&& (fabs(delta.x()) * scale() >= 3
	    || fabs(delta.y()) * scale() >= 3)) {
	for (std::list<delt *>::iterator iter = _highlight[dhlt_click].begin();
	     iter != _highlight[dhlt_click].end(); ++iter)
	    (*iter)->drag_prepare();
	_drag_state = drag_dragging;
    }

    if (_drag_state == drag_dragging && _last_cursorno == deg_element) {
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
	&& (fabs(p.x() - _dragr.x()) * scale() >= 3
	    || fabs(p.y() - _dragr.y()) * scale() >= 3))
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
	_cdiagram->find_rect_elts(_dragr.normalize(), elts);
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
	changed = (*iter)->drag_canvas_changed(*_cdiagram);

    if (changed) {
	point old_ctr = scroll_center();
	_cdiagram->layout_recompute_bounds();
	scroll_recenter(old_ctr);
    }
}

void wdiagram::on_drag_rect_complete()
{
    std::vector<dwidget *> elts;
    _cdiagram->find_rect_elts(_dragr, elts);
    for (std::vector<dwidget *>::iterator iter = elts.begin();
	 iter != elts.end(); ++iter)
	if (delt *e = (*iter)->cast_elt())
	    e->unhighlight(dhlt_rect_click);
    redraw(_dragr.normalize());
}

void wdiagram::on_drag_hand_motion(double x_root, double y_root)
{
    if (_drag_state == drag_hand_start
	&& (fabs(x_root - _dragr.x()) >= 3
	    || fabs(y_root - _dragr.y()) >= 3)) {
	_dragr.set_size(gtk_adjustment_get_value(_horiz_adjust),
			gtk_adjustment_get_value(_vert_adjust));
	_drag_state = drag_hand_dragging;
    }

    if (_drag_state == drag_hand_dragging) {
	double dx = _dragr.x() - x_root;
	double dy = _dragr.y() - y_root;
	gtk_adjustment_set_value(_horiz_adjust, std::min(_dragr.width() + dx, _horiz_adjust->upper - _horiz_adjust->page_size));
	gtk_adjustment_set_value(_vert_adjust, std::min(_dragr.height() + dy, _vert_adjust->upper - _vert_adjust->page_size));
    }
}

void wdiagram::set_cursor(delt *h, double x, double y, int state)
{
    int cnum;
    if (h) {
	int gnum = h->find_gadget(this, x, y);
	cnum = (gnum >= deg_element && gnum < ncursors ? gnum : c_main);
    } else if ((state & GDK_SHIFT_MASK)
	       || (_drag_state != drag_hand_start && _drag_state != drag_hand_dragging))
	cnum = c_main;
    else
	cnum = c_hand;
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
	    delt *e = _cdiagram->point_elt(p);
	    highlight(e, dhlt_hover, false, false);
	    set_cursor(e, event->motion.x, event->motion.y, event->motion.state);
	} else if (_drag_state == drag_start || _drag_state == drag_dragging)
	    on_drag_motion(p);
	else if (_drag_state == drag_rect_start || _drag_state == drag_rect_dragging)
	    on_drag_rect_motion(p);
	else if (_drag_state == drag_hand_start || _drag_state == drag_hand_dragging)
	    on_drag_hand_motion(event->motion.x_root, event->motion.y_root);

	// Getting pointer position tells GTK to give us more motion events
	GdkModifierType mod;
	gint mx, my;
	(void) gdk_window_get_pointer(_widget->window, &mx, &my, &mod);

    } else if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
	point p = window_to_canvas(event->button.x, event->button.y);
	delt *e = _cdiagram->point_elt(p);
	if (e) {
	    _drag_state = drag_start;
	    _dragr.set_origin(p);
	} else if (event->button.state & GDK_SHIFT_MASK) {
	    _drag_state = drag_rect_start;
	    _dragr.set_origin(p);
	} else {
	    _drag_state = drag_hand_start;
	    _dragr.set_origin(event->button.x_root, event->button.y_root);
	}

	if (!(event->button.state & GDK_SHIFT_MASK)) {
	    if (e && !e->highlighted(dhlt_click))
		highlight(e, dhlt_click, false, true);
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

	highlight(e, dhlt_pressed, false, false);
	set_cursor(e, event->button.x, event->button.y, event->button.state);

    } else if ((event->type == GDK_BUTTON_RELEASE && event->button.button == 1)
	       || (event->type == GDK_FOCUS_CHANGE && !event->focus_change.in)) {
	if (_drag_state == drag_dragging)
	    on_drag_complete();
	else if (_drag_state == drag_rect_dragging)
	    on_drag_rect_complete();
	_drag_state = drag_none;
	highlight(0, dhlt_pressed, false, false);

    } else if (event->type == GDK_2BUTTON_PRESS && event->button.button == 1) {
	delt *e = _cdiagram->point_elt(window_to_canvas(event->button.x, event->button.y));
	highlight(e, dhlt_click, true, true);
	if (e) {
	    _rw->element_show(e->flat_name(), 1, true);
	    _drag_state = drag_start;
	}

    } else if (event->type == GDK_BUTTON_RELEASE && event->button.button == 3) {
	/*delt *e = _cdiagram->point_elt(window_to_canvas(event->button.x, event->button.y));
	if (e) {
	    rectangle bounds = *e;
	    e->remove(_rects, bounds);
	    e->set_orientation(3 - e->orientation());
	    e->insert(_rects, this, bounds);
	    redraw(bounds);
	    }*/
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
