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
#include "wrouter.hh"
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
    gtk_widget_add_events(_widget, GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK);

    GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(_widget->parent);
    _horiz_adjust = gtk_scrolled_window_get_hadjustment(sw);
    _vert_adjust = gtk_scrolled_window_get_vadjustment(sw);
    
    _name_attrs = 0;
    _class_attrs = pango_attr_list_new();
    PangoAttribute *a = pango_attr_scale_new(PANGO_SCALE_SMALL);
    a->start_index = 0;
    a->end_index = G_MAXUINT;
    pango_attr_list_insert(_class_attrs, a);

    double portscale = 1.6;
    _style.port_offset = 4 * portscale;
    _style.min_port_offset = 3 * portscale;
    _style.port_separation = 3 * portscale;
    _style.min_port_distance = 7 * portscale;
    _style.port_layout_length = 6 * portscale;
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
    
    g_signal_connect(G_OBJECT(_widget), "event",
		     G_CALLBACK(on_diagram_event), this);
    g_signal_connect(G_OBJECT(_widget), "expose-event",
		     G_CALLBACK(diagram_expose), this);
    g_signal_connect(G_OBJECT(_widget), "map",
		     G_CALLBACK(diagram_map), this);
    g_signal_connect(G_OBJECT(_widget), "size-allocate",
		     G_CALLBACK(on_diagram_size_allocate), this);

    _highlight[0] = _highlight[1] = _highlight[2] = 0;
    for (int i = 0; i < 9; i++)
	_dir_cursor[i] = 0;
    _last_cursorno = c_c;
}

wdiagram::~wdiagram()
{
    pango_attr_list_unref(_class_attrs);
    delete _relt;
    _relt = 0;
    for (int i = 0; i < 9; i++)
	if (_dir_cursor[i])
	    gdk_cursor_unref(_dir_cursor[i]);
}

void wdiagram::initialize()
{
    if (!_dir_cursor[c_ulft]) {
	_dir_cursor[c_c] = _rw->_normal_cursor;
	_dir_cursor[c_ulft] = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
	_dir_cursor[c_top] = gdk_cursor_new(GDK_TOP_SIDE);
	_dir_cursor[c_urt] = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
	_dir_cursor[c_rt] = gdk_cursor_new(GDK_RIGHT_SIDE);
	_dir_cursor[c_lrt] = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
	_dir_cursor[c_bot] = gdk_cursor_new(GDK_BOTTOM_SIDE);
	_dir_cursor[c_llft] = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
	_dir_cursor[c_lft] = gdk_cursor_new(GDK_LEFT_SIDE);
	for (int i = 0; i < 9; i++)
	    gdk_cursor_ref(_dir_cursor[i]);
    }
}


void wdiagram::display(const String &ename, bool scroll_to)
{
    if (elt *e = _elt_map[ename])
	if (!(e->_highlight & (1 << htype_click)))
	    highlight(e, htype_click, 0, scroll_to);
}

inline void wdiagram::find_rect_elts(const rectangle &r, std::vector<ink *> &result) const
{
    _rects.find_all(r, result);
    std::sort(result.begin(), result.end(), ink::z_index_less);
    std::vector<ink *>::iterator eltsi = std::unique(result.begin(), result.end());
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

wdiagram::elt::~elt()
{
    while (_elt.size()) {
	delete _elt.back();
	_elt.pop_back();
    }
    while (_conn.size()) {
	delete _conn.back();
	_conn.pop_back();
    }
}

void wdiagram::elt::fill(RouterT *r, ProcessingT *processing, HashMap<String, elt *> &collector, Vector<ElementT *> &path, int &z_index)
{
    for (RouterT::iterator i = r->begin_elements(); i != r->end_elements(); ++i) {
	elt *e = new elt(this, z_index);
	z_index++;
	_elt.push_back(e);
	e->_e = i.operator->();
	e->_processing_code = processing->decorated_processing_code(e->_e);
	
	if (i->tunnel()) {
	    e->_visible = false;
	    e->_layout = true;
	    e->_width = e->_height = 0;
	    continue;
	}

	const String &name = e->_e->name(), &type_name = e->_e->type_name();
	if (name.length() > type_name.length() + 1
	    && name[type_name.length()] == '@'
	    && name.substring(0, type_name.length()) == type_name)
	    e->_show_class = false;
	
	path.push_back(e->_e);
	RouterT::flatten_path(path, e->_flat_name, e->_flat_config);
	collector.insert(e->_flat_name, e);
	if (RouterT *r = e->_e->resolved_router()) {
	    ProcessingT subprocessing(r, processing->element_map());
	    subprocessing.create(e->_processing_code, true);
	    e->fill(r, &subprocessing, collector, path, z_index);
	}
	path.pop_back();

	if (e->_e->type_name().length() >= 5
	    && memcmp(e->_e->type_name().end() - 5, "Queue", 5) == 0)
	    e->_style = es_queue;
	if (e->_e->type_name().length() >= 4
	    && memcmp(e->_e->type_name().begin(), "ICMP", 4) == 0)
	    e->_vertical = false;
    }

    for (RouterT::conn_iterator i = r->begin_connections(); i != r->end_connections(); ++i) {
	conn *c = new conn(_elt[i->from_eindex()], i->from_port(),
			   _elt[i->to_eindex()], i->to_port(), z_index);
	_conn.push_back(c);
	z_index++;
    }
}

void wdiagram::router_create(bool incremental, bool always)
{
    if (!incremental) {
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
	_relt = new elt(0, 0);
	if (_rw->_r) {
	    Vector<ElementT *> path;
	    int z_index = 0;
	    _relt->fill(_rw->_r, _rw->_processing, _elt_map, path, z_index);
	}
	if (!_dir_cursor[0])
	    initialize();
    }
}


/*****
 *
 * SCC layout
 *
 */

class wdiagram::elt::layoutelt : public rectangle { public:
    int scc;
    std::vector<int> scc_contents;
    
    bool visited;
    int active;
    
    int posrel;
    int compact_prev;
    int compact_next;

    rectangle rgroup;

    layoutelt(int n, double width, double height)
	: rectangle(0, 0, width, height), scc(n), visited(false), active(-1),
	  posrel(n), compact_prev(-1), compact_next(-1),
	  rgroup(0, 0, width, height) {
    }
};

void wdiagram::elt::layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc)
{
    const std::vector<int> &scc_contents = layinfo[scc].scc_contents;

    // find compact connections
    for (std::vector<int>::const_iterator iter = scc_contents.begin();
	 iter != scc_contents.end(); ++iter) {
	ElementT *x = router->element(*iter);
	int c;
	if (x->ninputs() != 1
	    || (c = router->find_connection_id_to(PortT(x, 0))) < 0
	    || !connlive[c])
	    continue;
	const PortT &out = router->connection(c).from();
	if (router->find_connection_id_from(out) < 0
	    || out.port != 0
	    || out.element->noutputs() > 2)
	    continue;
	if (out.element->noutputs() == 2) {
	    const String &pc = out.element->type()->processing_code();
	    if (pc != "a/ah" && pc != "l/lh")
		continue;
	    // check that x[1] has <= 1 connection (!= 1 should not happen,
	    // but erroneous configurations could easily make it)
	    if (router->find_connection_id_from(PortT(out.element, 1)) == -2)
		continue;
	}

	// actually position
	int partner = out.eindex();
	layinfo[*iter].posrel = layinfo[partner].posrel;
	layinfo[*iter]._x = layinfo[partner].center_x() - layinfo[*iter].width()/2;
	if (out.element->noutputs() == 2)
	    layinfo[*iter]._y = layinfo[partner].y2() + 10;
	else
	    layinfo[*iter]._y = layinfo[partner].y2() + 4;
	layinfo[layinfo[*iter].posrel].rgroup |= layinfo[*iter];
	layinfo[partner].compact_next = *iter;
	layinfo[*iter].compact_prev = partner;
    }

    // lay out compact connections
    for (std::vector<int>::const_iterator iter = scc_contents.begin();
	 iter != scc_contents.end(); ++iter)
	while (layinfo[layinfo[*iter].posrel].posrel != layinfo[*iter].posrel) {
	    int g = layinfo[*iter].posrel, g2 = layinfo[g].posrel;
	    layinfo[*iter].shift(layinfo[g].x(), layinfo[g].y());
	    layinfo[g].rgroup |= layinfo[*iter];
	    layinfo[*iter].posrel = g2;
	}

    //
    
    // 
    // print SCCs
#if 0
    for (std::vector<int>::const_iterator iter = scc_contents.begin();
	 iter != scc_contents.end(); ++iter)
	if (layinfo[*iter].posrel == *iter) {
	    fprintf(stderr, "COMPACT %d :", *iter);
	    for (std::vector<int>::const_iterator iterx = scc_contents.begin(); iterx != scc_contents.end(); ++iterx)
		if (layinfo[*iterx].posrel == *iter)
		    fprintf(stderr, " %s", router->ename(*iterx).c_str());
	    fprintf(stderr, "\n");
	}
#endif
}

