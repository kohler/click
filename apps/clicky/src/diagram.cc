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

extern "C" {
static void diagram_map(GtkWidget *, gpointer);
static gboolean on_diagram_event(GtkWidget *, GdkEvent *, gpointer);
static gboolean diagram_expose(GtkWidget *, GdkEventExpose *, gpointer);
}

ClickyDiagram::ClickyDiagram(RouterWindow *rw)
    : _rw(rw), _scale_step(0), _scale(1), _relt(0)
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
    _eltstyle.port_offset = 4 * portscale;
    _eltstyle.min_port_offset = 3 * portscale;
    _eltstyle.port_separation = 3 * portscale;
    _eltstyle.min_port_distance = 7 * portscale;
    _eltstyle.port_layout_length = 6 * portscale;
    _eltstyle.port_length[0] = 7 * portscale;
    _eltstyle.port_width[0] = 4.5 * portscale;
    _eltstyle.port_length[1] = 6 * portscale;
    _eltstyle.port_width[1] = 3.8 * portscale;
    _eltstyle.agnostic_separation = 1.25 * portscale;
    _eltstyle.port_agnostic_separation[0] = sqrt(2.) * _eltstyle.agnostic_separation;
    _eltstyle.port_agnostic_separation[1] = _eltstyle.agnostic_separation;
    _eltstyle.inside_dx = 7.5 * portscale;
    _eltstyle.inside_dy = 4.5 * portscale;
    _eltstyle.inside_contents_dy = 3 * portscale;
    _eltstyle.min_height = 19 * portscale;
    _eltstyle.height_increment = 4;
    _eltstyle.element_dx = 8 * portscale;
    _eltstyle.element_dy = 8 * portscale;
    
    g_signal_connect(G_OBJECT(_widget), "event",
		     G_CALLBACK(on_diagram_event), this);
    g_signal_connect(G_OBJECT(_widget), "expose-event",
		     G_CALLBACK(diagram_expose), this);
    g_signal_connect(G_OBJECT(_widget), "map",
		     G_CALLBACK(diagram_map), this);

    _highlight[0] = _highlight[1] = _highlight[2] = 0;
    for (int i = 0; i < 9; i++)
	_dir_cursor[i] = 0;
    _last_cursorno = c_c;
}

ClickyDiagram::~ClickyDiagram()
{
    pango_attr_list_unref(_class_attrs);
    delete _relt;
    _relt = 0;
    for (int i = 0; i < 9; i++)
	if (_dir_cursor[i])
	    gdk_cursor_unref(_dir_cursor[i]);
}

void ClickyDiagram::initialize()
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


void ClickyDiagram::display(const String &ename, bool scroll_to)
{
    if (elt *e = _elt_map[ename])
	highlight(e, htype_click, 0, scroll_to);
}

void ClickyDiagram::zoom(bool incremental, int amount)
{
    _scale_step = (incremental ? _scale_step + amount : amount);

    if (_layout) {
	GtkAdjustment *ha = _horiz_adjust, *va = _vert_adjust;
	double old_x_center = (ha->value + ha->page_size / 2) / _scale;
	double old_y_center = (va->value + va->page_size / 2) / _scale;
    
	_scale = pow(1.2, _scale_step);

	gtk_layout_set_size(GTK_LAYOUT(_widget), (guint) (_relt->_width * _scale), (guint) (_relt->_height * _scale));
	
	redraw();

	double scaled_width = ha->page_size / _scale;
	double scaled_height = va->page_size / _scale;

	if (old_x_center - scaled_width / 2 < 0
	    && old_x_center + scaled_width / 2 <= _relt->_width)
	    gtk_adjustment_set_value(ha, 0);
	else if (old_x_center + scaled_width / 2 > _relt->_width
		 && old_x_center - scaled_width / 2 >= 0)
	    gtk_adjustment_set_value(ha, _relt->_width * _scale - ha->page_size);
	else
	    gtk_adjustment_set_value(ha, (old_x_center - scaled_width / 2) * _scale);

	if (old_y_center - scaled_height / 2 < 0
	    && old_y_center + scaled_height / 2 <= _relt->_height)
	    gtk_adjustment_set_value(va, 0);
	else if (old_y_center + scaled_height / 2 > _relt->_height
		 && old_y_center - scaled_height / 2 >= 0)
	    gtk_adjustment_set_value(va, _relt->_height * _scale - va->page_size);
	else
	    gtk_adjustment_set_value(va, (old_y_center - scaled_height / 2) * _scale);
    } else
	_scale = pow(1.2, _scale_step);	
}

