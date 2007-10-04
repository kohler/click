#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "dwidget.hh"
#include "diagram.hh"
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <clicktool/processingt.hh>
#include <clicktool/elementmap.hh>
#include <list>
#include <math.h>
#include "wrouter.hh"
#include "whandler.hh"
extern "C" {
#include "support.h"
}
namespace clicky {

delt::~delt()
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

void delt::fill(RouterT *r, ProcessingT *processing, HashMap<String, delt *> &collector, Vector<ElementT *> &path, int &z_index)
{
    for (RouterT::iterator i = r->begin_elements(); i != r->end_elements(); ++i) {
	delt *e = new delt(this, z_index);
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
	    e->_style = esflag_queue;
	if (e->_e->type_name().length() >= 4
	    && memcmp(e->_e->type_name().begin(), "ICMP", 4) == 0)
	    e->_vertical = false;
    }

    for (RouterT::conn_iterator i = r->begin_connections(); i != r->end_connections(); ++i) {
	dconn *c = new dconn(_elt[i->from_eindex()], i->from_port(),
			     _elt[i->to_eindex()], i->to_port(), z_index);
	_conn.push_back(c);
	z_index++;
    }
}


/*****
 *
 * SCC layout
 *
 */

class delt::layoutelt : public rectangle { public:
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

void delt::layout_one_scc(RouterT *router, std::vector<layoutelt> &layinfo, const Bitvector &connlive, int scc)
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

void delt::position_contents_scc(RouterT *router)
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

void delt::position_contents_first_heuristic(RouterT *router, const dstyle &style)
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
	i[1] += i[0] + style.element_dy;
    col_width[0] = col_width[0] / 2;
    for (std::vector<double>::iterator i = col_width.begin(); i + 1 < col_width.end(); ++i)
	i[1] += i[0] + style.element_dx;

    for (std::vector<delt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei) {
	(*ei)->_xrect._x = col_width[(*ei)->_rowpos] - (*ei)->_width / 2;
	(*ei)->_xrect._y = ((*ei)->_row > 0 ? row_height[(*ei)->_row - 1] + style.element_dy : 0);
    }
}

void delt::position_contents_dot(RouterT *router, const dstyle &es, ErrorHandler *errh)
{
    StringAccum sa;
    sa << "digraph {\n"
       << "nodesep=" << (es.element_dx / 100) << ";\n"
       << "ranksep=" << (es.element_dy / 100) << ";\n"
       << "node [shape=record]\n";
    for (std::vector<delt *>::size_type i = 0; i < _elt.size(); ++i) {
	delt *e = _elt[i];
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
	if (eindex < 0 || (std::vector<delt*>::size_type)eindex >= _elt.size())
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
    for (std::vector<delt *>::iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->_visible) {
	    min_x = MIN(min_x, (*n)->_xrect._x);
	    min_y = MIN(min_y, (*n)->_xrect._y);
	}

    for (std::vector<delt *>::iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->_visible) {
	    (*n)->_xrect._x -= min_x;
	    (*n)->_xrect._y -= min_y;
	}
}

void delt::layout_contents(wdiagram *d, RouterT *router, PangoLayout *pl)
{
    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	(*ci)->layout(d, pl);
	(*ci)->_row = -1;
    }

    position_contents_dot(router, d->style(), d->main()->error_handler());
    //position_contents_scc(router);
    //position_contents_first_heuristic(router, d->style());

    _contents_width = _contents_height = 0;
    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	_contents_width = MAX(_contents_width, (*ci)->_xrect._x + (*ci)->_width);
	_contents_height = MAX(_contents_height, (*ci)->_xrect._y + (*ci)->_height);
    }
}