void wdiagram::elt::position_contents_scc(RouterT *router)
{
    // initialize DFS
    Bitvector connlive(router->nconnections(), true);
    std::vector<layoutelt> layinfo;
    for (int i = 0; i < router->nelements(); i++)
	layinfo.push_back(layoutelt(i, _elt[i]->width(), _elt[i]->height()));

    // DFS to find strongly connected components and break cycles
    std::vector<int> connpath, eltpath, connpos;
    Vector<int> conns;
    
    for (int i = 0; i < router->nelements(); ++i) {
	if (layinfo[i].visited)
	    continue;
	assert(connpath.size() == 0 && eltpath.size() == 0 && connpos.size() == 0);
	eltpath.push_back(i);
	connpath.push_back(-1);
	connpos.push_back(-1);
	layinfo[i].active = 0;
	assert(layinfo[i].scc == i);
	int cur_scc = i;

	// loop over connections
	while (connpos.size()) {
	    router->find_connections_from(router->element(eltpath.back()), conns);

	  more_connections:
	    // skip dead connections
	    assert(connpos.back() < conns.size());
	    do {
		++connpos.back();
	    } while (connpos.back() < conns.size() && !connlive[conns[connpos.back()]]);

	    // no more connections: backtrack
	    if (connpos.back() == conns.size()) {
		layinfo[eltpath.back()].active = -1;
		layinfo[eltpath.back()].visited = true;
		layinfo[eltpath.back()].scc = cur_scc;
		connpos.pop_back();
		eltpath.pop_back();
		connpath.pop_back();
		continue;
	    }

	    // live connection
	    int to_eindex = router->connection(conns[connpos.back()]).to_eindex();

	    // break cycle
	    if (layinfo[to_eindex].active >= 0) {
		// example eltpath:    0  1  2
		// example connpath: -1 c# c#: split last
		std::vector<int>::iterator cycle_begin = eltpath.begin() + layinfo[to_eindex].active;
		std::vector<int>::iterator eltpath_max = std::max_element(cycle_begin, eltpath.end());
		int killconn;
		if (eltpath_max + 1 != eltpath.end())
		    killconn = connpath[eltpath_max - eltpath.begin() + 1];
		else
		    killconn = conns[connpos.back()];
		connlive[killconn] = false;
		goto more_connections;
	    }

	    // connect components
	    if (layinfo[to_eindex].visited) {
		cur_scc = MIN(cur_scc, layinfo[to_eindex].scc);
		goto more_connections;
	    } else {
		layinfo[to_eindex].active = eltpath.size();
		connpath.push_back(conns[connpos.back()]);
		eltpath.push_back(to_eindex);
		connpos.push_back(-1);
		continue;
	    }
	}
    }

    // assign SCCs
    for (int i = 0; i < router->nelements(); ++i) {
	assert(layinfo[i].scc <= i);
	layinfo[i].scc = layinfo[layinfo[i].scc].scc;
	layinfo[layinfo[i].scc].scc_contents.push_back(i);
    }

    // lay out SCCs
    for (int i = 0; i < router->nelements(); ++i)
	if (layinfo[i].scc == i)
	    layout_one_scc(router, layinfo, connlive, i);
}


/*****
 *
 * Layout
 *
 */

