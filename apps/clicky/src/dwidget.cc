#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "dwidget.hh"
#include "dstyle.hh"
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

void dcontext::set_font_description(const String &font)
{
    if (pl_font != font) {
	PangoFontDescription *font_desc = pango_font_description_from_string(font.c_str());
	pango_layout_set_font_description(pl, font_desc);
	pango_font_description_free(font_desc);
	pl_font = font;
    }
}


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
    delete[] _portoff[0];
    delete[] _portoff[1];
}

void delt::prepare(wdiagram *d, ElementT *e, ProcessingT *processing,
		   HashMap<String, delt *> &collector, Vector<ElementT *> &path,
		   int &z_index)
{
    assert(!_e);
    _e = e;
    _processing_code = processing->decorated_processing_code(_e);
    _des = d->css_set()->elt_style(this);
    if (_des->style == destyle_queue)
	_dqs = d->css_set()->queue_style(this);

    if (_e->tunnel())
	return;

    path.push_back(_e);
    RouterT::flatten_path(path, _flat_name, _flat_config);
    collector.insert(_flat_name, this);
    if (RouterT *r = _e->resolved_router()) {
	ProcessingT subprocessing(r, processing->element_map());
	subprocessing.create(_processing_code, true);
	prepare_router(d, r, &subprocessing, collector, path, z_index);
    }
    path.pop_back();
}

void delt::prepare_router(wdiagram *d, RouterT *router, ProcessingT *processing,
			  HashMap<String, delt *> &collector,
			  Vector<ElementT *> &path, int &z_index)
{
    for (RouterT::iterator i = router->begin_elements();
	 i != router->end_elements(); ++i) {
	delt *e = new delt(this, z_index);
	z_index++;
	_elt.push_back(e);
	e->prepare(d, i.operator->(), processing, collector, path, z_index);
    }

    for (RouterT::conn_iterator i = router->begin_connections();
	 i != router->end_connections(); ++i) {
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

double delt::shadow(int side) const
{
    if (_des->shadow_style == 0
	|| (_des->shadow_style == 1 && (side == 0 || side == 3)))
	return 0;
    else
	return _des->shadow_width;
}

double delt::min_width() const
{
    return 18;			// XXX
}

double delt::min_height() const
{
    return 18;			// XXX
}

void delt::position_contents_first_heuristic(RouterT *router)
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
	i[1] += i[0] + std::max(_des->margin[0], _des->margin[2]);
    col_width[0] = col_width[0] / 2;
    for (std::vector<double>::iterator i = col_width.begin(); i + 1 < col_width.end(); ++i)
	i[1] += i[0] + std::max(_des->margin[1], _des->margin[3]);

    for (std::vector<delt *>::iterator ei = _elt.begin(); ei != _elt.end(); ++ei) {
	(*ei)->_xrect._x = col_width[(*ei)->_rowpos] - (*ei)->_width / 2;
	if ((*ei)->_row > 0)
	    (*ei)->_xrect._y = row_height[(*ei)->_row - 1] + std::max((*ei)->_des->margin[0], (*ei)->_des->margin[2]);
	else
	    (*ei)->_xrect._y = 0;
    }
}

void delt::position_contents_dot(RouterT *router, dcss_set *dcs, ErrorHandler *errh)
{
    ref_ptr<delt_style> gdes = dcs->elt_style(this);
    double gxsep = std::max(gdes->margin[1], gdes->margin[3]);
    double gysep = std::max(gdes->margin[0], gdes->margin[2]);
    double txsep = gdes->margin[1] + gdes->margin[3];
    double tysep = gdes->margin[0] + gdes->margin[2];
    
    StringAccum sa;
    sa << "digraph {\n"
       << "nodesep=" << (gxsep / 100) << ";\n"
       << "ranksep=" << (gysep / 100) << ";\n"
       << "node [shape=record]\n";
    for (std::vector<delt *>::size_type i = 0; i < _elt.size(); ++i) {
	delt *e = _elt[i];
	double w = e->width() + (e->_des->margin[1] + e->_des->margin[3] - txsep);
	double h = e->height() + (e->_des->margin[0] + e->_des->margin[2] - tysep);
	sa << "n" << i << " [width=" << (w/100) << ",height="
	   << (h/100) << ",fixedsize=true,label=\"{{";
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
	delt *e = _elt[eindex];
	e->_xrect._x = x * 100. / 72 - e->_width / 2;
	if (e->_des->margin[1] != e->_des->margin[3])
	    e->_xrect._x -= e->_des->margin[1] - e->_des->margin[3];
	e->_xrect._y = -y * 100. / 72 - e->_height / 2;
	if (e->_des->margin[0] != e->_des->margin[2])
	    e->_xrect._y -= e->_des->margin[2] - e->_des->margin[0];
	goto skip_to_semicolon;
    }

    double min_x = 1000000, min_y = 1000000;
    for (std::vector<delt *>::iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->_visible) {
	    min_x = MIN(min_x, (*n)->_xrect._x - (*n)->_des->margin[3]);
	    min_y = MIN(min_y, (*n)->_xrect._y - (*n)->_des->margin[0]);
	}

    for (std::vector<delt *>::iterator n = _elt.begin();
	 n != _elt.end(); ++n)
	if ((*n)->_visible) {
	    (*n)->_xrect._x -= min_x;
	    (*n)->_xrect._y -= min_y;
	}
}

void delt::layout_contents(dcontext &dcx, RouterT *router)
{
    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	(*ci)->layout(dcx);
	(*ci)->_row = -1;
    }

    position_contents_dot(router, dcx.d->css_set(), dcx.d->main()->error_handler());
    //position_contents_scc(router);
    //position_contents_first_heuristic(router);

    _contents_width = _contents_height = 0;
    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci) {
	_contents_width = MAX(_contents_width, (*ci)->_xrect._x + (*ci)->_width + (*ci)->_des->margin[1]);
	_contents_height = MAX(_contents_height, (*ci)->_xrect._y + (*ci)->_height + (*ci)->_des->margin[2]);
    }
}