ClickyDiagram::elt::~elt()
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

void ClickyDiagram::elt::fill(RouterT *r, ProcessingT *processing, HashMap<String, elt *> &collector, Vector<ElementT *> &path, int &z_index)
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
    }

    for (RouterT::conn_iterator i = r->begin_connections(); i != r->end_connections(); ++i) {
	conn *c = new conn(_elt[i->from_eindex()], i->from_port(),
			   _elt[i->to_eindex()], i->to_port(), z_index);
	_conn.push_back(c);
	z_index++;
    }
}

void ClickyDiagram::router_create(bool incremental, bool always)
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

class ClickyDiagram::elt::layoutelt : public rectangle { public:
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

void ClickyDiagram::elt::layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc)
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

void ClickyDiagram::elt::position_contents_scc(RouterT *router)
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

void ClickyDiagram::elt::position_contents_first_heuristic(RouterT *router, const eltstyle &es)
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

void ClickyDiagram::elt::position_contents_dot(RouterT *router, const eltstyle &es, ErrorHandler *errh)
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

void ClickyDiagram::elt::layout_contents(RouterT *router, ClickyDiagram *cd, PangoLayout *pl)
{
    for (std::vector<elt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	(*ci)->layout(cd, pl);
	(*ci)->_row = -1;
    }

    position_contents_dot(router, cd->_eltstyle, cd->_rw->error_handler());
    //position_contents_scc(router);
    //position_contents_first_heuristic(router, cd->_eltstyle);

    _contents_width = _contents_height = 0;
    for (std::vector<elt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	_contents_width = MAX(_contents_width, (*ci)->_xrect._x + (*ci)->_width);
	_contents_height = MAX(_contents_height, (*ci)->_xrect._y + (*ci)->_height);
    }
}

void ClickyDiagram::elt::layout(ClickyDiagram *cd, PangoLayout *pl)
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
    const eltstyle &es = cd->_eltstyle;
    double w = MAX(MAX(_name_raw_width, _contents_width), _class_raw_width);
    _width = ceil(MAX(w + 2 * es.inside_dx,
		 MAX(_e->ninputs(), _e->noutputs()) * es.min_port_distance
		      + 2 * es.min_port_offset));
    
    double want_height = _name_raw_height + _class_raw_height + 2 * es.inside_dy;
    if (_contents_height)
	want_height += es.inside_contents_dy + _contents_height;
    _height = ceil(es.min_height
		   + MAX(ceil((want_height - es.min_height)
			      / es.height_increment), 0) * es.height_increment);

    _layout = true;
}

void ClickyDiagram::elt::finish_compound(const eltstyle &es)
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

void ClickyDiagram::elt::finish(const eltstyle &es, double dx, double dy, rect_search<ink> &r)
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

void ClickyDiagram::elt::remove(rect_search<ink> &rects, rectangle &rect)
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

void ClickyDiagram::elt::insert(rect_search<ink> &rects, const eltstyle &style, rectangle &rect)
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

void ClickyDiagram::elt::drag_prepare()
{
    _xrect = *this;
    for (std::vector<elt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei)
	(*ei)->drag_prepare();
}

void ClickyDiagram::elt::drag_shift(double dx, double dy, ClickyDiagram *cd)
{
    rectangle rect = *this;
    remove(cd->_rects, rect);
    _x = _xrect._x + dx;
    _y = _xrect._y + dy;
    insert(cd->_rects, cd->_eltstyle, rect);
    cd->redraw(rect);
    for (std::vector<elt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei)
	(*ei)->drag_shift(dx, dy, cd);
}