void wdiagram::elt::position_contents_first_heuristic(RouterT *router, const eltstyle &es)
{
    // lay out into rows
    std::vector<int> row_counts;
    std::vector<double> row_height, col_width;
    int input_eindex = router->eindex("input");
    std::list<int> remains;
    for (size_t i = 0; i < _elt.size(); i++)
	remains.push_back(i);
    Vector<PortT> conn;

    // lay out by row
    while (remains.size()) {
	std::list<int>::iterator best = remains.begin();
	int best_nin_missing = -1, best_nin_present = -1, best_lowest_row = -1;

	// find nonsense
	for (std::list<int>::iterator ri = best; ri != remains.end(); ++ri) {
	    int nin_missing = 0, nin_present = 0, lowest_row = -1;
	    ElementT *elt = _elt[*ri]->_e;
	    for (int i = 0; i < elt->ninputs(); i++) {
		router->find_connections_to(PortT(elt, i), conn);
		for (Vector<PortT>::iterator x = conn.begin(); x != conn.end(); ++x)
		    if (_elt[x->eindex()]->_row < 0)
			nin_missing++;
		    else {
			nin_present++;
			if (_elt[x->eindex()]->_row > lowest_row)
			    lowest_row = _elt[x->eindex()]->_row;
			//			fprintf(stderr, "  %s [%d] -> [%d] %s (%d)\n", x->element->name().c_str(), x->port, i, elt->name().c_str(), _elt[x->eindex()]->_row);
		    }
	    }
	    if (best == ri
		|| (nin_missing == 0 && best_nin_missing > 0)
		|| (nin_present > best_nin_present)) {
		//|| (nin_present == best_nin_present && nin_missing < best_nin_missing)
		//|| (nin_present == best_nin_present && nin_missing == best_nin_missing && lowest_row < best_lowest_row)) {
		best = ri;
		best_nin_missing = nin_missing;
		best_nin_present = nin_present;
		best_lowest_row = lowest_row;
	    }
	}

	assert(_elt[*best]->_row < 0);
	assert(best_lowest_row >= -1);
	
	// lay that one out
	unsigned row;
	if (*best == input_eindex)
	    row = 0;
	else if (best_lowest_row < 0)
	    row = (input_eindex < 0 ? 0 : 1);
	else
	    row = best_lowest_row + 1;

	while (row >= row_counts.size()) {
	    row_counts.push_back(0);
	    row_height.push_back(0);
	}
	unsigned col = row_counts[row];
	if (col == col_width.size())
	    col_width.push_back(0);
	
	_elt[*best]->_row = row;
	_elt[*best]->_rowpos = col;
	row_height[row] = MAX(row_height[row], _elt[*best]->_height);
	row_counts[row]++;
	col_width[col] = MAX(col_width[col], _elt[*best]->_width);

	// remove and continue
	remains.erase(best);
    }

    for (std::vector<double>::iterator i = row_height.begin(); i + 1 < row_height.end(); ++i)
	i[1] += i[0] + es.element_dy;
    col_width[0] = col_width[0] / 2;
    for (std::vector<double>::iterator i = col_width.begin(); i + 1 < col_width.end(); ++i)
	i[1] += i[0] + es.element_dx;

    for (std::vector<elt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei) {
	(*ei)->_xrect._x = col_width[(*ei)->_rowpos] - (*ei)->_width / 2;
	(*ei)->_xrect._y = ((*ei)->_row > 0 ? row_height[(*ei)->_row - 1] + es.element_dy : 0);
    }
}

void wdiagram::elt::position_contents_dot(RouterT *router, const eltstyle &es, ErrorHandler *errh)
{
    StringAccum sa;
    sa << "digraph {\n"
       << "nodesep=" << (es.element_dx / 100) << ";\n"
       << "ranksep=" << (es.element_dy / 100) << ";\n"
       << "node [shape=record]\n";
    for (std::vector<elt *>::size_type i = 0; i < _elt.size(); ++i) {
	elt *e = _elt[i];
	sa << "n" << i << " [width=" << (e->width()/100) << ",height="
	   << (e->height()/100) << ",fixedsize=true,label=\"{{";
	for (int p = 0; p < e->_e->ninputs(); p++)
	    sa << (p ? "|<i" : "<i") << p << ">";
	sa << "}|" << e->_e->name() << "|{";
	for (int p = 0; p < e->_e->noutputs(); p++)
	    sa << (p ? "|<o" : "<o") << p << ">";
	sa << "}}\"];\n";
    }
    for (int i = 0; i < router->nconnections(); i++) {
	const ConnectionT &c = router->connection(i);
	sa << 'n' << c.from_eindex() << ':' << 'o' << c.from_port()
	   << " -> n" << c.to_eindex() << ':' << 'i' << c.to_port()
	   << ';' << '\n';
    }
    sa << "}\n";

    String result = shell_command_output_string("dot", sa.take_string(), errh);

    const char *end = result.end();
    for (const char *s = result.begin(); s != end; ) {
	s = cp_skip_space(s, end);
	if (s + 1 >= end || *s != 'n' || !isdigit((unsigned char) s[1])) {
	  skip_to_semicolon:
	    while (s != end && *s != ';')
		++s;
	    if (s != end)
		++s;
	    continue;
	}

	int eindex;
	s = cp_integer(s + 1, end, 10, &eindex);
	while (s != end && isspace((unsigned char) *s))
	    ++s;
	if (eindex < 0 || (std::vector<elt*>::size_type)eindex >= _elt.size())
	    goto skip_to_semicolon;
	
	if (s == end || *s != '[')
	    goto skip_to_semicolon;

      skip_to_p:
	while (s != end && *s != 'p' && *s != ';') {
	    if (*s == '\"') {
		for (++s; s != end && *s != '\"'; ++s)
		    /* nada */;
	    }
	    ++s;
	}
	if (s == end || *s == ';')
	    goto skip_to_semicolon;
	if (s + 2 >= end || s[1] != 'o' || s[2] != 's') {
	    ++s;
	    goto skip_to_p;
	}
	s = cp_skip_space(s + 3, end);
	if (s == end || *s != '=')
	    goto skip_to_semicolon;
	s = cp_skip_space(s + 1, end);
	if (s != end && *s == '"')
	    ++s;
	s = cp_skip_space(s, end);
	const char *t = s;
	while (t != end && (isdigit((unsigned char) *t) || *t == '.' || *t == '+' || *t == 'e' || *t == 'E' || *t == '-'))
	    ++t;
	if (t == s)
	    goto skip_to_semicolon;
	double x, y;
	if (!cp_double(result.substring(s, t), &x))
	    goto skip_to_semicolon;
	s = cp_skip_space(t, end);
	if (s == end || *s != ',')
	    goto skip_to_semicolon;
	s = t = cp_skip_space(s + 1, end);
	while (t != end && (isdigit((unsigned char) *t) || *t == '.' || *t == '+' || *t == 'e' || *t == 'E' || *t == '-'))
	    ++t;
	if (!cp_double(result.substring(s, t), &y))
	    goto skip_to_semicolon;
	_elt[eindex]->_xrect._x = x * 100. / 72 - _elt[eindex]->_width / 2;
	_elt[eindex]->_xrect._y = -y * 100. / 72 - _elt[eindex]->_height / 2;
	goto skip_to_semicolon;
    }

    double min_x = 1000000, min_y = 1000000;
    for (std::vector<elt *>::iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->_visible) {
	    min_x = MIN(min_x, (*n)->_xrect._x);
	    min_y = MIN(min_y, (*n)->_xrect._y);
	}

    for (std::vector<elt *>::iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->_visible) {
	    (*n)->_xrect._x -= min_x;
	    (*n)->_xrect._y -= min_y;
	}
}

void wdiagram::elt::layout_contents(RouterT *router, wdiagram *cd, PangoLayout *pl)
{
    for (std::vector<elt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	(*ci)->layout(cd, pl);
	(*ci)->_row = -1;
    }

    position_contents_dot(router, cd->_style, cd->_rw->error_handler());
    //position_contents_scc(router);
    //position_contents_first_heuristic(router, cd->_style);

    _contents_width = _contents_height = 0;
    for (std::vector<elt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	_contents_width = MAX(_contents_width, (*ci)->_xrect._x + (*ci)->_width);
	_contents_height = MAX(_contents_height, (*ci)->_xrect._y + (*ci)->_height);
    }
}