void delt::layout_ports(dcss_set *dcs)
{
    delete[] _portoff[0];
    delete[] _portoff[1];
    _portoff[0] = _portoff[1] = 0;
    
    for (int isoutput = 0; isoutput < 2; ++isoutput) {
	_ports_length[isoutput] = 2 * _des->ports_padding;
	if (!_e->nports(isoutput))
	    continue;
	ref_ptr<dport_style> dps = dcs->port_style(this, isoutput, 0, 0);
	if (_e->nports(isoutput) > 1)
	    _portoff[isoutput] = new double[_e->nports(isoutput) + 1];
	double tm = 0;
	for (int p = 0; p < _e->nports(isoutput); ++p) {
	    if (!dps->uniform_style && p)
		dps = dcs->port_style(this, isoutput, p, 0);
	    if (dps->shape == dpshape_triangle)
		_ports_length[isoutput] += dps->length - 2;
	    else
		_ports_length[isoutput] += dps->length + 4;
	    double old_tm = tm;
	    tm += dps->margin[_orientation];
	    if (_e->nports(isoutput) > 1)
		_portoff[isoutput][p] = tm;
	    tm += dps->margin[_orientation ^ 2];
	    if (old_tm + 0.1 > tm)
		tm = old_tm + 0.1;
	}
	if (_e->nports(isoutput) > 1)
	    _portoff[isoutput][_e->nports(isoutput)] = tm;
    }
}

static void append_markup_quote(StringAccum &sa, const String &str,
				int precision)
{
    const char *s = str.begin();
    while (s != str.end() && isspace((unsigned char) *s))
	++s;
    const char *last = s;
    for (; s != str.end() && precision != 0; ++s) {
	if (precision > 0)
	    // XXX UTF-8
	    --precision;
	if (*s == '<' || *s == '>') {
	    sa.append(last, s);
	    sa << (*s == '<' ? "&lt;" : "&gt;");
	    last = s + 1;
	}
    }
    sa.append(last, s);
    if (s != str.end())
	sa.append("...", 3);
}

void delt::restyle(dcontext &dcx)
{
    StringAccum sa;
    const char *last = _des->text.begin();
    for (const char *s = _des->text.begin(); s != _des->text.end(); ++s)
	if (*s == '%' && s + 1 != _des->text.end()) {
	    sa.append(last, s);

	    int width = -1, precision = -1, altflag = 0;
	    const char *pct = s; 
	    for (++s; s != _des->text.end(); ++s)
		if (isdigit((unsigned char) *s)) {
		    if (precision >= 0)
			precision = 10 * precision + *s - '0';
		    else
			width = 10 * width + *s - '0';
		} else if (*s == '.')
		    precision = 0;
		else if (*s == '#')
		    altflag = 1;
		else
		    break;

	    if (s == _des->text.end()) {
	      invalid_format:
		last = s = pct;
		continue;
	    }
	    
	    if (*s == 'n')
		append_markup_quote(sa, name(), precision);
	    else if (*s == 'c')
		append_markup_quote(sa, type_name(), precision);
	    else if (*s == 'f')
		append_markup_quote(sa, flat_name(), precision);
	    else if (*s == 'C') {
		if (!altflag)
		    append_markup_quote(sa, _e->configuration(), precision);
		else if (_e->configuration()) {
		    sa << '(';
		    append_markup_quote(sa, _e->configuration(), precision);
		    sa << ')';
		}
	    } else
		goto invalid_format;
	    
	    last = s + 1;
	}
    sa.append(last, _des->text.end());
    _markup = sa.take_string();
    
    pango_layout_set_width(dcx, -1);
    if (_des->font != dcx.pl_font)
	dcx.set_font_description(_des->font);
    pango_layout_set_markup(dcx, _markup.data(), _markup.length());
    PangoRectangle rect;
    pango_layout_get_pixel_extents(dcx, NULL, &rect);
    _markup_width = rect.width;
    _markup_height = rect.height;

    _generation = dcx.generation;
}