void ClickyDiagram::conn::finish(const eltstyle &style)
{
    double fromx, fromy, tox, toy;
    _from_elt->output_position(_from_port, style, fromx, fromy);
    _to_elt->input_position(_to_port, style, tox, toy);
    _x = MIN(fromx, tox - 3);
    _y = MIN(fromy, toy - 12);
    _width = MAX(fromx, tox + 3) - _x;
    _height = MAX(fromy + 5, toy) - _y;
}

void ClickyDiagram::elt::port_position(double side_length, int, int nports, const eltstyle &style, double &offset0, double &separation)
{
    if (nports == 0) {
	offset0 = separation = 0;
	return;
    }

    double pl = style.port_layout_length;
    if (pl * nports + style.port_separation * (nports - 1) >= side_length - 2 * style.port_offset) {
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

void ClickyDiagram::elt::draw_input_port(cairo_t *cr, const eltstyle &style, double x, double y, int processing)
{
    double pl = style.port_length[0];
    double pw = style.port_width[0];
    double as = style.port_agnostic_separation[0];
    cairo_set_line_width(cr, 1);

    for (int i = 0; i < 2; i++) {
	cairo_move_to(cr, x - pl / 2, y);
	cairo_line_to(cr, x, y + pw);
	cairo_line_to(cr, x + pl / 2, y);
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
	    cairo_move_to(cr, x - pl / 2 + as, y);
	    cairo_line_to(cr, x, y + pw - 1.15 * as);
	    cairo_line_to(cr, x + pl / 2 - as, y);
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
}

void ClickyDiagram::elt::draw_output_port(cairo_t *cr, const eltstyle &style, double x, double y, int processing)
{
    double pl = style.port_length[1];
    double pw = style.port_width[1];
    double as = style.port_agnostic_separation[1];
    cairo_set_line_width(cr, 1);

    for (int i = 0; i < 2; i++) {
	cairo_move_to(cr, x - pl / 2, y);
	cairo_line_to(cr, x - pl / 2, y - pw);
	cairo_line_to(cr, x + pl / 2, y - pw);
	cairo_line_to(cr, x + pl / 2, y);
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
	    cairo_move_to(cr, x - pl / 2 + as, y);
	    cairo_line_to(cr, x - pl / 2 + as, y - pw + as);
	    cairo_line_to(cr, x + pl / 2 - as, y - pw + as);
	    cairo_line_to(cr, x + pl / 2 - as, y);
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
}
	    
void ClickyDiagram::elt::draw(ClickyDiagram *cd, cairo_t *cr, PangoLayout *pl)
{
    if (!_visible)
	return;
    
    // background
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

    // input ports
    double offset, separation;
    const char *pcpos = _processing_code.begin();
    int pcode;
    port_position(_width, 0, _e->ninputs(), cd->_eltstyle, offset, separation);
    offset += _x + shift;
    for (int i = 0; i < _e->ninputs(); i++) {
	pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	draw_input_port(cr, cd->_eltstyle, offset + separation * i, _y + shift + 0.5, pcode);
    }
    pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
    port_position(_width, 1, _e->noutputs(), cd->_eltstyle, offset, separation);
    offset += _x + shift;
    for (int i = 0; i < _e->noutputs(); i++) {
	pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	draw_output_port(cr, cd->_eltstyle, offset + separation * i, _y + shift + _height - 0.5, pcode);
    }
    
    // text
    pango_layout_set_wrap(pl, PANGO_WRAP_CHAR);
    cairo_set_source_rgb(cr, 0, 0, 0);

    double x = MAX(_name_raw_width, _class_raw_width);
    if (_width < x && _height > x && !_elt.size()) {
	// vertical layout
	cairo_save(cr);
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

	cairo_restore(cr);
	
    } else {
	// normal horizontal layout
	pango_layout_set_width(pl, (int) ((_width - 2) * PANGO_SCALE));
	pango_layout_set_attributes(pl, cd->_name_attrs);
	pango_layout_set_text(pl, _e->name().data(), _e->name().length());

	double dy;
	if (_elt.size())
	    dy = cd->_eltstyle.inside_dy;
	else
	    dy = MAX((_height - _name_raw_height - _class_raw_height) / 2, 1);
	cairo_move_to(cr, _x + shift + MAX(_width / 2 - _name_raw_width / 2, 1), _y + dy + shift);
	pango_cairo_show_layout(cr, pl);

	if (_show_class) {
	    pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	    pango_layout_set_attributes(pl, cd->_class_attrs);
	    cairo_move_to(cr, _x + shift + MAX(_width / 2 - _class_raw_width / 2, 1), _y + shift + dy + _name_raw_height);
	    pango_cairo_show_layout(cr, pl);
	}
    }

    // outline
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.2, 0.5);
    cairo_set_line_width(cr, 3 - shift);
    cairo_move_to(cr, _x + 3, _y + _height + 1.5 + shift / 2);
    cairo_line_to(cr, _x + _width + 1.5 + shift / 2, _y + _height + 1.5 + shift / 2);
    cairo_line_to(cr, _x + _width + 1.5 + shift / 2, _y + 3);
    cairo_stroke(cr);
    
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, _x + shift + 0.5, _y + shift + 0.5);
    cairo_rel_line_to(cr, _width - 1, 0);
    cairo_rel_line_to(cr, 0, _height - 1);
    cairo_rel_line_to(cr, -_width + 1, 0);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void ClickyDiagram::conn::draw(ClickyDiagram *cd, cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0, 0, 0);

    double fromx, fromy, tox, toy;
    _from_elt->output_position(_from_port, cd->_eltstyle, fromx, fromy);
    _to_elt->input_position(_to_port, cd->_eltstyle, tox, toy);
    cairo_move_to(cr, fromx, fromy);
    if (fabs(tox - fromx) >= 6) {
	cairo_line_to(cr, fromx, fromy + 3);
	cairo_curve_to(cr, fromx, fromy + 10, tox, toy - 12, tox, toy - 7);
    }
    cairo_line_to(cr, tox, toy);
    cairo_stroke(cr);

    cairo_move_to(cr, tox, toy);
    cairo_line_to(cr, tox - 3, toy - 6);
    cairo_line_to(cr, tox, toy - 4);
    cairo_line_to(cr, tox + 3, toy - 6);
    cairo_close_path(cr);
    cairo_fill(cr);
}

void ClickyDiagram::layout()
{
    //fprintf(stderr, "Layout\n");
    if (!_relt)
	router_create(true, true);
    if (!_layout && _rw->_r) {
	PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);
	ElementMap::push_default(_rw->element_map());
	_relt->layout_contents(_rw->_r, this, pl);
	_rects.clear();
	_relt->finish(_eltstyle, 4, 4, _rects);
	_relt->_x = _relt->_y = 0;
	_relt->_width = _relt->_contents_width + 8;
	_relt->_height = _relt->_contents_height + 8;
	g_object_unref(G_OBJECT(pl));
	gtk_layout_set_size(GTK_LAYOUT(_widget), (guint) (_relt->_width * _scale), (guint) (_relt->_height * _scale));
	ElementMap::pop_default();
	_layout = true;
    }
}

void ClickyDiagram::on_expose(const GdkRectangle *area, bool clip)
{
    if (!_layout)
	layout();

    cairo_t *cr = gdk_cairo_create(GTK_LAYOUT(_widget)->bin_window);
    if (clip) {
	cairo_rectangle(cr, area->x, area->y, area->width, area->height);
	cairo_clip(cr);
    }

    // background
    cairo_rectangle(cr, area->x, area->y, area->width, area->height);
    const GdkColor &bgcolor = _widget->style->bg[GTK_STATE_NORMAL];
    cairo_set_source_rgb(cr, bgcolor.red / 65535., bgcolor.green / 65535., bgcolor.blue / 65535.);
    cairo_fill(cr);
    
    PangoLayout *pl = gtk_widget_create_pango_layout(_widget, NULL);

    rectangle r(area->x, area->y, area->width, area->height);
    r.scale(1 / _scale);
    r.expand(elt_expand);
    std::vector<ink *> elts;
    _rects.find_all(r, elts);
    std::sort(elts.begin(), elts.end(), ink::z_index_less);
    std::vector<ink *>::iterator eltsi = std::unique(elts.begin(), elts.end());
    elts.erase(eltsi, elts.end());

    cairo_scale(cr, _scale, _scale);
    for (eltsi = elts.begin(); eltsi != elts.end(); ++eltsi)
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
gboolean diagram_expose(GtkWidget *, GdkEventExpose *e, gpointer user_data)
{
    ClickyDiagram *cd = reinterpret_cast<ClickyDiagram *>(user_data);
    cd->on_expose(&e->area, true);
    return FALSE;
}

void diagram_map(GtkWidget *, gpointer user_data)
{
    reinterpret_cast<ClickyDiagram *>(user_data)->router_create(true, true);
}
}



/*****
 *
 * motion exposure
 *
 */

void ClickyDiagram::unhighlight(uint8_t htype, rectangle *expose)
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

ClickyDiagram::elt *ClickyDiagram::point_elt(double x, double y) const
{
    std::vector<ink *> elts;
    x /= _scale, y /= _scale;
    _rects.find_all(x, y, elts);
    std::sort(elts.begin(), elts.end(), ink::z_index_greater);
    std::vector<ink *>::iterator eltsi = std::unique(elts.begin(), elts.end());
    elts.erase(eltsi, elts.end());
    for (eltsi = elts.begin(); eltsi != elts.end(); ++eltsi)
	if ((*eltsi)->contains(x, y))
	    if (elt *e = (*eltsi)->cast_elt())
		if (e->_visible)
		    return e;
    return 0;
}

void ClickyDiagram::highlight(elt *h, uint8_t htype, rectangle *expose, bool scroll_to)
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
	
	if ((h->x2() + elt_shadow) * _scale >= ha->value + ha->page_size
	    && h->x() * _scale >= floor((h->x2() + elt_shadow) * _scale - ha->page_size))
	    gtk_adjustment_set_value(ha, floor((h->x2() + elt_shadow) * _scale - ha->page_size));
	else if ((h->x2() + elt_shadow) * _scale >= ha->value + ha->page_size
		 || h->x1() * _scale < ha->value)
	    gtk_adjustment_set_value(ha, floor(h->x() * _scale) - 4);
	
	if ((h->y2() + elt_shadow) * _scale >= va->value + va->page_size
	    && h->y() * _scale >= floor((h->y2() + elt_shadow) * _scale - va->page_size))
	    gtk_adjustment_set_value(va, floor((h->y2() + elt_shadow) * _scale - va->page_size));
	else if ((h->y2() + elt_shadow) * _scale >= va->value + va->page_size
		 || h->y1() * _scale < va->value)
	    gtk_adjustment_set_value(va, floor(h->y() * _scale) - 4);
    }
}