void wdiagram::elt::layout(wdiagram *cd, PangoLayout *pl)
{
    if (_layout)
	return;

    // get extents of name and data
    pango_layout_set_attributes(pl, cd->_name_attrs);
    pango_layout_set_text(pl, _e->name().data(), _e->name().length());
    PangoRectangle rect;
    pango_layout_get_pixel_extents(pl, NULL, &rect);
    _name_raw_width = rect.width;
    _name_raw_height = rect.height;

    if (_show_class) {
	pango_layout_set_attributes(pl, cd->_class_attrs);
	pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	pango_layout_get_pixel_extents(pl, NULL, &rect);
	_class_raw_width = rect.width;
	_class_raw_height = rect.height;
    } else
	_class_raw_width = _class_raw_height = 0;

    // get contents width and height
    if (_expanded && _elt.size())
	layout_contents(_e->resolved_router(), cd, pl);
    
    // get element width and height
    const eltstyle &style = cd->_style;
    double xwidth, xheight;

    if (_style == es_queue && _contents_height == 0) {
	xwidth = MAX(_name_raw_height + _class_raw_height,
		     style.min_queue_width);
	xheight = MAX(MAX(_name_raw_width, _class_raw_width) + 2 * style.inside_dy,
		      style.min_queue_height);
    } else {
	xwidth = MAX(MAX(_name_raw_width, _contents_width), _class_raw_width);
	xheight = _name_raw_height + _class_raw_height + 2 * style.inside_dy;
	if (_contents_height)
	    xheight += style.inside_contents_dy + _contents_height;
    }

    double xportlen = MAX(_e->ninputs(), _e->noutputs()) *
	style.min_port_distance + 2 * style.min_port_offset;
    
    if (_vertical)
	_width = ceil(MAX(xwidth + 2 * style.inside_dx, xportlen));
    else {
	_width = ceil(xwidth + 2 * style.inside_dx);
	xheight = MAX(xheight, xportlen);
    }
    
    _height = ceil(style.min_preferred_height +
		   (MAX(ceil((xheight - style.min_preferred_height)
			     / style.height_increment), 0) *
		    style.height_increment));

    _layout = true;
}

void wdiagram::elt::finish_compound(const eltstyle &es)
{
    if (_elt.size() > 0 && _elt[0]->_e->name() == "input") {
	_elt[0]->_x = _x;
	_elt[0]->_y = _y - 10;
	_elt[0]->_width = _width;
	_elt[0]->_height = 10 + es.port_width[0] - 1;
    }
    if (_elt.size() > 1 && _elt[1]->_e->name() == "output") {
	_elt[1]->_x = _x;
	_elt[1]->_y = _y + _height - es.port_width[1] + 1;
	_elt[1]->_width = _width;
	_elt[1]->_height = 10;
    }
}

void wdiagram::elt::finish(const eltstyle &es, double dx, double dy, rect_search<ink> &r)
{
    if (_e) {
	dx += es.inside_dx;
	double text_height = _name_raw_height + _class_raw_height;
	dy += (_height + text_height - _contents_height + es.inside_contents_dy) / 2;
    }

    for (std::vector<elt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci)
	if ((*ci)->_visible) {
	    (*ci)->_x = floor((*ci)->_xrect._x + dx);
	    (*ci)->_y = floor((*ci)->_xrect._y + dy);
	    r.insert(*ci);
	    if ((*ci)->_elt.size())
		(*ci)->finish(es, (*ci)->_xrect._x + dx, (*ci)->_xrect._y + dy, r);
	}

    if (_e && _parent && _elt.size())
	finish_compound(es);

    for (std::vector<conn *>::iterator ci = _conn.begin();
	 ci != _conn.end(); ++ci) {
	(*ci)->finish(es);
	r.insert(*ci);
    }
}

void wdiagram::elt::union_bounds(rectangle &r, bool self) const
{
    if (self)
	r |= *this;
    for (std::vector<elt *>::const_iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci)
	if ((*ci)->_visible)
	    (*ci)->union_bounds(r, true);
    for (std::vector<conn *>::const_iterator ci = _conn.begin();
	 ci != _conn.end(); ++ci)
	r |= **ci;
}

void wdiagram::elt::remove(rect_search<ink> &rects, rectangle &rect)
{
    rect |= *this;
    rects.remove(this);
    
    Vector<int> conn;
    _e->router()->find_connections_to(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	rect |= *_parent->_conn[*iter];
	rects.remove(_parent->_conn[*iter]);
    }

    _e->router()->find_connections_from(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	rect |= *_parent->_conn[*iter];
	rects.remove(_parent->_conn[*iter]);
    }
}

void wdiagram::elt::insert(rect_search<ink> &rects, const eltstyle &style, rectangle &rect)
{
    rect |= *this;
    rects.insert(this);

    Vector<int> conn;
    _e->router()->find_connections_to(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	_parent->_conn[*iter]->finish(style);
	rect |= *_parent->_conn[*iter];
	rects.insert(_parent->_conn[*iter]);
    }

    _e->router()->find_connections_from(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	_parent->_conn[*iter]->finish(style);
	rect |= *_parent->_conn[*iter];
	rects.insert(_parent->_conn[*iter]);
    }
}

void wdiagram::elt::drag_prepare()
{
    _xrect = *this;
    for (std::vector<elt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei)
	(*ei)->drag_prepare();
}

void wdiagram::elt::drag_shift(double dx, double dy, wdiagram *cd)
{
    rectangle rect = *this;
    remove(cd->_rects, rect);
    _x = _xrect._x + dx;
    _y = _xrect._y + dy;
    insert(cd->_rects, cd->_style, rect);
    cd->redraw(rect);
    for (std::vector<elt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei)
	(*ei)->drag_shift(dx, dy, cd);
}

void wdiagram::conn::finish(const eltstyle &style)
{
    double fromx, fromy, tox, toy;
    _from_elt->output_position(_from_port, style, fromx, fromy);
    _to_elt->input_position(_to_port, style, tox, toy);
    if (_from_elt->_vertical && _to_elt->_vertical) {
	_x = MIN(fromx, tox - 3);
	_y = MIN(fromy, toy - 12);
	_width = MAX(fromx, tox + 3) - _x;
	_height = MAX(fromy + 5, toy) - _y;
    } else {
	_x = MIN(fromx, tox - 12);
	_y = MIN(fromy - 2, toy - 12);
	_width = MAX(fromx + 5, tox + 3) - _x;
	_height = MAX(fromy + 5, toy + 3) - _y;
    }
}