void delt::layout(dcontext &dcx)
{
    if (_layout)
	return;
    _layout = true;
    _visible = _des->display != dedisp_none && !_e->tunnel();
    _orientation = _des->orientation;
    if (!_visible) {
	_width = _height = 0;
	return;
    }

    // get text extents
    restyle(dcx);

    // get contents width and height
    if (_expanded && _elt.size() && _des->display == dedisp_open)
	layout_contents(dcx, _e->resolved_router());
    
    // get element width and height
    double xwidth, xheight;

    if (_des->style == destyle_queue
	&& _contents_height == 0 && side_vertical(_orientation)) {
	xwidth = _markup_height;
	xheight = _markup_width + _des->padding[0] + _des->padding[2] + _des->orientation_padding;
    } else {
	xwidth = MAX(_markup_width, _contents_width);
	xheight = _markup_height + _des->padding[0] + _des->padding[2];
	if (_contents_height)
	    xheight += _contents_height;
    }
    xwidth = MAX(xwidth, _des->min_width);
    xheight = MAX(xheight, _des->min_height);

    // analyze port positions
    layout_ports(dcx.d->css_set());
    double xportlen = MAX(_ports_length[0], _ports_length[1]);
    
    if (_orientation == 0)
	_width = ceil(MAX(xwidth + _des->padding[1] + _des->padding[3],
			  xportlen));
    else {
	_width = ceil(xwidth + _des->padding[1] + _des->padding[3]);
	xheight = MAX(xheight, xportlen);
    }

    if (xheight > _des->min_height && _des->height_step > 0)
	xheight = _des->min_height + ceil((xheight - _des->min_height) / _des->height_step) * _des->height_step;
    _height = xheight;

    // adjust by border width and fix to integer boundaries
    _width = ceil(_width + 2 * _des->border_width);
    _height = ceil(_height + 2 * _des->border_width);

    if (_des->shadow_style != dshadow_none)
	dcx.d->notify_shadow(_des->shadow_width);
}

void delt::layout_compound_ports(dcss_set *dcs)
{
    if (_elt.size() > 0 && _elt[0]->_e->name() == "input") {
	_elt[0]->_x = _x;
	_elt[0]->_y = _y - 10;
	_elt[0]->_width = _width;
	ref_ptr<dport_style> dps = dcs->port_style(this, false, 0, 0);
	_elt[0]->_height = 10 + dps->width - 1;
	_elt[0]->_des = dcs->elt_style(_elt[0]);
	_elt[0]->_visible = _elt[0]->_layout = true;
	_elt[0]->layout_ports(dcs);
    }
    if (_elt.size() > 1 && _elt[1]->_e->name() == "output") {
	_elt[1]->_x = _x;
	ref_ptr<dport_style> dps = dcs->port_style(this, true, 0, 0);
	_elt[1]->_y = _y + _height - dps->width + 1;
	_elt[1]->_width = _width;
	_elt[1]->_height = 10;
	_elt[1]->_des = dcs->elt_style(_elt[1]);
	_elt[1]->_visible = _elt[1]->_layout = true;
	_elt[1]->layout_ports(dcs);
    }
}

void delt::layout_complete(dcontext &dcx, double dx, double dy)
{
    if (_e) {
	dx += _des->padding[3];
	dy += _markup_height + _des->padding[0];
    }

    for (std::vector<delt *>::iterator ci = _elt.begin();
	 ci != _elt.end(); ++ci)
	if ((*ci)->_visible) {
	    (*ci)->_x = floor((*ci)->_xrect._x + dx);
	    (*ci)->_y = floor((*ci)->_xrect._y + dy);
	    dcx.d->rects().insert(*ci);
	    if ((*ci)->_elt.size() && (*ci)->_des->display != dedisp_closed)
		(*ci)->layout_complete(dcx, (*ci)->_xrect._x + dx,
				       (*ci)->_xrect._y + dy);
	}

    if (_e && _parent && _elt.size())
	layout_compound_ports(dcx.d->css_set());

    for (std::vector<dconn *>::iterator ci = _conn.begin();
	 ci != _conn.end(); ++ci)
	if ((*ci)->layout())
	    dcx.d->rects().insert(*ci);
}