void ClickyDiagram::on_drag_motion(double x, double y)
{
    elt *h = _highlight[htype_click];
    if (_drag_state == 0
	&& (fabs(x - _drag_first_x) * _scale >= 3
	    || fabs(y - _drag_first_y) * _scale >= 3)) {
	for (elt *hx = h; hx; hx = hx->_next_htype_click)
	    hx->drag_prepare();
	_drag_state = 1;
    }
    
    if (_drag_state == 1 && _last_cursorno == c_c) {
	for (elt *hx = h; hx; hx = hx->_next_htype_click)
	    hx->drag_shift(x - _drag_first_x, y - _drag_first_y, this);
    } else if (_drag_state == 1) {
	// assume that _highlight[htype_hover] is relevant
	elt *h = _highlight[htype_hover];
	assert(h);
	rectangle r = *h;
	h->remove(_rects, r);
	int vtype = _last_cursorno % 3;
	int htype = _last_cursorno - vtype;
	double dx = x - _drag_first_x, dy = y - _drag_first_y;
	if (vtype == c_top && h->_xrect._height - dy > 3 * _scale) {
	    h->_y = h->_xrect._y + dy;
	    h->_height = h->_xrect._height - dy;
	} else if (vtype == c_bot && h->_xrect._height + dy > 3 * _scale)
	    h->_height = h->_xrect._height + dy;
	if (htype == c_lft && h->_xrect._width - dx > 3 * _scale) {
	    h->_x = h->_xrect._x + dx;
	    h->_width = h->_xrect._width - dx;
	} else if (htype == c_rt && h->_xrect._width + dx > 3 * _scale)
	    h->_width = h->_xrect._width + dx;
	h->insert(_rects, _eltstyle, r);
	
	if (h->_parent && h->_elt.size()) {
	    h->_elt[0]->remove(_rects, r);
	    h->_elt[1]->remove(_rects, r);
	    h->finish_compound(_eltstyle);
	    h->_elt[0]->insert(_rects, _eltstyle, r);
	    h->_elt[1]->insert(_rects, _eltstyle, r);
	}
	
	redraw(r);
    }
}