void wdiagram::elt::port_offsets(double side_length, int nports, const eltstyle &style, double &offset0, double &separation)
{
    if (nports == 0) {
	offset0 = separation = 0;
	return;
    }

    double pl = style.port_layout_length;
    if (nports == 1) {
	separation = 1;
	offset0 = side_length / 2 + 0.5;
    } else if (pl * nports + style.port_separation * (nports - 1) >= side_length - 2 * style.port_offset) {
	separation = (side_length - 2 * style.min_port_offset - pl) / (nports - 1);
	offset0 = style.min_port_offset + pl / 2 + 0.5;
    } else {
	separation = (side_length - 2 * style.port_offset + style.port_separation) / nports;
	offset0 = style.port_offset + (separation - style.port_separation) / 2 + 0.5;
    }
}

static void cairo_processing_rgb(cairo_t *cr, int processing)
{
    if (processing == ProcessingT::VAGNOSTIC)
	cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
    else if (processing & ProcessingT::VPUSH)
	cairo_set_source_rgb(cr, 0, 0, 0);
    else
	cairo_set_source_rgb(cr, 1, 1, 1);
}

void wdiagram::elt::draw_input_port(cairo_t *cr, const eltstyle &style, double x, double y, int processing)
{
    double pl = style.port_length[0];
    double pw = style.port_width[0];
    double as = style.port_agnostic_separation[0];
    cairo_set_line_width(cr, 1);
    cairo_matrix_t original_matrix;
    cairo_get_matrix(cr, &original_matrix);
    cairo_translate(cr, x, y);
    if (!_vertical)
	cairo_rotate(cr, -M_PI / 2);

    for (int i = 0; i < 2; i++) {
	cairo_move_to(cr, -pl / 2, 0);
	cairo_line_to(cr, 0, pw);
	cairo_line_to(cr, pl / 2, 0);
	if (processing != ProcessingT::VPUSH && i == 0)
	    cairo_set_source_rgb(cr, 1, 1, 1);
	else
	    cairo_set_source_rgb(cr, 0, 0, 0);
	if (i == 0) {
	    cairo_close_path(cr);
	    cairo_fill(cr);
	} else
	    cairo_stroke(cr);
    }
    
    if (processing == ProcessingT::VAGNOSTIC
	|| (processing & ProcessingT::VAFLAG))
	for (int i = 0; i < 2; i++) {
	    cairo_move_to(cr, -pl / 2 + as, 0);
	    cairo_line_to(cr, 0, pw - 1.15 * as);
	    cairo_line_to(cr, pl / 2 - as, 0);
	    if (i == 0) {
		cairo_processing_rgb(cr, processing);
		cairo_close_path(cr);
		cairo_fill(cr);
	    } else {
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_set_line_width(cr, 0.75);
		cairo_stroke(cr);
	    }
	}

    cairo_set_matrix(cr, &original_matrix);
}

void wdiagram::elt::draw_output_port(cairo_t *cr, const eltstyle &style, double x, double y, int processing)
{
    double pl = style.port_length[1];
    double pw = style.port_width[1];
    double as = style.port_agnostic_separation[1];
    cairo_set_line_width(cr, 1);
    cairo_matrix_t original_matrix;
    cairo_get_matrix(cr, &original_matrix);
    cairo_translate(cr, x, y);
    if (!_vertical)
	cairo_rotate(cr, -M_PI / 2);

    for (int i = 0; i < 2; i++) {
	cairo_move_to(cr, -pl / 2, 0);
	cairo_line_to(cr, -pl / 2, -pw);
	cairo_line_to(cr, pl / 2, -pw);
	cairo_line_to(cr, pl / 2, 0);
	if (processing != ProcessingT::VPUSH && i == 0)
	    cairo_set_source_rgb(cr, 1, 1, 1);
	else
	    cairo_set_source_rgb(cr, 0, 0, 0);
	if (i == 0) {
	    cairo_close_path(cr);
	    cairo_fill(cr);
	} else
	    cairo_stroke(cr);
    }
    
    if (processing == ProcessingT::VAGNOSTIC
	|| (processing & ProcessingT::VAFLAG))
	for (int i = 0; i < 2; i++) {
	    cairo_move_to(cr, -pl / 2 + as, 0);
	    cairo_line_to(cr, -pl / 2 + as, -pw + as);
	    cairo_line_to(cr, pl / 2 - as, -pw + as);
	    cairo_line_to(cr, pl / 2 - as, 0);
	    if (i == 0) {
		cairo_processing_rgb(cr, processing);	
		cairo_close_path(cr);
		cairo_fill(cr);
	    } else {
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_set_line_width(cr, 0.75);
		cairo_stroke(cr);
	    }
	}

    cairo_set_matrix(cr, &original_matrix);
}

void wdiagram::elt::clip_to_border(cairo_t *cr, double shift) const
{
    double fx = floor(_x + shift), fy = floor(_y + shift);
    cairo_rectangle(cr, fx, fy,
		    ceil(_x + shift + _width - fx),
		    ceil(_y + shift + _height - fy));
    cairo_clip(cr);
}

void wdiagram::elt::draw_text(wdiagram *cd, cairo_t *cr, PangoLayout *pl, double shift)
{
    // text
    cairo_set_source_rgb(cr, 0, 0, 0);
    pango_layout_set_wrap(pl, PANGO_WRAP_CHAR);

    double max_text_width = MAX(_name_raw_width, _class_raw_width);
    if (max_text_width > _width - 2
	|| _name_raw_height + _class_raw_height > _height - 2)
	cairo_save(cr);
    
    if (_width - 2 < max_text_width && _height - 2 > max_text_width
	&& !_elt.size()) {
	// vertical layout
	if (_name_raw_height + _class_raw_height > _width - 2)
	    clip_to_border(cr, shift);
	
	cairo_translate(cr, _x + shift, _y + shift + _height);
	cairo_rotate(cr, -M_PI / 2);
	cairo_stroke(cr);

	pango_layout_set_width(pl, (int) ((_height - 2) * PANGO_SCALE));
	pango_layout_set_attributes(pl, cd->_name_attrs);
	pango_layout_set_text(pl, _e->name().data(), _e->name().length());
	double dy = MAX((_width - _name_raw_height - _class_raw_height) / 2, 0);
	cairo_move_to(cr, MAX(_height / 2 - _name_raw_width / 2, 1), dy);
	pango_cairo_show_layout(cr, pl);
	
	if (_show_class) {
	    pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	    pango_layout_set_attributes(pl, cd->_class_attrs);
	    cairo_move_to(cr, MAX(_height / 2 - _class_raw_width / 2, 1), dy + _name_raw_height);
	    pango_cairo_show_layout(cr, pl);
	}

    } else {
	// normal horizontal layout, no text wrapping
	if (max_text_width > _width - 2
	    || _name_raw_height + _class_raw_height > _height - 2)
	    clip_to_border(cr, shift);

	pango_layout_set_width(pl, (int) ((_width - 2) * PANGO_SCALE));
	pango_layout_set_attributes(pl, cd->_name_attrs);
	pango_layout_set_text(pl, _e->name().data(), _e->name().length());

	double name_width, name_height;
	if (_width - 2 >= _name_raw_width)
	    name_width = _name_raw_width, name_height = _name_raw_height;
	else {
	    PangoRectangle rect;
	    pango_layout_get_pixel_extents(pl, NULL, &rect);
	    name_width = _width - 2, name_height = rect.height;
	}

	double class_width, class_height;
	if (_width - 2 >= _class_raw_width)
	    class_width = _class_raw_width, class_height = _class_raw_height;
	else {
	    pango_layout_set_attributes(pl, cd->_class_attrs);
	    pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	    PangoRectangle rect;
	    pango_layout_get_pixel_extents(pl, NULL, &rect);
	    class_width = _width - 2, class_height = rect.height;
	    pango_layout_set_attributes(pl, cd->_name_attrs);
	    pango_layout_set_text(pl, _e->name().data(), _e->name().length());
	}

	double dy;
	if (_elt.size())
	    dy = cd->_style.inside_dy;
	else
	    dy = MAX((_height - name_height - class_height) / 2, 1);
	cairo_move_to(cr, _x + shift + MAX(_width / 2 - name_width / 2, 1),
		      _y + dy + shift);
	pango_cairo_show_layout(cr, pl);

	if (_show_class) {
	    pango_layout_set_attributes(pl, cd->_class_attrs);
	    pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	    cairo_move_to(cr, _x + shift + MAX(_width / 2 - class_width / 2, 1),
			  _y + shift + dy + name_height);
	    pango_cairo_show_layout(cr, pl);
	}
    }

    if (max_text_width > _width - 2
	|| _name_raw_height + _class_raw_height > _height - 2)
	cairo_restore(cr);
}