void delt::layout_main(dcontext &dcx, RouterT *router)
{
    _des = dcx.d->css_set()->elt_style(this);
    dcx.d->rects().clear();
    layout_contents(dcx, router);
    layout_complete(dcx, _des->margin[3], _des->margin[0]);
    assign(0, 0, _contents_width + _des->margin[1] + _des->margin[3],
	   _contents_height + _des->margin[0] + _des->margin[2]);
}

bool dconn::layout()
{
    if (!_from_elt->visible() || !_to_elt->visible())
	return false;
    point op = _from_elt->output_position(_from_port, 0);
    point ip = _to_elt->input_position(_to_port, 0);
    if (_from_elt->orientation() == 0 && _to_elt->orientation() == 0) {
	_x = MIN(op.x(), ip.x() - 3);
	_y = MIN(op.y(), ip.y() - 12);
	_width = MAX(op.x(), ip.x() + 3) - _x;
	_height = MAX(op.y() + 5, ip.y()) - _y;
    } else {
	_x = MIN(op.x(), ip.x() - 12);
	_y = MIN(op.y() - 2, ip.y() - 12);
	_width = MAX(op.x() + 5, ip.x() + 3) - _x;
	_height = MAX(op.y() + 5, ip.y() + 3) - _y;
    }
    return true;
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
	expand(_des->margin[0], _des->margin[1],
	       _des->margin[2], _des->margin[3]);
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

void delt::remove(rect_search<dwidget> &rects, rectangle &bounds)
{
    bounds |= *this;
    rects.remove(this);
    
    Vector<int> conn;
    _e->router()->find_connections_to(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	bounds |= *_parent->_conn[*iter];
	rects.remove(_parent->_conn[*iter]);
    }

    _e->router()->find_connections_from(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter) {
	bounds |= *_parent->_conn[*iter];
	rects.remove(_parent->_conn[*iter]);
    }

    if (_parent && _elt.size()) {
	_elt[0]->remove(rects, bounds);
	_elt[1]->remove(rects, bounds);
    }
}

void delt::insert(rect_search<dwidget> &rects,
		  dcss_set *dcs, rectangle &bounds)
{
    bounds |= *this;
    rects.insert(this);

    Vector<int> conn;
    _e->router()->find_connections_to(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter)
	if (_parent->_conn[*iter]->layout()) {
	    bounds |= *_parent->_conn[*iter];
	    rects.insert(_parent->_conn[*iter]);
	}

    _e->router()->find_connections_from(_e, conn);
    for (Vector<int>::iterator iter = conn.begin(); iter != conn.end(); ++iter)
	if (_parent->_conn[*iter]->layout()) {
	    bounds |= *_parent->_conn[*iter];
	    rects.insert(_parent->_conn[*iter]);
	}

    if (_parent && _elt.size()) {
	layout_compound_ports(dcs);
	_elt[0]->insert(rects, dcs, bounds);
	_elt[1]->insert(rects, dcs, bounds);
    }
}

/*****
 *
 * drawing
 *
 */

double delt::hard_port_position(bool isoutput, int port,
				double side_length) const
{
    assert(_e->nports(isoutput) > 1);
    
    double offset0 = _des->ports_padding;
    if (2 * offset0 > side_length) {
	offset0 = side_length / 2;
	side_length = 0;
    } else
	side_length -= 2 * offset0;

    return offset0 + side_length * _portoff[isoutput][port]
		 / _portoff[isoutput][_e->nports(isoutput)];
}

point delt::input_position(int port, dport_style *dps) const
{
    double off = port_position(false, port, side_length(_orientation ^ 1));

    double pos = side(_orientation);
    if (dps) {
	double dd = (_des->style == destyle_queue ? 0 : _des->border_width);
	if (dps->shape == dpshape_triangle)
	    dd = std::max(dd - dps->border_width / 2, dps->border_width / 2);
	pos += (side_greater(_orientation ^ 2) ? dd : -dd);
    }
    
    if (side_horizontal(_orientation))
	return point(pos, _y + off);
    else
	return point(_x + off, pos);
}

point delt::output_position(int port, dport_style *dps) const
{
    double off = port_position(true, port, side_length(_orientation ^ 1));
    
    double pos = side(_orientation ^ 2);
    if (dps) {
	double dd = _des->border_width;
	if (dps->shape == dpshape_triangle)
	    dd = std::max(dd - dps->border_width / 2, dps->border_width / 2);
	pos += (side_greater(_orientation ^ 2) ? -dd : dd);
    }
    
    if (side_horizontal(_orientation))
	return point(pos, _y + off);
    else
	return point(_x + off, pos);
}

static inline void cairo_set_border(cairo_t *cr, const double *color,
				    int style, double width)
{
    if (color)
	cairo_set_source_rgba(cr, color[0], color[1], color[2], color[3]);
    double dash[2];
    if (style == dborder_dashed) {
	dash[0] = 4 * width;
	dash[1] = 3 * width;
	cairo_set_dash(cr, dash, 2, 0);
    } else if (style == dborder_dotted) {
	if (width <= 2) {
	    dash[0] = 0.5 * width;
	    dash[1] = 1.5 * width;
	} else
	    dash[0] = dash[1] = width;
	cairo_set_dash(cr, dash, 2, 0);
    } else
	cairo_set_dash(cr, 0, 0, 0);
    cairo_set_line_width(cr, width);
}

void delt::draw_port(dcontext &dcx, dport_style *dps, point p, double shift,
		     bool isoutput)
{
    // align position
    if (dcx.scale_step == 0 && _aligned) {
	double dd = (dps->shape == dpshape_triangle ? 0.5 : 0);
	if (side_vertical(_orientation))
	    p._x = round(p._x - dd) + dd;
	else
	    p._y = round(p._y - dd) + dd;
    }

    cairo_matrix_t original_matrix;
    cairo_get_matrix(dcx, &original_matrix);
    cairo_translate(dcx, p.x() + shift, p.y() + shift);
    int port_orientation = _orientation ^ (isoutput ? 2 : 0);
    if (port_orientation)
	cairo_rotate(dcx, port_orientation * M_PI_2);

    for (int i = 0; i < 4; i++) {
	if ((i > 0 && dps->border_style == dborder_none)
	    || (i > 1 && dps->border_style != dborder_inset))
	    break;

	const double *color;
	if (i == 0 && dps->border_style == dborder_inset)
	    color = white_color;
	else if (i == 0 || i == 2)
	    color = dps->color;
	else
	    color = dps->border_color;
	if (color[3] == 0)
	    continue;

	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);

	double offset;
	if (i > 1)
	    offset = (dps->shape == dpshape_triangle ? 2 * M_SQRT2 : 2)
		* dps->border_width;
	else
	    offset = 0;

	if (i == 1)
	    cairo_set_border(dcx, 0, dps->border_style, dps->border_width);
	else if (i == 3)
	    cairo_set_line_width(dcx, 0.75 * dps->border_width);

	if (dps->shape == dpshape_triangle) {
	    cairo_move_to(dcx, -dps->length / 2 + offset, 0);
	    cairo_line_to(dcx, 0, dps->width - 1.15 * offset);
	    cairo_line_to(dcx, dps->length / 2 - offset, 0);
	    if (i < 3)
		cairo_close_path(dcx);
	} else {
	    cairo_move_to(dcx, -dps->length / 2 + offset, 0);
	    cairo_line_to(dcx, -dps->length / 2 + offset, dps->width - offset);
	    cairo_line_to(dcx, dps->length / 2 - offset, dps->width - offset);
	    cairo_line_to(dcx, dps->length / 2 - offset, 0);
	    if (!(i & 1))
		cairo_close_path(dcx);
	}
	
	if (i & 1)
	    cairo_stroke(dcx);
	else
	    cairo_fill(dcx);
    }

    cairo_set_matrix(dcx, &original_matrix);
}