void delt::layout(wdiagram *cd, PangoLayout *pl)
{
    if (_layout)
	return;

    // get extents of name and data
    pango_layout_set_attributes(pl, cd->style().name_attrs);
    pango_layout_set_text(pl, _e->name().data(), _e->name().length());
    PangoRectangle rect;
    pango_layout_get_pixel_extents(pl, NULL, &rect);
    _name_raw_width = rect.width;
    _name_raw_height = rect.height;

    if (_show_class) {
	pango_layout_set_attributes(pl, cd->style().class_attrs);
	pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	pango_layout_get_pixel_extents(pl, NULL, &rect);
	_class_raw_width = rect.width;
	_class_raw_height = rect.height;
    } else
	_class_raw_width = _class_raw_height = 0;

    // get contents width and height
    if (_expanded && _elt.size())
	layout_contents(cd, _e->resolved_router(), pl);
    
    // get element width and height
    const dstyle &style = cd->style();
    double xwidth, xheight;

    if ((_style & esflag_queue) && _contents_height == 0) {
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

void delt::layout_compound_ports(const dstyle &style)
{
    if (_elt.size() > 0 && _elt[0]->_e->name() == "input") {
	_elt[0]->_x = _x;
	_elt[0]->_y = _y - 10;
	_elt[0]->_width = _width;
	_elt[0]->_height = 10 + style.port_width[0] - 1;
    }
    if (_elt.size() > 1 && _elt[1]->_e->name() == "output") {
	_elt[1]->_x = _x;
	_elt[1]->_y = _y + _height - style.port_width[1] + 1;
	_elt[1]->_width = _width;
	_elt[1]->_height = 10;
    }
}

void delt::layout_complete(wdiagram *d, double dx, double dy)
{
    if (_e) {
	dx += d->style().inside_dx;
	double text_height = _name_raw_height + _class_raw_height;
	dy += (_height + text_height - _contents_height
	       + d->style().inside_contents_dy) / 2;
    }

    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci)
	if ((*ci)->_visible) {
	    (*ci)->_x = floor((*ci)->_xrect._x + dx);
	    (*ci)->_y = floor((*ci)->_xrect._y + dy);
	    d->rects().insert(*ci);
	    if ((*ci)->_elt.size())
		(*ci)->layout_complete(d, (*ci)->_xrect._x + dx,
				       (*ci)->_xrect._y + dy);
	}

    if (_e && _parent && _elt.size())
	layout_compound_ports(d->style());

    for (std::vector<dconn *>::iterator ci = _conn.begin();
	 ci != _conn.end(); ++ci) {
	(*ci)->layout(d->style());
	d->rects().insert(*ci);
    }
}

void delt::layout_main(wdiagram *d, RouterT *router, PangoLayout *pl)
{
    d->rects().clear();
    layout_contents(d, router, pl);
    layout_complete(d, lay_border, lay_border);
    assign(0, 0, _contents_width + 2 * lay_border, _contents_height + 2 * lay_border);
}

void dconn::layout(const dstyle &style)
{
    double fromx, fromy, tox, toy;
    _from_elt->output_position(_from_port, style, fromx, fromy);
    _to_elt->input_position(_to_port, style, tox, toy);
    if (_from_elt->vertical() && _to_elt->vertical()) {
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

/*****
 *
 * recalculating bounds
 *
 */

void delt::layout_recompute_bounds()
{
    if (_elt.size()) {
	assert(_elt[0]->visible());
	assign(*_elt[0]);
	union_bounds(*this, false);
	_contents_width = _width;
	_contents_height = _height;
	expand(lay_border);
    }
}

void delt::union_bounds(rectangle &r, bool self) const
{
    if (self)
	r |= *this;
    for (std::vector<delt *>::const_iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci)
	if ((*ci)->_visible)
	    (*ci)->union_bounds(r, true);
    for (std::vector<dconn *>::const_iterator ci = _conn.begin();
	 ci != _conn.end(); ++ci)
	r |= **ci;
}

void delt::remove(rect_search<dwidget> &rects, rectangle &rect)
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

void delt::insert(rect_search<dwidget> &rects, const dstyle &style, rectangle &rect)
{
    rect |= *this;
    rects.insert(this);

    Vector<int> conn;
    _e->router()->find_connections_to(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	_parent->_conn[*iter]->layout(style);
	rect |= *_parent->_conn[*iter];
	rects.insert(_parent->_conn[*iter]);
    }

    _e->router()->find_connections_from(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	_parent->_conn[*iter]->layout(style);
	rect |= *_parent->_conn[*iter];
	rects.insert(_parent->_conn[*iter]);
    }
}

/*****
 *
 * drawing
 *
 */

void delt::port_offsets(double side_length, int nports, const dstyle &style, double &offset0, double &separation)
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

void delt::draw_input_port(cairo_t *cr, const dstyle &style, double x, double y, int processing)
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

void delt::draw_output_port(cairo_t *cr, const dstyle &style, double x, double y, int processing)
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

void delt::clip_to_border(cairo_t *cr, double shift) const
{
    double fx = floor(_x + shift), fy = floor(_y + shift);
    cairo_rectangle(cr, fx, fy,
		    ceil(_x + shift + _width - fx),
		    ceil(_y + shift + _height - fy));
    cairo_clip(cr);
}

void delt::draw_text(wdiagram *cd, cairo_t *cr, PangoLayout *pl, double shift)
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
	pango_layout_set_attributes(pl, cd->style().name_attrs);
	pango_layout_set_text(pl, _e->name().data(), _e->name().length());
	double dy = MAX((_width - _name_raw_height - _class_raw_height) / 2, 0);
	cairo_move_to(cr, MAX(_height / 2 - _name_raw_width / 2, 1), dy);
	pango_cairo_show_layout(cr, pl);
	
	if (_show_class) {
	    pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	    pango_layout_set_attributes(pl, cd->style().class_attrs);
	    cairo_move_to(cr, MAX(_height / 2 - _class_raw_width / 2, 1), dy + _name_raw_height);
	    pango_cairo_show_layout(cr, pl);
	}

    } else {
	// normal horizontal layout, no text wrapping
	if (max_text_width > _width - 2
	    || _name_raw_height + _class_raw_height > _height - 2)
	    clip_to_border(cr, shift);

	pango_layout_set_width(pl, (int) ((_width - 2) * PANGO_SCALE));
	pango_layout_set_attributes(pl, cd->style().name_attrs);
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
	    pango_layout_set_attributes(pl, cd->style().class_attrs);
	    pango_layout_set_text(pl, _e->type_name().data(), _e->type_name().length());
	    PangoRectangle rect;
	    pango_layout_get_pixel_extents(pl, NULL, &rect);
	    class_width = _width - 2, class_height = rect.height;
	    pango_layout_set_attributes(pl, cd->style().name_attrs);
	    pango_layout_set_text(pl, _e->name().data(), _e->name().length());
	}

	double dy;
	if (_elt.size())
	    dy = cd->style().inside_dy;
	else
	    dy = MAX((_height - name_height - class_height) / 2, 1);
	cairo_move_to(cr, _x + shift + MAX(_width / 2 - name_width / 2, 1),
		      _y + dy + shift);
	pango_cairo_show_layout(cr, pl);

	if (_show_class) {
	    pango_layout_set_attributes(pl, cd->style().class_attrs);
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

void delt::draw_outline(wdiagram *cd, cairo_t *cr, PangoLayout *, double shift)
{
    const dstyle &style = cd->style();
    
    // shadow
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.2, 0.5);
    cairo_set_line_width(cr, 3 - shift);
    cairo_move_to(cr, _x + 3, _y + _height + 1.5 + shift / 2);
    cairo_line_to(cr, _x + _width + 1.5 + shift / 2, _y + _height + 1.5 + shift / 2);
    cairo_line_to(cr, _x + _width + 1.5 + shift / 2, _y + 3);
    cairo_stroke(cr);

    // outline
    if (_style & esflag_queue) {
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

void delt::draw(wdiagram *cd, cairo_t *cr, PangoLayout *pl)
{
    if (!_visible)
	return;
    
    // draw background
    if (_highlight & (1 << dhlt_click))
	cairo_set_source_rgb(cr, 1, 1, 180./255);
    else if (_highlight & (1 << dhlt_hover))
	cairo_set_source_rgb(cr, 1, 1, 242./255);
    else
	cairo_set_source_rgb(cr, 1.00 - .02 * MIN(_depth - 1, 4),
			     1.00 - .02 * MIN(_depth - 1, 4),
			     0.87 - .06 * MIN(_depth - 1, 4));

    double shift = (_highlight & (1 << dhlt_pressed) ? 1 : 0);
    
    cairo_move_to(cr, _x + shift, _y + shift);
    cairo_rel_line_to(cr, _width, 0);
    cairo_rel_line_to(cr, 0, _height);
    cairo_rel_line_to(cr, -_width, 0);
    cairo_close_path(cr);
    cairo_fill(cr);

    // draw fullness
    if ((_style & esflag_fullness) && _hvalue_fullness >= 0) {
	cairo_set_source_rgba(cr, 0, 0, 1.0, 0.2);
	cairo_move_to(cr, _x + shift, _y + _height + shift);
	if (_vertical) {
	    cairo_rel_line_to(cr, _width, 0);
	    cairo_rel_line_to(cr, 0, -_height * _hvalue_fullness);
	    cairo_rel_line_to(cr, -_width, 0);
	} else {
	    cairo_rel_line_to(cr, 0, -_height);
	    cairo_rel_line_to(cr, _width * _hvalue_fullness, 0);
	    cairo_rel_line_to(cr, 0, _height);
	}
	cairo_close_path(cr);
	cairo_fill(cr);
    }
    
    // contents drawn by rectsearch based on z_index!

    // draw ports
    double offset0, separation;
    const char *pcpos = _processing_code.begin();
    int pcode;
    if (_vertical) {
	port_offsets(_width, _e->ninputs(), cd->style(), offset0, separation);
	offset0 += _x + shift;
	for (int i = 0; i < _e->ninputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_input_port(cr, cd->style(), offset0 + separation * i, _y + shift + 0.5, pcode);
	}
	pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
	port_offsets(_width, _e->noutputs(), cd->style(), offset0, separation);
	offset0 += _x + shift;
	for (int i = 0; i < _e->noutputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_output_port(cr, cd->style(), offset0 + separation * i, _y + shift + _height - 0.5, pcode);
	}
    } else {
	port_offsets(_height, _e->ninputs(), cd->style(), offset0, separation);
	offset0 += _y + shift;
	for (int i = 0; i < _e->ninputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_input_port(cr, cd->style(), _x + shift + 0.5, offset0 + separation * i, pcode);
	}
	pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
	port_offsets(_height, _e->noutputs(), cd->style(), offset0, separation);
	offset0 += _y + shift;
	for (int i = 0; i < _e->noutputs(); i++) {
	    pcpos = ProcessingT::processing_code_next(pcpos, _processing_code.end(), pcode);
	    draw_output_port(cr, cd->style(), _x + shift + _width - 0.5, offset0 + separation * i, pcode);
	}
    }

    // outline
    draw_outline(cd, cr, pl, shift);

    // text
    draw_text(cd, cr, pl, shift);
}

void dconn::draw(wdiagram *cd, cairo_t *cr)
{
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1);

    double fromx, fromy, tox, toy;
    _from_elt->output_position(_from_port, cd->style(), fromx, fromy);
    _to_elt->input_position(_to_port, cd->style(), tox, toy);
    cairo_move_to(cr, fromx, fromy);
    if ((_from_elt->vertical() && _to_elt->vertical() && fabs(tox - fromx) <= 6)
	|| (!_from_elt->vertical() && !_to_elt->vertical() && fabs(toy - fromy) <= 6))
	/* no curves */;
    else {
	point curvea;
	if (_from_elt->vertical()) {
	    cairo_line_to(cr, fromx, fromy + 3);
	    curvea = point(fromx, fromy + 10);
	} else {
	    cairo_line_to(cr, fromx + 3, fromy);
	    curvea = point(fromx + 10, fromy);
	}
	if (_to_elt->vertical())
	    cairo_curve_to(cr, curvea.x(), curvea.y(),
			   tox, toy - 12, tox, toy - 7);
	else
	    cairo_curve_to(cr, curvea.x(), curvea.y(),
			   tox - 12, toy, tox - 7, toy);
    }
    cairo_line_to(cr, tox, toy);
    cairo_stroke(cr);

    if (_to_elt->vertical()) {
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

/*****
 *
 * Dragging
 *
 */

void delt::drag_prepare()
{
    _xrect = *this;
    for (std::vector<delt *>::iterator ei = _elt.begin();
	 ei != _elt.end(); ++ei)
	(*ei)->drag_prepare();
}

void delt::drag_shift(wdiagram *d, const point &delta)
{
    rectangle bounds = *this;
    remove(d->rects(), bounds);
    _x = _xrect._x + delta.x();
    _y = _xrect._y + delta.y();
    insert(d->rects(), d->style(), bounds);
    d->redraw(bounds);
    for (std::vector<delt *>::iterator ei = _elt.begin();
	 ei != _elt.end(); ++ei)
	(*ei)->drag_shift(d, delta);
}

void delt::drag_size(wdiagram *d, const point &delta, int direction)
{
    rectangle bounds = *this;
    remove(d->rects(), bounds);
    
    int vtype = direction % 3;
    int htype = direction - vtype;
    
    if (vtype == deg_border_top
	&& _xrect._height - delta.y() >= d->style().min_dimen) {
	_y = _xrect._y + delta.y();
	_height = _xrect._height - delta.y();
    } else if (vtype == deg_border_bot
	       && _xrect._height + delta.y() >= d->style().min_dimen)
	_height = _xrect._height + delta.y();
    
    if (htype == deg_border_lft
	&& _xrect._width - delta.x() >= d->style().min_dimen) {
	_x = _xrect._x + delta.x();
	_width = _xrect._width - delta.x();
    } else if (htype == deg_border_rt
	       && _xrect._width + delta.x() >= d->style().min_dimen)
	_width = _xrect._width + delta.x();
    
    insert(d->rects(), d->style(), bounds);
    
    if (_parent && _elt.size()) {
	_elt[0]->remove(d->rects(), bounds);
	_elt[1]->remove(d->rects(), bounds);
	layout_compound_ports(d->style());
	_elt[0]->insert(d->rects(), d->style(), bounds);
	_elt[1]->insert(d->rects(), d->style(), bounds);
    }
    
    d->redraw(bounds);
}

bool delt::drag_canvas_changed(const rectangle &canvas) const
{
    return !(_xrect.x() > canvas.x() + drag_threshold
	     && x() > canvas.x() + drag_threshold
	     && _xrect.x2() < canvas.x2() - drag_threshold
	     && x2() < canvas.x2() - drag_threshold
	     && _xrect.y() > canvas.y() + drag_threshold
	     && y() > canvas.y() + drag_threshold
	     && _xrect.y2() < canvas.y2() - drag_threshold
	     && y2() < canvas.y2() - drag_threshold);
}

/*****
 *
 * gadgets
 *
 */

bool delt::expand_handlers(wmain *w)
{
    handler_value *hv =
	w->hvalues().find_force(_flat_name + ".handlers");
    if (!hv->have_hvalue()) {
	hv->set_flags(w, hv->flags() | hflag_notify_delt);
	hv->refresh(w);
	return true;
    } else
	return false;
}

void delt::gadget_fullness(wdiagram *d)
{
    handler_value *lv = d->main()->hvalues().find(_flat_name + ".length");
    handler_value *cv = d->main()->hvalues().find(_flat_name + ".capacity");
    if (!lv || !cv) {
	expand_handlers(d->main());
	return;
    }
    
    if (!lv->have_hvalue())
	lv->refresh(d->main());
    if (!cv->have_hvalue())
	cv->refresh(d->main());

    double l, c, old_f = _hvalue_fullness;
    if (lv->have_hvalue() && cv->have_hvalue()
	&& cp_double(lv->hvalue(), &l) && cp_double(cv->hvalue(), &c)
	&& l >= 0 && l <= c)
	_hvalue_fullness = l / c;
    else
	_hvalue_fullness = -1;

    if ((old_f < 0) != (_hvalue_fullness < 0)
	|| fabs(old_f - _hvalue_fullness) * (_vertical ? _height : _width) * d->scale() > 0.5)
	d->redraw(*this);
}

void delt::add_gadget(wdiagram *d, int gadget)
{
    assert(gadget == esflag_fullness);
    if (gadget == esflag_fullness && (_style & esflag_fullness) == 0) {
	_style |= esflag_fullness;
	_hvalue_fullness = -1;
	gadget_fullness(d);
    }
}

void delt::remove_gadget(wdiagram *d, int gadget)
{
    assert(gadget == esflag_fullness);
    if (gadget == esflag_fullness && (_style & esflag_fullness) != 0) {
	_style &= ~esflag_fullness;
	_hvalue_fullness = -1;
	d->redraw(*this);
    }
}

void delt::notify_read(wdiagram *d, handler_value *hv)
{
    (void) hv;
    if (_style & esflag_fullness)
	gadget_fullness(d);
}

/*****
 *
 * parts
 *
 */

int delt::find_gadget(wdiagram *d, double window_x, double window_y) const
{
    rectangle r = d->canvas_to_window(*this);
    r.integer_align();
    if (!r.contains(window_x, window_y))
	return deg_none;
    
    double attach = MAX(2.0, d->scale());
    if (d->scale_step() > -5
	&& r.width() >= 18
	&& r.height() >= 18
	&& (window_x - r.x1() < attach
	    || window_y - r.y1() < attach
	    || r.x2() - window_x < attach
	    || r.y2() - window_y < attach)) {
	int gnum = deg_main;
	if (window_x - r.x1() < 12)
	    gnum += deg_border_lft;
	else if (r.x2() - window_x < 12)
	    gnum += deg_border_rt;
	if (window_y - r.y1() < 12)
	    gnum += deg_border_top;
	else if (r.y2() - window_y < 12)
	    gnum += deg_border_bot;
	return gnum;
    }

    return deg_main;
}

}