void wdiagram::elt::draw_outline(wdiagram *cd, cairo_t *cr, PangoLayout *, double shift)
{
    const eltstyle &style = cd->_style;
    
    // shadow
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.2, 0.5);
    cairo_set_line_width(cr, 3 - shift);
    cairo_move_to(cr, _x + 3, _y + _height + 1.5 + shift / 2);
    cairo_line_to(cr, _x + _width + 1.5 + shift / 2, _y + _height + 1.5 + shift / 2);
    cairo_line_to(cr, _x + _width + 1.5 + shift / 2, _y + 3);
    cairo_stroke(cr);

    // outline
    if (_style == es_queue) {
	cairo_set_source_rgb(cr, 0.87, 0.87, 0.5);
	cairo_set_line_width(cr, 1);
	for (int i = 1; i * style.queue_line_sep <= _height - style.inside_dy; ++i) {
	    cairo_move_to(cr, _x + shift + 0.5,
			  _y + shift - 0.5 + _height - floor(i * style.queue_line_sep));
	    cairo_rel_line_to(cr, _width, 0);
	}
	cairo_stroke(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, _x + shift + 0.5, _y + shift + 0.5);
	cairo_rel_line_to(cr, 0, _height - 1);
	cairo_rel_line_to(cr, _width - 1, 0);
	cairo_rel_line_to(cr, 0, -_height + 1);
	cairo_stroke(cr);
	    
    } else {
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_set_line_width(cr, 1);
	cairo_move_to(cr, _x + shift + 0.5, _y + shift + 0.5);
	cairo_rel_line_to(cr, _width - 1, 0);
	cairo_rel_line_to(cr, 0, _height - 1);
	cairo_rel_line_to(cr, -_width + 1, 0);
	cairo_close_path(cr);
	cairo_stroke(cr);
    }
}

void wdiagram::elt::draw(wdiagram *cd, cairo_t *cr, PangoLayout *pl)
{
    if (!_visible)
	return;
    
    // draw background
    if (_highlight & (1 << htype_click))
	cairo_set_source_rgb(cr, 1, 1, 180./255);
    else if (_highlight & (1 << htype_hover))
	cairo_set_source_rgb(cr, 1, 1, 242./255);
    else
	cairo_set_source_rgb(cr, 1.00 - .02 * MIN(_depth - 1, 4),
			     1.00 - .02 * MIN(_depth - 1, 4),
			     0.87 - .06 * MIN(_depth - 1, 4));

    double shift = (_highlight & (1 << htype_pressed) ? 1 : 0);
    
    cairo_move_to(cr, _x + shift, _y + shift);
    cairo_rel_line_to(cr, _width, 0);
    cairo_rel_line_to(cr, 0, _height);
    cairo_rel_line_to(cr, -_width, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // contents drawn by rectsearch based on z_index!

    // draw ports
    double offset0, separation;
    const char *pcpos = _processing_code.begin();
    int pcode;
    if (_vertical) {
	port_offsets(_width, _e->ninputs(), cd->_style, offset0, separation);
	offset0 += _x + shift;
	for (int i = 0; i < _e->ninputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_input_port(cr, cd->_style, offset0 + separation * i, _y + shift + 0.5, pcode);
	}
	pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
	port_offsets(_width, _e->noutputs(), cd->_style, offset0, separation);
	offset0 += _x + shift;
	for (int i = 0; i < _e->noutputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_output_port(cr, cd->_style, offset0 + separation * i, _y + shift + _height - 0.5, pcode);
	}
    } else {
	port_offsets(_height, _e->ninputs(), cd->_style, offset0, separation);
	offset0 += _y + shift;
	for (int i = 0; i < _e->ninputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_input_port(cr, cd->_style, _x + shift + 0.5, offset0 + separation * i, pcode);
	}
	pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
	port_offsets(_height, _e->noutputs(), cd->_style, offset0, separation);
	offset0 += _y + shift;
	for (int i = 0; i < _e->noutputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_output_port(cr, cd->_style, _x + shift + _width - 0.5, offset0 + separation * i, pcode);
	}
    }

    // outline
    draw_outline(cd, cr, pl, shift);

    // text
    draw_text(cd, cr, pl, shift);
}

void wdiagram::conn::draw(wdiagram *cd, cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1);

    double fromx, fromy, tox, toy;
    _from_elt->output_position(_from_port, cd->_style, fromx, fromy);
    _to_elt->input_position(_to_port, cd->_style, tox, toy);
    cairo_move_to(cr, fromx, fromy);
    if ((_from_elt->_vertical && _to_elt->_vertical && fabs(tox - fromx) <= 6)
	|| (!_from_elt->_vertical && !_to_elt->_vertical && fabs(toy - fromy) <= 6))
	/* no curves */;
    else {
	point curvea;
	if (_from_elt->_vertical) {
	    cairo_line_to(cr, fromx, fromy + 3);
	    curvea = point(fromx, fromy + 10);
	} else {
	    cairo_line_to(cr, fromx + 3, fromy);
	    curvea = point(fromx + 10, fromy);
	}
	if (_to_elt->_vertical)
	    cairo_curve_to(cr, curvea.x(), curvea.y(),
			   tox, toy - 12, tox, toy - 7);
	else
	    cairo_curve_to(cr, curvea.x(), curvea.y(),
			   tox - 12, toy, tox - 7, toy);
    }
    cairo_line_to(cr, tox, toy);
    cairo_stroke(cr);

    if (_to_elt->_vertical) {
	cairo_move_to(cr, tox, toy + 0.25);
	cairo_line_to(cr, tox - 3, toy - 5.75);
	cairo_line_to(cr, tox, toy - 3.75);
	cairo_line_to(cr, tox + 3, toy - 5.75);
    } else {
	cairo_move_to(cr, tox + 0.25, toy);
	cairo_line_to(cr, tox - 5.75, toy - 3);
	cairo_line_to(cr, tox - 3.75, toy);
	cairo_line_to(cr, tox - 5.75, toy + 3);
    }
    cairo_close_path(cr);
    cairo_fill(cr);
}