void delt::draw_ports(dcontext &dcx, double shift)
{
    const char *pcpos = _processing_code.begin();
    int pcode;
    ref_ptr<dport_style> dps;
    
    for (int i = 0; i < _e->ninputs(); i++) {
	pcpos = ProcessingT::processing_code_next
	    (pcpos, _processing_code.end(), pcode);
	dps = dcx.d->css_set()->port_style(this, false, i, pcode);
	draw_port(dcx, dps.get(), input_position(i, dps.get()), shift, false);
    }
    pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
    for (int i = 0; i < _e->noutputs(); i++) {
	pcpos = ProcessingT::processing_code_next
	    (pcpos, _processing_code.end(), pcode);
	dps = dcx.d->css_set()->port_style(this, true, i, pcode);
	draw_port(dcx, dps.get(), output_position(i, dps.get()), shift, true);
    }
}

void delt::draw_background(dcontext &dcx, double shift)
{
    double pos[4];
    double bwd = floor(_des->border_width / 2);

    // figure out positions
    pos[3] = _x + shift + bwd;
    pos[0] = _y + shift + bwd;
    pos[1] = _x + shift + _width - bwd;
    pos[2] = _y + shift + _height - bwd;
    if (_des->style == destyle_queue)
	pos[_orientation] = side(_orientation) + shift;
    
    // background
    if (_des->background_color[3]) {
	const double *color = _des->background_color;
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	cairo_move_to(dcx, pos[3], pos[0]);
	cairo_line_to(dcx, pos[1], pos[0]);
	cairo_line_to(dcx, pos[1], pos[2]);
	cairo_line_to(dcx, pos[3], pos[2]);
	cairo_close_path(dcx);
	cairo_fill(dcx);
    }

    // fullness
    if ((_style & esflag_fullness) && _hvalue_fullness >= 0) {
	double xpos = pos[_orientation];
	pos[_orientation] = fma(_hvalue_fullness, pos[_orientation] - pos[_orientation ^ 2], pos[_orientation ^ 2]);
	cairo_set_source_rgba(dcx, 0, 0, 1.0, 0.2);
	cairo_move_to(dcx, pos[1], pos[2]);
	cairo_line_to(dcx, pos[3], pos[2]);
	cairo_line_to(dcx, pos[3], pos[0]);
	cairo_line_to(dcx, pos[1], pos[0]);
	cairo_close_path(dcx);
	cairo_fill(dcx);
	_drawn_fullness = _hvalue_fullness;
	pos[_orientation] = xpos;
    }

    // queue lines
    if (_des->style == destyle_queue) {
	cairo_set_border(dcx, _dqs->queue_stripe_color, _dqs->queue_stripe_style, _dqs->queue_stripe_width);
	int o = _orientation;
	double qls = _dqs->queue_stripe_spacing;
	int num_lines = (int) ((side_length(o) - _des->padding[o]) / qls);
	double xpos = pos[(o + 2) & 3] + 0.5;
	if (!side_greater(o))
	    qls = -qls, xpos -= 1;
	for (int i = 1; i <= num_lines; ++i) {
	    pos[(o + 2) & 3] = pos[o] = xpos + round(i * qls);
	    cairo_move_to(dcx, pos[((o + 3) & 2) + 1], pos[(o + 3) & 2]);
	    cairo_line_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 1) & 2]);
	}
	cairo_stroke(dcx);
    }
}