void ClickyDiagram::set_cursor(elt *h, double x, double y)
{
    int cnum = c_c;
    if (h) {
	double hx1 = h->x1() * _scale;
	double hy1 = h->y1() * _scale;
	double hx2 = h->x2() * _scale;
	double hy2 = h->y2() * _scale;
	double attach = MAX(2.0, _scale);
	if (hx2 - hx1 >= 6 && hy2 - hy1 >= 6
	    && (x - hx1 < attach || y - hy1 < attach
		|| hx2 - x < attach || hy2 - y < attach)) {
	    cnum = c_c;
	    if (x - hx1 < 12)
		cnum += c_lft;
	    else if (hx2 - x < 12)
		cnum += c_rt;
	    if (y - hy1 < 12)
		cnum += c_top;
	    else if (hy2 - y < 12)
		cnum += c_bot;
	}
    }
    if (_last_cursorno != cnum) {
	_last_cursorno = cnum;
	gdk_window_set_cursor(_widget->window, _dir_cursor[_last_cursorno]);
    }
}

gboolean ClickyDiagram::on_event(GdkEvent *event)
{
    if (event->type == GDK_MOTION_NOTIFY) {
	if (!(event->motion.state & GDK_BUTTON1_MASK)) {
	    elt *h = point_elt(event->motion.x, event->motion.y);
	    highlight(h, htype_hover, 0, false);
	    set_cursor(h, event->motion.x, event->motion.y);
	} else if (_highlight[htype_click])
	    on_drag_motion(event->motion.x / _scale, event->motion.y / _scale);
	
	GdkModifierType mod;
	gint mx, my;
	(void) gdk_window_get_pointer(_widget->window, &mx, &my, &mod);

    } else if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
	elt *h = point_elt(event->button.x, event->button.y);
	if (h) {
	    _drag_first_x = event->button.x / _scale;
	    _drag_first_y = event->button.y / _scale;
	    _drag_state = 0;
	}

	if (!(event->button.state & GDK_SHIFT_MASK)) {
	    //|| !_highlight[htype_click]) {
	    //if (!h || !(h->_highlight & (1 << htype_click)))
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
	    _drag_state = -1;
	} else if (h) {
	    h->_highlight |= 1 << htype_click;
	    h->_next_htype_click = _highlight[htype_click];
	    _highlight[htype_click] = h;
	} else
	    _drag_state = -1;

	highlight(h, htype_pressed, 0, false);
	
    } else if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1) {
	unhighlight(htype_pressed, 0);
	
    } else if (event->type == GDK_2BUTTON_PRESS && event->button.button == 1) {
	elt *h = point_elt(event->button.x, event->button.y);
	highlight(h, htype_click, 0, true);
	if (h)
	    _rw->element_show(h->_flat_name, 1, true);
    }
    
    return FALSE;
}

extern "C" {
static gboolean on_diagram_event(GtkWidget *, GdkEvent *event, gpointer user_data)
{
    ClickyDiagram *cd = reinterpret_cast<ClickyDiagram *>(user_data);
    return cd->on_event(event);
}
}