void wdiagram::layout()
{
    //fprintf(stderr, "Layout\n");
    if (!_relt)
	router_create(true, true);
    if (!_layout && _rw->_r) {
	PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);
	ElementMap::push_default(_rw->element_map());
	_relt->layout_contents(_rw->_r, this, pl);
	_rects.clear();
	_relt->finish(_style, lay_border, lay_border, _rects);
	_relt->_x = _relt->_y = 0;
	_relt->_width = _relt->_contents_width + 2 * lay_border;
	_relt->_height = _relt->_contents_height + 2 * lay_border;
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
	cairo_set_line_width(cr, 4);
	const GdkColor &bgcolor = _widget->style->bg[GTK_STATE_ACTIVE];
	cairo_set_source_rgb(cr, bgcolor.red / 65535., bgcolor.green / 65535., bgcolor.blue / 65535.);
	rectangle r = canvas_to_window(_dragr).normalize();
	if (r.width() < 8 || r.height() < 8) {
	    cairo_rectangle(cr, r.x(), r.y(), r.width(), r.height());
	    cairo_fill(cr);
	} else {
	    cairo_rectangle(cr, r.x() + 2, r.y() + 2, r.width() - 4, r.height() - 4);
	    cairo_stroke(cr);
	}
    }
    
    PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);

    rectangle r(area->x + _origin_x, area->y + _origin_y,
		area->width, area->height);
    r.scale(1 / _scale);
    r.expand(elt_expand);
    std::vector<ink *> elts;
    find_rect_elts(r, elts);

    cairo_translate(cr, -_origin_x, -_origin_y);
    cairo_scale(cr, _scale, _scale);
    for (std::vector<ink *>::iterator eltsi = elts.begin();
	 eltsi != elts.end(); ++eltsi)
	if (elt *e = (*eltsi)->cast_elt())
	    e->draw(this, cr, pl);
	else {
	    assert((*eltsi)->cast_conn());
	    (*eltsi)->cast_conn()->draw(this, cr);
	}
    
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
 * motion exposure
 *
 */

void wdiagram::unhighlight(uint8_t htype, rectangle *expose)
{
    assert(htype <= htype_pressed);
    while (elt *h = _highlight[htype]) {
	h->_highlight &= ~(1 << htype);
	if (!_layout)
	    /* do nothing */;
	else if (expose)
	    *expose |= *h;
	else
	    redraw(*h);
	if (htype == htype_click) {
	    _highlight[htype] = h->_next_htype_click;
	    h->_next_htype_click = 0;
	} else
	    _highlight[htype] = 0;
    }
}

wdiagram::elt *wdiagram::point_elt(const point &p) const
{
    std::vector<ink *> elts;
    _rects.find_all(p.x(), p.y(), elts);
    std::sort(elts.begin(), elts.end(), ink::z_index_greater);
    std::vector<ink *>::iterator eltsi = std::unique(elts.begin(), elts.end());
    elts.erase(eltsi, elts.end());
    for (eltsi = elts.begin(); eltsi != elts.end(); ++eltsi)
	if ((*eltsi)->contains(p))
	    if (elt *e = (*eltsi)->cast_elt())
		if (e->_visible)
		    return e;
    return 0;
}

void wdiagram::highlight(elt *h, uint8_t htype, rectangle *expose, bool scroll_to)
{
    bool same = (h == _highlight[htype]);
    if (!same || (h && !(h->_highlight & (1 << htype)))
	|| (htype == htype_click && h && h->_next_htype_click)) {
	unhighlight(htype, expose);
	if ((_highlight[htype] = h)) {
	    h->_highlight |= (1 << htype);
	    if (!_layout)
		/* do nothing */;
	    else if (expose)
		*expose |= *h;
	    else
		redraw(*h);
	}
    }
    if (h && scroll_to && _layout) {
	GtkAdjustment *ha = _horiz_adjust, *va = _vert_adjust;
	point h_tl = canvas_to_window(h->x(), h->y());
	point h_br = canvas_to_window(h->x2() + elt_shadow, h->y2() + elt_shadow);
	
	if (h_br.x() >= ha->value + ha->page_size
	    && h_tl.x() >= floor(h_br.x() - ha->page_size))
	    gtk_adjustment_set_value(ha, floor(h_br.x() - ha->page_size));
	else if (h_br.x() >= ha->value + ha->page_size
		 || h_tl.x() < ha->value)
	    gtk_adjustment_set_value(ha, floor(h_tl.x() - 4));
	
	if (h_br.y() >= va->value + va->page_size
	    && h_tl.y() >= floor(h_br.y() - va->page_size))
	    gtk_adjustment_set_value(va, floor(h_br.y() - va->page_size));
	else if (h_br.y() >= va->value + va->page_size
		 || h_tl.y() < va->value)
	    gtk_adjustment_set_value(va, floor(h_tl.y() - 4));
    }
}