void delt::clip_to_border(dcontext &dcx, double shift) const
{
    double x1 = _x + shift, y1 = _y + shift;
    double x2 = _x + _width + shift, y2 = _y + _height + shift;
    cairo_user_to_device(dcx, &x1, &y1);
    cairo_user_to_device(dcx, &x2, &y2);
    x1 = floor(x1);
    y1 = floor(y1);
    x2 = ceil(x2) - 1;
    y2 = ceil(y2) - 1;
    cairo_device_to_user(dcx, &x1, &y1);
    cairo_device_to_user(dcx, &x2, &y2);
    cairo_rectangle(dcx, x1, y1, x2 - x1, y2 - y1);
    cairo_clip(dcx);
}

void delt::draw_text(dcontext &dcx, double shift)
{
    // text
    const double *color = _des->color;
    cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
    pango_layout_set_wrap(dcx, PANGO_WRAP_CHAR);
    pango_layout_set_alignment(dcx, PANGO_ALIGN_CENTER);
    if (dcx.pl_font != _des->font)
	dcx.set_font_description(_des->font);

    double space[2];
    space[0] = space[1] = 2 * _des->border_width;
    space[_orientation & 1] += 2 * _des->orientation_padding;
    bool saved = false;

    if (_width - space[1] < _markup_width
	&& _height - space[0] > _markup_width
	&& !_elt.size()) {
	// vertical layout
	saved = true;
	cairo_save(dcx);
	if (_markup_height > _width - 2)
	    clip_to_border(dcx, shift);
	
	cairo_translate(dcx, _x + shift, _y + shift + _height);
	cairo_rotate(dcx, -M_PI / 2);

	pango_layout_set_width(dcx, (int) ((_height - space[0]) * PANGO_SCALE));
	pango_layout_set_markup(dcx, _markup.data(), _markup.length());
	double dy = MAX((_width - _markup_height) / 2, 0);
	cairo_move_to(dcx, space[0] / 2, dy);
	pango_cairo_show_layout(dcx, dcx);

    } else {
	// normal horizontal layout, no text wrapping
	if (_markup_width > _width - space[1]
	    || _markup_height > _height - space[0]) {
	    saved = true;
	    cairo_save(dcx);
	    clip_to_border(dcx, shift);
	}

	pango_layout_set_width(dcx, (int) ((_width - space[1]) * PANGO_SCALE));
	pango_layout_set_markup(dcx, _markup.data(), _markup.length());

	double name_width, name_height;
	if (_width - space[1] >= _markup_width)
	    name_width = _markup_width, name_height = _markup_height;
	else {
	    PangoRectangle rect;
	    pango_layout_get_pixel_extents(dcx, NULL, &rect);
	    name_width = _width - space[1], name_height = rect.height;
	}

	double dy = MAX((_height - name_height - _contents_height) / 2, 1);
	cairo_move_to(dcx, _x + space[1] / 2 + shift, _y + dy + shift);
	pango_cairo_show_layout(dcx, dcx);
    }

    if (saved)
	cairo_restore(dcx);
}

void delt::draw_outline(dcontext &dcx, double shift)
{
    // shadow
    if (_des->shadow_style != dshadow_none) {
	const double *color = _des->shadow_color;
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	cairo_set_dash(dcx, 0, 0, 0);
	double sw = _des->shadow_width;
	if (_des->shadow_style == dshadow_drop) {
	    cairo_set_line_width(dcx, sw - shift);
	    cairo_move_to(dcx, _x + sw, _y + _height + (sw + shift) / 2);
	    cairo_line_to(dcx, _x + _width + (sw + shift) / 2, _y + _height + (sw + shift) / 2);
	    cairo_line_to(dcx, _x + _width + (sw + shift) / 2, _y + sw);
	    cairo_stroke(dcx);
	} else {
	    cairo_set_line_width(dcx, sw);
	    double x = _x + shift - sw / 2, y = _y + shift - sw / 2;
	    cairo_move_to(dcx, x, y);
	    cairo_line_to(dcx, x + _width + sw, y);
	    cairo_line_to(dcx, x + _width + sw, y + _height + sw);
	    cairo_line_to(dcx, x, y + _height + sw);
	    cairo_close_path(dcx);
	    cairo_stroke(dcx);
	}
    }

    // outline
    if (_des->border_style != dborder_none) {
	cairo_set_border(dcx, _des->border_color, _des->border_style,
			 _des->border_width);
	double pos[4];
	double bwd = _des->border_width / 2;
	pos[3] = _x + shift + bwd;
	pos[0] = _y + shift + bwd;
	pos[1] = _x + shift + _width - bwd;
	pos[2] = _y + shift + _height - bwd;
	int o = _orientation;
	if (_des->style == destyle_queue)
	    pos[o] = side(o) + shift;
	cairo_move_to(dcx, pos[((o + 3) & 2) + 1], pos[o & 2]);
	cairo_line_to(dcx, pos[((o + 2) & 2) + 1], pos[(o + 3) & 2]);
	cairo_line_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 2) & 2]);
	cairo_line_to(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2]);
	if (_des->style != destyle_queue)
	    cairo_close_path(dcx);
	cairo_stroke(dcx);
    }
}

void delt::draw(dcontext &dcx)
{
    if (_visible) {
	if (_highlight != _drawn_highlight || dcx.generation != _generation) {
	    ref_ptr<delt_style> old_des = _des;
	    _des = dcx.d->css_set()->elt_style(this);
	    if (_des->style == destyle_queue)
		_dqs = dcx.d->css_set()->queue_style(this);
	    if (dcx.generation != _generation || old_des->text != _des->text)
		restyle(dcx);
	    _drawn_highlight = _highlight;
	}
	double shift = (_highlight & (1 << dhlt_pressed) ? 1 : 0);
	draw_background(dcx, shift);
	draw_text(dcx, shift);
	draw_ports(dcx, shift);
	draw_outline(dcx, shift);
    }
}