void wdiagram::on_drag_motion(const point &p)
{
    elt *h = _highlight[htype_click];
    if (_drag_state == drag_start
	&& (fabs(p.x() - _dragr.x()) * _scale >= 3
	    || fabs(p.y() - _dragr.y()) * _scale >= 3)) {
	for (elt *hx = h; hx; hx = hx->_next_htype_click)
	    hx->drag_prepare();
	_drag_state = drag_dragging;
    }
    
    if (_drag_state == drag_dragging && _last_cursorno == c_c) {
	for (elt *hx = h; hx; hx = hx->_next_htype_click)
	    hx->drag_shift(p.x() - _dragr.x(), p.y() - _dragr.y(), this);
    } else if (_drag_state == drag_dragging) {
	// assume that _highlight[htype_hover] is relevant
	elt *h = _highlight[htype_hover];
	assert(h);
	rectangle r = *h;
	h->remove(_rects, r);
	int vtype = _last_cursorno % 3;
	int htype = _last_cursorno - vtype;
	double dx = p.x() - _dragr.x(), dy = p.y() - _dragr.y();
	if (vtype == c_top && h->_xrect._height - dy >= _style.min_dimen) {
	    h->_y = h->_xrect._y + dy;
	    h->_height = h->_xrect._height - dy;
	} else if (vtype == c_bot && h->_xrect._height + dy >= _style.min_dimen)
	    h->_height = h->_xrect._height + dy;
	if (htype == c_lft && h->_xrect._width - dx >= _style.min_dimen) {
	    h->_x = h->_xrect._x + dx;
	    h->_width = h->_xrect._width - dx;
	} else if (htype == c_rt && h->_xrect._width + dx >= _style.min_dimen)
	    h->_width = h->_xrect._width + dx;
	h->insert(_rects, _style, r);
	
	if (h->_parent && h->_elt.size()) {
	    h->_elt[0]->remove(_rects, r);
	    h->_elt[1]->remove(_rects, r);
	    h->finish_compound(_style);
	    h->_elt[0]->insert(_rects, _style, r);
	    h->_elt[1]->insert(_rects, _style, r);
	}
	
	redraw(r);
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
	
	elt **pprev = &_highlight[htype_click];
	for (elt *e = *pprev; e; e = *pprev)
	    if (e->_highlight & (1 << htype_rect_click)) {
		e->_highlight &= ~((1 << htype_rect_click) | (1 << htype_click));
		*pprev = e->_next_htype_click;
		e->_next_htype_click = 0;
		to_redraw |= *e;
	    } else
		pprev = &e->_next_htype_click;
	
	_dragr._width = p.x() - _dragr.x();
	_dragr._height = p.y() - _dragr.y();

	std::vector<ink *> elts;
	find_rect_elts(_dragr.normalize(), elts);
	for (std::vector<ink *>::iterator iter = elts.begin();
	     iter != elts.end(); ++iter)
	    if (elt *e = (*iter)->cast_elt())
		if (!(e->_highlight & (1 << htype_click))) {
		    e->_highlight |= (1 << htype_rect_click) | (1 << htype_click);
		    e->_next_htype_click = _highlight[htype_click];
		    _highlight[htype_click] = e;
		    to_redraw |= *e;
		}

	to_redraw |= _dragr.normalize();
	redraw(to_redraw);
    }
}

void wdiagram::on_drag_complete()
{
    rectangle r = *_relt;
    for (elt *h = _highlight[htype_click]; h; h = h->_next_htype_click)
	if (!(h->_xrect.x() > _relt->x() + drag_threshold
	      && h->x() > _relt->x() + drag_threshold
	      && h->_xrect.x2() < _relt->x2() - drag_threshold
	      && h->x2() < _relt->x2() - drag_threshold
	      && h->_xrect.y() > _relt->y() + drag_threshold
	      && h->y() > _relt->y() + drag_threshold
	      && h->_xrect.y2() < _relt->y2() - drag_threshold
	      && h->y2() < _relt->y2() - drag_threshold)) {
	    point old_ctr = scroll_center();
	    _relt->assign(h->_x, h->_y, 0, 0);
	    _relt->union_bounds(*_relt, false);
	    _relt->_contents_width = _relt->_width;
	    _relt->_contents_height = _relt->_height;
	    _relt->expand(lay_border);
	    scroll_recenter(old_ctr);
	    break;
	}
}

void wdiagram::on_drag_rect_complete()
{
    std::vector<ink *> elts;
    find_rect_elts(_dragr, elts);
    for (std::vector<ink *>::iterator iter = elts.begin(); iter != elts.end(); ++iter)
	if (elt *e = (*iter)->cast_elt())
	    e->_highlight &= ~(1 << htype_rect_click);
    redraw(_dragr.normalize());
}

void wdiagram::set_cursor(elt *h, double x, double y)
{
    int cnum = c_c;
    if (h) {
	point h_tl = canvas_to_window(h->x1(), h->y1());
	point h_br = canvas_to_window(h->x2(), h->y2());
	double attach = MAX(2.0, _scale);
	if (_scale_step > -5
	    && h_br.x() - h_tl.x() >= 18 && h_br.y() - h_tl.y() >= 18
	    && (x - h_tl.x() < attach || y - h_tl.y() < attach
		|| h_br.x() - x < attach || h_br.y() - y < attach)) {
	    cnum = c_c;
	    if (x - h_tl.x() < 12)
		cnum += c_lft;
	    else if (h_br.x() - x < 12)
		cnum += c_rt;
	    if (y - h_tl.y() < 12)
		cnum += c_top;
	    else if (h_br.y() - y < 12)
		cnum += c_bot;
	}
    }
    if (_last_cursorno != cnum) {
	_last_cursorno = cnum;
	gdk_window_set_cursor(_widget->window, _dir_cursor[_last_cursorno]);
    }
}

gboolean wdiagram::on_event(GdkEvent *event)
{
    if (event->type == GDK_MOTION_NOTIFY) {
	point p = window_to_canvas(event->motion.x, event->motion.y);
	if (!(event->motion.state & GDK_BUTTON1_MASK)) {
	    elt *h = point_elt(p);
	    highlight(h, htype_hover, 0, false);
	    set_cursor(h, event->motion.x, event->motion.y);
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
	elt *h = point_elt(_dragr.origin());
	_drag_state = (h ? drag_start : drag_rect_start);

	if (!(event->button.state & GDK_SHIFT_MASK)) {
	    if (!h || !(h->_highlight & (1 << htype_click)))
		highlight(h, htype_click, 0, false);
	    if (h)
		_rw->element_show(h->_flat_name, 0, true);
	} else if (h && (h->_highlight & (1 << htype_click))) {
	    elt **prev = &_highlight[htype_click];
	    while (*prev && *prev != h)
		prev = &(*prev)->_next_htype_click;
	    if (*prev == h)
		*prev = h->_next_htype_click;
	    h->_highlight &= ~(1 << htype_click);
	    h->_next_htype_click = 0;
	    _drag_state = drag_none;
	} else if (h) {
	    h->_highlight |= 1 << htype_click;
	    h->_next_htype_click = _highlight[htype_click];
	    _highlight[htype_click] = h;
	}

	highlight(h, htype_pressed, 0, false);
	
    } else if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1) {
	if (_drag_state == drag_dragging)
	    on_drag_complete();
	else if (_drag_state == drag_rect_dragging)
	    on_drag_rect_complete();
	_drag_state = drag_none;
	unhighlight(htype_pressed, 0);
	
    } else if (event->type == GDK_2BUTTON_PRESS && event->button.button == 1) {
	elt *h = point_elt(window_to_canvas(event->button.x, event->button.y));
	highlight(h, htype_click, 0, true);
	if (h)
	    _rw->element_show(h->_flat_name, 1, true);
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