void dconn::draw(dcontext &dcx)
{
    cairo_set_source_rgb(dcx, 0, 0, 0);
    cairo_set_line_width(dcx, 1);
    cairo_set_dash(dcx, 0, 0, 0);

    point op = _from_elt->output_position(_from_port, false);
    point ip = _to_elt->input_position(_to_port, false);
    
    if (_from_elt->vertical())
	cairo_move_to(dcx, op.x(), op.y() - 0.5);
    else
	cairo_move_to(dcx, op.x() - 0.5, op.y());
    
    if ((_from_elt->vertical() && _to_elt->vertical()
	 && fabs(ip.x() - op.x()) <= 6)
	|| (!_from_elt->vertical() && !_to_elt->vertical()
	    && fabs(ip.y() - op.y()) <= 6))
	/* no curves */;
    else {
	point curvea;
	if (_from_elt->vertical()) {
	    cairo_line_to(dcx, op.x(), op.y() + 3);
	    curvea = point(op.x(), op.y() + 10);
	} else {
	    cairo_line_to(dcx, op.x() + 3, op.y());
	    curvea = point(op.x() + 10, op.y());
	}
	if (_to_elt->vertical())
	    cairo_curve_to(dcx, curvea.x(), curvea.y(),
			   ip.x(), ip.y() - 12, ip.x(), ip.y() - 7);
	else
	    cairo_curve_to(dcx, curvea.x(), curvea.y(),
			   ip.x() - 12, ip.y(), ip.x() - 7, ip.y());
    }
    cairo_line_to(dcx, ip.x(), ip.y());
    cairo_stroke(dcx);

    if (_to_elt->vertical()) {
	cairo_move_to(dcx, ip.x(), ip.y() + 0.25);
	cairo_line_to(dcx, ip.x() - 3, ip.y() - 5.75);
	cairo_line_to(dcx, ip.x(), ip.y() - 3.75);
	cairo_line_to(dcx, ip.x() + 3, ip.y() - 5.75);
    } else {
	cairo_move_to(dcx, ip.x() + 0.25, ip.y());
	cairo_line_to(dcx, ip.x() - 5.75, ip.y() - 3);
	cairo_line_to(dcx, ip.x() - 3.75, ip.y());
	cairo_line_to(dcx, ip.x() - 5.75, ip.y() + 3);
    }
    cairo_close_path(dcx);
    cairo_fill(dcx);
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
    insert(d->rects(), d->css_set(), bounds);
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
	&& _xrect._height - delta.y() >= min_height()) {
	_y = _xrect._y + delta.y();
	_height = _xrect._height - delta.y();
    } else if (vtype == deg_border_bot
	       && _xrect._height + delta.y() >= min_height())
	_height = _xrect._height + delta.y();
    
    if (htype == deg_border_lft
	&& _xrect._width - delta.x() >= min_width()) {
	_x = _xrect._x + delta.x();
	_width = _xrect._width - delta.x();
    } else if (htype == deg_border_rt
	       && _xrect._width + delta.x() >= min_width())
	_width = _xrect._width + delta.x();

    _aligned = false;
    insert(d->rects(), d->css_set(), bounds);
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

    double l, c;
    if (lv->have_hvalue() && cv->have_hvalue()
	&& cp_double(lv->hvalue(), &l) && cp_double(cv->hvalue(), &c)
	&& l >= 0 && l <= c)
	_hvalue_fullness = l / c;
    else
	_hvalue_fullness = -1;

    if ((_drawn_fullness < 0) != (_hvalue_fullness < 0)
	|| (fabs(_drawn_fullness - _hvalue_fullness)
	    * side_length(_orientation) * d->scale()) > 0.5)
	d->redraw(*this);
}

void delt::add_gadget(wdiagram *d, int gadget)
{
    assert(gadget == esflag_fullness);
    if (gadget == esflag_fullness && (_style & esflag_fullness) == 0) {
	_style |= esflag_fullness;
	_hvalue_fullness = _drawn_fullness = -1;
	gadget_fullness(d);
    }
}

void delt::remove_gadget(wdiagram *d, int gadget)
{
    assert(gadget == esflag_fullness);
    if (gadget == esflag_fullness && (_style & esflag_fullness) != 0) {
	_style &= ~esflag_fullness;
	_hvalue_fullness = _drawn_fullness = -1;
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
    if (!r.contains(window_x, window_y))
	return deg_none;
    //r.integer_align();
    
    double attach = MAX(2.0, d->scale() * _des->border_width);
    if (d->scale_step() > -5
	&& r.width() >= 18
	&& r.height() >= 18
	&& (window_x - r.x1() < attach
	    || window_y - r.y1() < attach
	    || r.x2() - window_x - 1 < attach
	    || r.y2() - window_y - 1 < attach)) {
	int gnum = deg_main;
	if (window_x - r.x1() < 10)
	    gnum += deg_border_lft;
	else if (r.x2() - window_x < 10)
	    gnum += deg_border_rt;
	if (window_y - r.y1() < 10)
	    gnum += deg_border_top;
	else if (r.y2() - window_y < 10)
	    gnum += deg_border_bot;
	return gnum;
    }

    return deg_main;
}

}
