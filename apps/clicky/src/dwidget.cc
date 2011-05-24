#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "dwidget.hh"
#include "wdiagram.hh"
#include "ddecor.hh"
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <clicktool/processingt.hh>
#include <clicktool/elementmap.hh>
#include "scopechain.hh"
#include <list>
#include <math.h>
#include <locale.h>
#include "crouter.hh"
#include "whandler.hh"
#include "transform.hh"
extern "C" {
#include "support.h"
}
namespace clicky {

static inline void cairo_rel_line_to_point(cairo_t *cr, const point &p)
{
    cairo_rel_line_to(cr, p.x(), p.y());
}

static inline void cairo_curve_to_points(cairo_t *cr, const point &p0, const point &p1, const point &p2)
{
    cairo_curve_to(cr, p0.x(), p0.y(), p1.x(), p1.y(), p2.x(), p2.y());
}

dcontext::dcontext(crouter *cr_, PangoLayout *pl_, cairo_t *cairo_,
		   unsigned generation_, int scale_step_, double scale_)
    : cr(cr_), pl(pl_), cairo(cairo_), generation(generation_),
      scale_step(scale_step_), scale(scale_), penumbra(1)
{
}

unsigned dcontext::step_generation()
{
    static unsigned the_generation;
    return ++the_generation;
}

void dcontext::set_font_description(const String &font)
{
    if (pl_font != font) {
	PangoFontDescription *font_desc = pango_font_description_from_string(font.c_str());
	pango_layout_set_font_description(pl, font_desc);
	pango_font_description_free(font_desc);
	pl_font = font;
    }
}


String dwidget::unparse() const
{
    if (_type == dw_elt)
	return static_cast<const delt *>(this)->flat_name();
    else {
	const dconn *c = static_cast<const dconn *>(this);
	StringAccum sa;
	sa << c->from_elt()->flat_name() << '[' << c->from_port()
	   << "]->[" << c->to_port() << ']' << c->to_elt()->flat_name();
	return sa.take_string();
    }
}


delt *delt::create(ElementT *e, delt *parent,
		   crouter *cr, ProcessingT *processing,
		   HashTable<String, delt *> *collector,
		   ScopeChain &chain)
{
    // see also create_split()
    delt *de = new delt(parent, parent->z_index() + 2);

    de->_e = e;
    de->_processing_code = processing->decorated_processing_code(e);
    de->_flow_code = processing->flow_code(e);

    de->_flat_name = chain.flat_name(e->name());
    de->_flat_config = chain.resolved_config(e->config());
    if (collector)
	(*collector)[de->_flat_name] = de;

    ElementClassT *resolved_type = chain.resolved_type(e);
    de->_primitive = resolved_type->primitive();
    de->_resolved_router = resolved_type->cast_router();

    // initial styles
    int x;
    de->_dess = cr->ccss()->elt_size_style(cr, de, &x);
    de->_dess_sensitivity = x;
    de->_des = cr->ccss()->elt_style(cr, de, &x);
    de->_des_sensitivity = x;

    return de;
}

delt *delt::create_split(crouter *cr, int split_type)
{
    delt *se = new delt(_parent, z_index() + 2);

    se->_e = _e;
    se->_processing_code = _processing_code;
    se->_flow_code = _flow_code;

    se->_flat_name = _flat_name;
    se->_flat_config = _flat_config;

    se->_primitive = _primitive;
    se->_resolved_router = _resolved_router;

    // initial styles
    int x;
    se->_dess = cr->ccss()->elt_size_style(cr, se, &x);
    se->_dess_sensitivity = x;
    se->_des = cr->ccss()->elt_style(cr, se, &x);
    se->_des_sensitivity = x;

    se->_split_type = split_type;
    if (!_split)
	_split = this;
    se->_split = _split;
    se->_flow_split = _flow_split;
    se->_port_split = _port_split;
    se->_split_copy = true;
    _split = se;

    return se;
}

delt::~delt()
{
    ddecor::free_list(_decor);
    while (_split && _split != this) {
	delt *s = _split;
	_split = s->_split;
	s->_split = 0;
	delete s;
    }
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

void delt::create_elements(crouter *cr, RouterT *router,
			   ProcessingT *processing,
			   HashTable<String, delt *> *collector,
			   ScopeChain &chain)
{
    assert(!_resolved_router || _resolved_router == router);
    assert(_elt.size() == 0);
    _resolved_router = router;

    // create elements (_elt[i]->eindex() == i)
    _elt.resize(router->nelements(), 0);
    for (int i = 0; i < router->nelements(); ++i)
	_elt[i] = delt::create(router->element(i), this,
			       cr, processing, collector, chain);

    // style elements
    for (int i = 0; i < router->nelements(); ++i)
	_elt[i]->reccss(cr, dsense_always);

    // create sub-elements for open or expanded compounds
    for (iterator e = begin_contents(); e; ++e)
	if (e->_resolved_router
	    && (e->_display == dedisp_normal || e->_display == dedisp_expanded)) {
	    chain.enter_element(e->_e);
	    ProcessingT subprocessing(*processing, e->_e);
	    e->create_elements(cr, subprocessing.router(), &subprocessing,
			       collector, chain);
	    chain.pop_element();
	}
}

int delt::assign_z_indexes(int z)
{
    set_z_index(z);
    z += 2;
    for (iterator e = begin_contents(); e; ++e)
	z = e->assign_z_indexes(z);
    return z;
}

void delt::create_connections(std::vector<delt_conn> &cc, crouter *cr) const
{
    bool input_output = (root() || _des->display != dedisp_expanded);

    // add local connections
    for (RouterT::conn_iterator it = _resolved_router->begin_connections();
	 it != _resolved_router->end_connections(); ++it)
	if (input_output || (it->from_eindex() != 0 && it->to_eindex() != 1))
	    cc.push_back(delt_conn(_elt[it->from_eindex()], it->from_port(),
				   _elt[it->to_eindex()], it->to_port()));

    // add expanded child connections
    for (iterator e = begin_contents(); e; ++e) {
	if (e->_elt.size() && e->_display == dedisp_expanded)
	    e->create_connections(cc, cr);
	else if (e->_elt.size() && e->_display == dedisp_normal)
	    e->create_connections(cr);
    }
}

void delt::create_connections(crouter *cr)
{
    for (std::vector<dconn *>::iterator it = _conn.begin();
	 it != _conn.end(); ++it)
	delete *it;
    _conn.clear();

    if (!root() && _des->display != dedisp_normal)
	return;

    // create initial connections
    std::vector<delt_conn> cc;
    create_connections(cc, cr);

    // expand connections
    Bitvector bv;
    while (cc.size()) {
	delt_conn c = cc.back();
	cc.pop_back();

	if (c.from->display() == dedisp_none
	    || c.to->display() == dedisp_none)
	    continue;

	if (c.from->display() == dedisp_expanded) {
	    RouterT *subr = c.from->_resolved_router;
	    for (RouterT::conn_iterator it = subr->find_connections_to(PortT(subr->element(1), c.from_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(c.from->_elt[it->from_eindex()],
				       it->from_port(), c.to, c.to_port));
	    continue;
	}

	if (c.to->display() == dedisp_expanded) {
	    RouterT *subr = c.to->_resolved_router;
	    for (RouterT::conn_iterator it = subr->find_connections_from(PortT(subr->element(0), c.to_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(c.from, c.from_port,
				       c.to->_elt[it->to_eindex()],
				       it->to_port()));
	    continue;
	}

	if (c.from->eindex() == 0 && !c.from->parent()->root()
	    && c.from->parent()->display() == dedisp_expanded) {
	    delt *pp = c.from->parent()->parent();
	    RouterT *subr = pp->_resolved_router;
	    for (RouterT::conn_iterator it = subr->find_connections_to(PortT(c.from->parent()->_e, c.from_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(pp->_elt[it->from_eindex()], it->from_port(), c.to, c.to_port));
	    continue;
	}

	if (c.to->eindex() == 1 && !c.to->parent()->root()
	    && c.to->parent()->display() == dedisp_expanded) {
	    delt *pp = c.to->parent()->parent();
	    RouterT *subr = pp->_resolved_router;
	    for (RouterT::conn_iterator it = subr->find_connections_from(PortT(c.to->parent()->_e, c.to_port));
		 it != subr->end_connections(); ++it)
		cc.push_back(delt_conn(c.from, c.from_port, pp->_elt[it->to_eindex()], it->to_port()));
	    continue;
	}

	if (c.from->display() != dedisp_passthrough
	    && c.to->display() == dedisp_passthrough) {
	    ProcessingT::forward_flow(c.to->_flow_code, c.to_port, &bv, c.to->noutputs());
	    RouterT *subr = c.to->_e->router();
	    for (int p = 0; p < c.to->noutputs(); ++p)
		if (bv[p]) {
		    for (RouterT::conn_iterator it = subr->find_connections_from(PortT(c.to->_e, p));
			 it != subr->end_connections(); ++it)
			cc.push_back(delt_conn(c.from, c.from_port,
					       c.to->parent()->_elt[it->to_eindex()],
					       it->to_port()));
		}
	    continue;
	}

	if (!dedisp_visible(c.from->display())
	    || !dedisp_visible(c.to->display()))
	    continue;

	if (dconn *conn = dconn::make(c.from, c.from_port, c.to, c.to_port))
	    _conn.push_back(conn);
    }
}


String delt::display_name() const
{
    delt *p = parent();
    // can't depend on p->_display being set yet
    if (!p->root() && p->_des->display == dedisp_expanded)
	return p->display_name() + "/" + name();
    else
	return name();
}


/*****
 *
 * Layout
 *
 */

double delt::shadow(double scale, int side) const
{
    if (_des->shadow_style == dshadow_none
	|| (_des->shadow_style == dshadow_drop && (side == 0 || side == 3)))
	return 0;
    else if (_des->shadow_style == dshadow_unscaled_outline)
	return _des->shadow_width / scale;
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

const char *cp_double(const char *begin, const char *end, double *result)
{
    const char *s = begin;
    if (s != end && (*s == '+' || *s == '-'))
	++s;
    if (s == end
	|| (*s == '.' && (s + 1 == end || !isdigit((unsigned char) s[1])))
	|| (*s != '.' && !isdigit((unsigned char) *s)))
	return begin;
    while (s != end && isdigit((unsigned char) *s))
	++s;
    if (s != end && *s == '.')
	++s;
    while (s != end && isdigit((unsigned char) *s))
	++s;
    if (s != end && (*s == 'e' || *s == 'E')) {
	const char *t = s + 1;
	if (t != end && (*t == '+' || *t == '-'))
	    ++t;
	while (t != end && isdigit((unsigned char) *t))
	    ++t;
	if (t != s + 1
	    && (t != s + 2 || isdigit((unsigned char) s[1])))
	    s = t;
    }
    *result = strtod(begin, 0);
    return s;
}

const char *delt::parse_connection_dot(delt *e1, const HashTable<int, delt *> &z_index_lookup, const char *s, const char *end)
{
    int eport, oz_index, oeport;
    delt *e2 = 0;
    Vector<point> route;

    if (s + 2 >= end || s[0] != ':' || s[1] != 'o' || !isdigit((unsigned char) s[2]))
	return s;
    s = cp_integer(s + 2, end, 10, &eport);
    if (s + 2 < end && s[0] == ':' && (s[1] == 's' || s[1] == 'e'))
	s += 2;
    s = cp_skip_space(s, end);
    if (s + 1 >= end || s[0] != '-' || s[1] != '>')
	return s;
    s = cp_skip_space(s + 2, end);
    if (s + 1 >= end || s[0] != 'n' || !isdigit((unsigned char) s[1]))
	return s;
    s = cp_integer(s + 1, end, 10, &oz_index);
    if (oz_index < 0 || !(e2 = z_index_lookup.get(oz_index)))
	return s;
    if (s + 2 >= end || s[0] != ':' || s[1] != 'i' || !isdigit((unsigned char) s[2]))
	return s;
    s = cp_integer(s + 2, end, 10, &oeport);
    if (s + 2 < end && s[0] == ':' && (s[1] == 'n' || s[1] == 'w'))
	s += 2;
    s = cp_skip_space(s, end);
    if (s >= end || s[0] != '[')
	return s;
    ++s;

  skip_to_p:
    while (s != end && *s != 'p' && *s != ';') {
	for (int quote = 0; s != end && (quote || (*s != ',' && *s != ']' && *s != ';')); ++s)
	    if (*s == '\"')
		quote = !quote;
	if (s != end)
	    s = cp_skip_space(s + 1, end);
    }
    if (s == end || *s == ';')
	return s;
    if (s + 2 >= end || s[1] != 'o' || s[2] != 's') {
	++s;
	goto skip_to_p;
    }
    s = cp_skip_space(s + 3, end);
    if (s == end || *s != '=')
	return s;
    s = cp_skip_space(s + 1, end);
    if (s != end && *s == '"')
	++s;
    s = cp_skip_space(s, end);

    const char *t;
    point origin(0, 0);
    int count = 0;
    while (s != end && *s != '"') {
	double x, y;
	int e = 0;
	if (s + 1 < end && *s == 'e' && s[1] == ',')
	    e = 1, s += 2;
	if ((t = cp_double(s, end, &x)) == s || t == end || *t != ',')
	    return t;
	s = t + 1;
	if ((t = cp_double(s, end, &y)) == s)
	    return t;
	s = cp_skip_space(t, end);
	point p(x * 100. / 72, -y * 100. / 72);
	if (e)
	    /* nada */;
	else if (++count <= 1)
	    origin = p;
	else
	    route.push_back(p - origin);
    }

    if (route.size() > 0 && (route.size() % 3) == 0) {
	for (std::vector<dconn *>::iterator ci = _conn.begin();
	     ci != _conn.end(); ++ci)
	    if ((*ci)->_elt[1] == e1 && (*ci)->_elt[0] == e2
		&& (*ci)->_port[1] == eport && (*ci)->_port[0] == oeport) {
		(*ci)->_route.swap(route);
		route.clear();
		break;
	    }
	if (route.size() != 0)
	    fprintf(stderr, "couldn't find connection %s[%d] -> [%d]%s\n", e1->name().c_str(), eport, oeport, e2->name().c_str());
    }

    return s;
}

int delt::flow_split_char(bool isoutput, int port) const
{
    int c = 0;
    if (_flow_split) {
	const char *s = _des->flow_split.begin(), *end = _des->flow_split.end();
	const char *slash = find(s, end, '/');
	assert(s < slash && slash < end);
	if (isoutput)
	    s = slash + 1;
	else
	    end = slash;
	c = (unsigned char) s[std::min(port, (int) (end - s - 1))] << 1;
    }
    if (_port_split && !isoutput)
	c += desplit_inputs;
    return c;
}

static void ports_dot(StringAccum &sa, int nports, char c)
{
    for (int p = 0; p < nports; ++p)
	 sa << (p ? "|<" : "<") << c << p << ">";
}

void delt::unparse_contents_dot(StringAccum &sa, crouter *cr, HashTable<int, delt *> &z_index_lookup) const
{
    delt fake_child(const_cast<delt *>(this), 0);
    ref_ptr<delt_size_style> gdess = cr->ccss()->elt_size_style(cr, &fake_child);
    double txsep = gdess->margin[1] + gdess->margin[3];
    double tysep = gdess->margin[0] + gdess->margin[2];

    for (iterator e = begin_contents(); e; ++e) {
	if (e->display() == dedisp_expanded) {
	    e->unparse_contents_dot(sa, cr, z_index_lookup);
	    continue;
	} else if (!e->visible())
	    continue;

	sa << "n" << e->z_index();
	assert(z_index_lookup.get(e->z_index()) == 0);
	z_index_lookup[e->z_index()] = e.operator->();

	double w = e->width() + (e->_dess->margin[1] + e->_dess->margin[3] - txsep);
	double h = e->height() + (e->_dess->margin[0] + e->_dess->margin[2] - tysep);
	sa << " [width=" << (w/100) << ",height=" << (h/100)
	   << ",fixedsize=true,label=\"{{";
	ports_dot(sa, e->ninputs(), 'i');
	sa << "}|" << e->display_name() << "|{";
	ports_dot(sa, e->noutputs(), 'o');
	sa << "}}\"];\n";

	e->_x = e->_y = 0;
    }
}

void delt::create_bbox_contents(double bbox[4], double mbbox[4], bool include_compound_ports) const
{
    size_t n = 0;
    for (iterator e = begin_contents(); e; ++e, ++n) {
	if (e->display() == dedisp_expanded)
	    e->create_bbox_contents(bbox, mbbox, false);
	else if (e->visible()
		 || (!root() && include_compound_ports && n < 2
		     && e->_e->nports(!n) != 0)) {
	    const double *m = e->_dess->margin;
	    bbox[0] = std::min(bbox[0], e->_y);
	    bbox[1] = std::max(bbox[1], e->_x + e->_width);
	    bbox[2] = std::max(bbox[2], e->_y + e->_height);
	    bbox[3] = std::min(bbox[3], e->_x);
	    mbbox[0] = std::min(mbbox[0], e->_y - m[0]);
	    mbbox[1] = std::max(mbbox[1], e->_x + e->_width + m[1]);
	    mbbox[2] = std::max(mbbox[2], e->_y + e->_height + m[2]);
	    mbbox[3] = std::min(mbbox[3], e->_x - m[3]);
	}
    }
}

void delt::shift_contents(double dx, double dy) const
{
    for (iterator e = begin_contents(); e; ++e) {
	if (e->display() == dedisp_expanded)
	    e->shift_contents(dx, dy);
	else if (e->visible()) {
	    e->_x += dx;
	    e->_y += dy;
	}
    }
}

void delt::position_contents_dot(crouter *cr, ErrorHandler *errh)
{
    delt fake_child(this, 0);
    ref_ptr<delt_size_style> gdess = cr->ccss()->elt_size_style(cr, &fake_child);
    double gxsep = std::max(gdess->margin[1], gdess->margin[3]);
    double gysep = std::max(gdess->margin[0], gdess->margin[2]);

    StringAccum sa;
    char *old_locale = setlocale(LC_ALL, "C");
    sa << "digraph {\n"
       << "nodesep=" << (gxsep / 100) << ";\n"
       << "ranksep=" << (gysep / 100) << ";\n"
       << "node [shape=record];\n"
       << "edge [arrowsize=0.2,headclip=true,tailclip=true];\n";
    if (_contents_width)
	sa << "size=\"" << (_contents_width / 100) << "," << (_contents_height / 100) << "\";\n"
	   << "ratio=compress;\n";
    switch (gdess->orientation) {
    case 1: sa << "rankdir=RL;\n"; break;
    case 2: sa << "rankdir=BT;\n"; break;
    case 3: sa << "rankdir=LR;\n"; break;
    }

    HashTable<int, delt *> z_index_lookup;

    if (!root()) {
	delt *ein = _elt[0], *eout = _elt[1];
	assert(ein && eout && ein->name() == "input" && !ein->visible()
	       && eout->name() == "output" && !eout->visible());
	if (_e->ninputs()) {
	    z_index_lookup[ein->z_index()] = ein;
	    sa << "{ rank=source; n" << ein->z_index() << " [";
	    if (_contents_width) {
		if (vertical())
		    sa << "width=" << (_contents_width / 100) << ",height=0,fixedsize=true,";
		else
		    sa << "height=" << (_contents_height / 100) << ",width=0,fixedsize=true,";
	    }
	    sa << "label=\"{{}|{";
	    ports_dot(sa, _e->ninputs(), 'o');
	    sa << "}}\"]; }\n";
	}
	if (_e->noutputs()) {
	    z_index_lookup[eout->z_index()] = eout;
	    sa << "{ rank=sink; n" << eout->z_index() << " [";
	    if (_contents_width) {
		if (vertical())
		    sa << "width=" << (_contents_width / 100) << ",height=0,fixedsize=true,";
		else
		    sa << "height=" << (_contents_height / 100) << ",width=0,fixedsize=true,";
	    }
	    sa << "label=\"{{";
	    ports_dot(sa, _e->noutputs(), 'i');
	    sa << "}|{}}\"]; }\n";
	}
    }

    unparse_contents_dot(sa, cr, z_index_lookup);

    for (std::vector<dconn *>::iterator ci = _conn.begin(); ci != _conn.end(); ++ci) {
	delt *eout = (*ci)->_elt[1], *ein = (*ci)->_elt[0];
	sa << 'n' << eout->z_index()
	   << ':' << 'o' << (*ci)->_port[1] << ':'
	   << (eout->vertical() ? 's' : 'e')
	   << " -> n" << ein->z_index()
	   << ':' << 'i' << (*ci)->_port[0] << ':'
	   << (ein->vertical() ? 'n' : 'w') << ";\n";
    }
    sa << "}\n";
    setlocale(LC_ALL, old_locale);

    //fprintf(stderr, "%s\n", sa.c_str());
    String result;
    {
	// run through dot twice, as it gives different/better results if you do this
	String dot1_res = shell_command_output_string("dot", sa.take_string(), errh);
	StringAccum outsa(shell_command_output_string("dot", dot1_res, errh));
	char *outs = outsa.begin(), *outend = outsa.end();
	for (char *s = outsa.begin(); s != outend; )
	    if (*s == '\\' && s + 1 < outend && s[1] == '\n')
		s += 2;
	    else if (*s == '\\' && s + 2 < outend && s[1] == '\r' && s[1] == '\n')
		s += 3;
	    else if (*s == '\\' && s + 1 < outend && s[1] == '\r')
		s += 2;
	    else
		*outs++ = *s++;
	outsa.adjust_length(outs - outsa.end());
	result = outsa.take_string();
    }
    //fprintf(stderr, "%s\n", result.c_str());
    //shell_command_output_string("cat > /tmp/x.dot", result, errh);
    //shell_command_output_string("dot -Tps > /tmp/x.ps", result, errh);

    const char *end = result.end();
    for (const char *s = result.begin(); s != end; ) {
	s = cp_skip_space(s, end);
	if (s + 1 >= end || *s != 'n' || !isdigit((unsigned char) s[1])) {
	    if (s != end && (*s == '{' || *s == '}')) {
		++s;
		continue;
	    }
	  skip_to_semicolon:
	    while (s != end && *s != ';')
		++s;
	    if (s != end)
		++s;
	    continue;
	}

	int z_index = 0;
	delt *e1;
	s = cp_integer(s + 1, end, 10, &z_index);
	while (s != end && isspace((unsigned char) *s))
	    ++s;
	if (z_index < 0 || !(e1 = z_index_lookup.get(z_index)))
	    goto skip_to_semicolon;

	if (s == end)
	    goto skip_to_semicolon;
	else if (*s == ':') {
	    s = parse_connection_dot(e1, z_index_lookup, s, end);
	    goto skip_to_semicolon;
	} else if (*s != '[')
	    goto skip_to_semicolon;

      skip_to_p:
	while (s != end && *s != 'p' && *s != ';') {
	    for (int quote = 0; s != end && (quote || (*s != ',' && *s != ']' && *s != ';')); ++s)
		if (*s == '\"')
		    quote = !quote;
	    if (s != end)
		s = cp_skip_space(s + 1, end);
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
	double x, y;
	const char *t;
	if ((t = cp_double(s, end, &x)) == s)
	    goto skip_to_semicolon;
	s = cp_skip_space(t, end);
	if (s == end || *s != ',')
	    goto skip_to_semicolon;
	s = cp_skip_space(s + 1, end);
	if ((t = cp_double(s, end, &y)) == s)
	    goto skip_to_semicolon;
	e1->_x = x * 100. / 72 - e1->_width / 2;
	if (e1->_dess->margin[1] != e1->_dess->margin[3])
	    e1->_x -= e1->_dess->margin[1] - e1->_dess->margin[3];
	e1->_y = -y * 100. / 72 - e1->_height / 2;
	if (e1->_dess->margin[0] != e1->_dess->margin[2])
	    e1->_y -= e1->_dess->margin[2] - e1->_dess->margin[0];
	goto skip_to_semicolon;
    }

    bool first_time = _contents_width == 0;

    double bbox[4], mbbox[4];
    bbox[0] = mbbox[0] = bbox[3] = mbbox[3] = 1000000;
    bbox[1] = mbbox[1] = bbox[2] = mbbox[2] = -1000000;
    create_bbox_contents(bbox, mbbox, first_time);

    if (!root() && !first_time)
	for (int i = 0; i < 4; ++i) {
	    double delta = fabs(bbox[i] - mbbox[i]) - _dess->padding[i];
	    if (delta > 0)
		bbox[i] += (rectangle::side_greater(i) ? delta : -delta);
	}

    // when laying out the root, consider connections as well as elements
    // to calculate the bounding box
    if (root())
	for (std::vector<dconn *>::iterator it = _conn.begin();
	     it != _conn.end(); ++it)
	    if ((*it)->layout()) {
		bbox[0] = std::min(bbox[0], (*it)->_y);
		bbox[1] = std::max(bbox[1], (*it)->_x + (*it)->_width);
		bbox[2] = std::max(bbox[2], (*it)->_y + (*it)->_height);
		bbox[3] = std::min(bbox[3], (*it)->_x);
		mbbox[0] = std::min(mbbox[0], (*it)->_y);
		mbbox[1] = std::max(mbbox[1], (*it)->_x + (*it)->_width);
		mbbox[2] = std::max(mbbox[2], (*it)->_y + (*it)->_height);
		mbbox[3] = std::min(mbbox[3], (*it)->_x);
	    }

    _contents_width = std::max(bbox[1] - bbox[3], 0.);
    _contents_height = std::max(bbox[2] - bbox[0], 0.);
    shift_contents(-bbox[3], -bbox[0]);

    if (!root() && first_time)
	position_contents_dot(cr, errh);
}


/*****
 *
 *
 *
 */

bool delt::reccss(crouter *cr, int change)
{
    int x;
    bool redraw = false;
    bool resplit = false;
    ref_ptr<delt_size_style> old_dess = _dess;
    ref_ptr<delt_style> old_des = _des;
    String old_markup = _markup;

    if (change & (_dess_sensitivity | dsense_always)) {
	_dess = cr->ccss()->elt_size_style(cr, this, &x);
	_dess_sensitivity = x;
	if (old_dess != _dess)
	    redraw = true;
    }

    if (change & (_des_sensitivity | dsense_always)) {
	_des = cr->ccss()->elt_style(cr, this, &x);
	_des_sensitivity = x;
	if (old_des != _des)
	    redraw = true;
    }

    if ((change & (_markup_sensitivity | dsense_always))
	|| (_des != old_des && _des->text != old_des->text)) {
	_markup = parse_markup(_des->text, cr, -1, &x);
	_markup_sensitivity = x;
    }

    if ((change & dsense_always) || _des->font != old_des->font
	|| _markup != old_markup) {
	_markup_width = _markup_height = -1024;
	redraw = true;
    }

    if ((change & dsense_always) || _des != old_des) {
	_display = _des->display;
	if (_port_split != _des->port_split
	    || _flow_split != (bool) _des->flow_split
	    || (_flow_split && old_des && old_des->flow_split != _des->flow_split))
	    resplit = true;
	if (_display == dedisp_expanded && primitive())
	    _display = dedisp_normal;
	if (dedisp_visible(_display)
	    && (_parent->root()		// NB _parent->_display is not yet set
		|| dedisp_children_visible(_parent->_des->display)))
	    _visible = true;
	else
	    _visible = false;
	// "input" and "output" are always invisible, but displayed
	if (_e->tunnel() && !_parent->root()
	    && (this == _parent->_elt[0] || this == _parent->_elt[1])) {
	    _visible = false;
	    _display = dedisp_normal;
	}
    }

    if ((!old_des || _des->decorations != old_des->decorations
	 || (!_decor && _des->decorations))
	&& _visible) {
	ddecor::free_list(_decor);
	String s = _des->decorations;
	while (String dname = cp_shift_spacevec(s))
	    if (String dtype = cr->ccss()->vstring("style", dname, cr, this)) {
		if (dtype == "fullness")
		    _decor = new dfullness_decor(dname, cr, this, _decor);
		else if (dtype == "activity")
		    _decor = new dactivity_decor(dname, cr, this, _decor);
	    }
    }

    // handle split changes
    if (resplit && !_split_copy && _visible) {
	// kill old split status
	while (_split && _split != this) {
	    delt *s = _split;
	    _split = s->_split;
	    s->_split = 0;
	    delete s;
	}
	_split_type = 0;
	_port_split = _des->port_split;
	_flow_split = (bool) _des->flow_split;

	if (_flow_split) {
	    // collect split characters
	    Bitvector inputs(256), outputs(256), *current = &inputs;
	    Vector<int> split_chars;
	    int portno = 0, max_ports = ninputs();
	    for (const char *s = _des->flow_split.begin();
		 s != _des->flow_split.end(); ++s)
		if (*s == '/')
		    portno = 0, max_ports = noutputs(), current = &outputs;
		else if (portno++ >= max_ports)
		    /* do nothing */;
		else if (!(*current)[(unsigned char) *s]) {
		    if (!inputs[(unsigned char) *s])
			split_chars.push_back(((unsigned char) *s) << 1);
		    (*current)[(unsigned char) *s] = true;
		}

	    // potentially split vertically as well as by flow
	    if (_port_split) {
		// create input and output versions
		int n = split_chars.size();
		for (int i = 0; i < n; ++i)
		    split_chars.push_back(split_chars[i] + desplit_inputs);
		// remove meaningless combinations
		for (int i = 0; i < split_chars.size(); ++i)
		    if (split_chars[i] & desplit_inputs
			? (_port_split == dpdisp_outputs
			   || !inputs[split_chars[i] >> 1])
			: (_port_split == dpdisp_inputs
			   || !outputs[split_chars[i] >> 1])) {
			split_chars[i] = split_chars.back();
			split_chars.pop_back();
			--i;
		    }
	    }

	    // create splits for collected characters
	    for (int *scit = split_chars.begin();
		 scit != split_chars.end(); ++scit) {
		if (!_split_type)
		    _split_type = *scit;
		else {
		    delt *se = create_split(cr, *scit);
		    se->reccss(cr, dsense_always);
		    redraw = true;
		}
	    }

	} else if (_port_split == dpdisp_inputs)
	    _split_type = desplit_inputs;

	else if (_port_split == dpdisp_both) {
	    delt *se = create_split(cr, desplit_inputs);
	    se->reccss(cr, dsense_always);
	    redraw = true;
	}
    }

    return redraw;
}



/*****
 *
 *
 *
 */

void delt::layout_contents(dcontext &dcx)
{
    for (iterator e = begin_contents(); e; ++e)
	e->layout(dcx);

    if (root() || _display == dedisp_normal)
	position_contents_dot(dcx.cr, dcx.cr->error_handler());
    //position_contents_scc(router);
    //position_contents_first_heuristic(router);
}

void delt::layout_ports(dcontext &dcx)
{
    // XXX layout_ports
    delete[] _portoff[0];
    delete[] _portoff[1];
    _portoff[0] = _portoff[1] = 0;
    dcss_set *dcs = dcx.cr->ccss();
    int poff = 0;

    for (int isoutput = 0; isoutput < 2; ++isoutput) {
	ref_ptr<dport_style> dps = dcs->port_style(dcx.cr, this, isoutput, 0, 0);
	_ports_length[isoutput] = 2 * dps->edge_padding;
	_ports_width[isoutput] = 0;
	if (!_e->nports(isoutput)
	    || (_port_split && isoutput == (_split_type & desplit_inputs)))
	    continue;
	if (_e->nports(isoutput) > 1)
	    _portoff[isoutput] = new double[_e->nports(isoutput) + 1];
	double tm = dps->edge_padding;
	for (int p = 0; p < _e->nports(isoutput); ++p, ++poff) {
	    if (p)
		dps = dcs->port_style(dcx.cr, this, isoutput, p, 0);

	    double l;
	    if (dps->shape == dpshape_triangle)
		l = dps->length - 2;
	    else
		l = dps->length + 4;

	    double w = dps->width;
	    if (dps->shape == dpshape_triangle)
		w -= 1.5;

	    if (dps->text) {
		String markup = parse_markup(dps->text, dcx.cr, p, 0);
		if (dcx.pl_font != dps->font)
		    dcx.set_font_description(dps->font);
		pango_layout_set_width(dcx, -1);
		pango_layout_set_markup(dcx, markup.data(), markup.length());
		PangoRectangle rect;
		pango_layout_get_pixel_extents(dcx, NULL, &rect);
		l = std::max(l, (double) rect.width + 2);
		w += (double) rect.height + 1;
	    }

	    _ports_length[isoutput] += l;

	    double old_tm = tm;
	    tm += dps->margin[orientation()] + l / 2;
	    if (_e->nports(isoutput) > 1)
		_portoff[isoutput][p] = tm;
	    tm += dps->margin[orientation() ^ 2] + l / 2;
	    if (old_tm + 0.1 > tm)
		tm = old_tm + 0.1;

	    _ports_width[isoutput] = MAX(_ports_width[isoutput], w);
	}
	if (_e->nports(isoutput) > 1)
	    _portoff[isoutput][_e->nports(isoutput)] = tm + dps->edge_padding;
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

String delt::parse_markup(const String &text, crouter *cr,
			  int port, int *sensitivity)
{
    if (sensitivity)
	*sensitivity = 0;

    StringAccum sa;
    const char *last = text.begin(), *send = text.end();
    for (const char *s = text.begin(); s != send; ++s)
	if (*s == '%' && s + 1 != send) {
	    sa.append(last, s);

	    enum { pm_width = 0, pm_precision = 1, pm_specifier = 2 };
	    int vals[3], altflag = 0, which = 0;
	    vals[0] = vals[1] = vals[2] = -1;
	    const char *pct = s;
	    for (++s; s != send; ++s)
		if (isdigit((unsigned char) *s))
		    vals[which] = (vals[which] >= 0 ? 10 * vals[which] : 0)
			+ *s - '0';
		else if (*s == '*') {
		    if (which == pm_specifier)
			vals[which] = port;
		} else if (*s == '.')
		    which = pm_precision;
		else if (*s == ':')
		    which = pm_specifier;
		else if (*s == '#')
		    altflag = 1;
		else
		    break;

	    if (s == send) {
	      invalid_format:
		last = s = pct;
		continue;
	    }

	    if (*s == 'n')
		append_markup_quote(sa, display_name(), vals[pm_precision]);
	    else if (*s == 'c')
		append_markup_quote(sa, type_name(), vals[pm_precision]);
	    else if (*s == 'f')
		append_markup_quote(sa, flat_name(), vals[pm_precision]);
	    else if (*s == 'C') {
		String c = _e->configuration();
		if (vals[pm_specifier] >= 0) {
		    Vector<String> conf;
		    cp_argvec(c, conf);
		    if (conf.size() > vals[pm_specifier])
			c = conf[vals[pm_specifier]];
		    else
			c = String();
		}
		if (!altflag)
		    append_markup_quote(sa, c, vals[pm_precision]);
		else if (c) {
		    sa << '(';
		    append_markup_quote(sa, c, vals[pm_precision]);
		    sa << ')';
		}
	    } else if (*s == '{') {
		const char *n = s + 1;
		for (++s; s != send && *s != '}'; ++s)
		    /* nada */;
		if (s == send || n == s)
		    goto invalid_format;
		handler_value *hv = cr->hvalues().find_placeholder(flat_name() + "." + text.substring(n, s), hflag_notify_delt);
		if (hv) {
		    if (hv->have_hvalue())
			append_markup_quote(sa, hv->hvalue(), vals[pm_precision]);
		    else {
			if (altflag && hv->refreshable())
			    hv->set_flags(cr, hv->flags() | hflag_autorefresh);
			if (hv->refreshable())
			    hv->refresh(cr);
			if (vals[pm_width] > 0)
			    sa.append_fill('?', vals[pm_width]);
		    }
		    if (sensitivity)
			*sensitivity |= dsense_handler;
		}
	    } else
		goto invalid_format;

	    last = s + 1;
	}
    sa.append(last, send);

    // Trim empty markup
    for (char *lt = find(sa.begin(), sa.end(), '<'); lt != sa.end(); ) {
	char *gt = find(lt + 1, sa.end(), '>');
	char *after = gt + 3 + (gt - lt);
	if (after <= sa.end()
	    && gt[1] == '<' && gt[2] == '/'
	    && memcmp(gt + 3, lt + 1, gt - lt) == 0) {
	    memmove(lt, after, sa.end() - after);
	    sa.pop_back(after - lt);
	    lt = find(lt, sa.end(), '<');
	} else
	    lt = find(gt, sa.end(), '<');
    }
    // Trim terminating newlines
    while (sa.length() && sa.back() == '\n')
	sa.pop_back();

    return sa.take_string();
}

void delt::dimension_markup(dcontext &dcx)
{
    pango_layout_set_width(dcx, -1);
    if (_des->font != dcx.pl_font)
	dcx.set_font_description(_des->font);
    pango_layout_set_markup(dcx, _markup.data(), _markup.length());
    PangoRectangle rect;
    pango_layout_get_pixel_extents(dcx, NULL, &rect);
    _markup_width = rect.width * _dess->scale;
    _markup_height = rect.height * _dess->scale;
}

void delt::layout(dcontext &dcx)
{
    assert(_des && _dess && _width == 0 && _height == 0);

    // get contents width and height
    if (_elt.size() && dedisp_children_visible(_display))
	layout_contents(dcx);

    // exit if not visible
    if (!dedisp_visible(_display))
	return;

    // get text extents
    dimension_markup(dcx);
    // get port position
    layout_ports(dcx);

    // get element width and height
    double xwidth, xheight;
    if (_des->style == destyle_queue
	&& _contents_height == 0 && side_vertical(orientation())) {
	xwidth = _markup_height;
	xheight = _markup_width;
    } else {
	xwidth = MAX(_markup_width, _contents_width);
	xheight = _markup_height;
	if (_contents_height)
	    xheight += _contents_height;
    }

    double xpad[4];
    static_assert(sizeof(_dess->padding) == sizeof(xpad), "Padding screwup.");
    memcpy(xpad, _dess->padding, sizeof(xpad));
    if (!_contents_height) {	// Open displays already account for port widths
	xpad[orientation()] = MAX(xpad[orientation()], _ports_width[0]);
	xpad[orientation() ^ 2] = MAX(xpad[orientation() ^ 2], _ports_width[1]);
    }

    xwidth = MAX(xwidth + xpad[1] + xpad[3], _dess->min_width);
    xheight = MAX(xheight + xpad[0] + xpad[2], _dess->min_height);
    if (side_vertical(orientation()))
	xheight = MAX(xheight, _dess->min_length);
    else
	xwidth = MAX(xwidth, _dess->min_length);

    // analyze port positions
    double xportlen = MAX(_ports_length[0], _ports_length[1]) * _dess->scale;

    if (orientation() == 0)
	_width = ceil(MAX(xwidth, xportlen));
    else {
	_width = ceil(xwidth);
	xheight = MAX(xheight, xportlen);
    }

    if (xheight > _dess->min_height && _dess->height_step > 0)
	xheight = _dess->min_height + ceil((xheight - _dess->min_height) / _dess->height_step) * _dess->height_step;
    _height = xheight;

    // adjust by border width and fix to integer boundaries
    _width = ceil(_width + 2 * _dess->border_width);
    _height = ceil(_height + 2 * _dess->border_width);
}


void delt::layout_compound_ports_copy(delt *compound, bool isoutput)
{
    // XXX layout_ports
    delete[] _portoff[!isoutput];
    _portoff[!isoutput] = 0;
    _ports_length[!isoutput] = compound->_ports_length[isoutput];
    _ports_width[!isoutput] = compound->_ports_width[isoutput];
    if (compound->_portoff[isoutput]) {
	assert(_e->nports(!isoutput) == compound->_e->nports(isoutput));
	int n = _e->nports(!isoutput);
	_portoff[!isoutput] = new double[n + 1];
	memcpy(_portoff[!isoutput], compound->_portoff[isoutput], (n + 1) * sizeof(double));
    }
}

void delt::layout_compound_ports(crouter *cr)
{
    delt *ein = _elt[0], *eout = _elt[1];
    assert(ein && eout && ein->name() == "input" && !ein->visible() && eout->name() == "output" && !eout->visible());

    ein->layout_compound_ports_copy(this, false);
    eout->layout_compound_ports_copy(this, true);

    ref_ptr<dport_style> dps = cr->ccss()->port_style(cr, this, false, 0, 0);
    double in_width = std::max(ein->_ports_width[1], dps->width);
    dps = cr->ccss()->port_style(cr, this, true, 0, 0);
    double out_width = std::max(eout->_ports_width[0], dps->width);

    double sides[4];
    int o = orientation();
    sides[o] = sides[o ^ 2] = side(o);
    sides[o ^ 2] += (side_greater(o) ? -1 : 1) * in_width;
    sides[o ^ 1] = side(o ^ 1);
    sides[o ^ 3] = side(o ^ 3);
    ein->assign(sides[3], sides[0], sides[1] - sides[3], sides[2] - sides[0]);

    sides[o] = sides[o ^ 2] = side(o ^ 2);
    sides[o] += (side_greater(o) ? 1 : -1) * out_width;
    eout->assign(sides[3], sides[0], sides[1] - sides[3], sides[2] - sides[0]);

    // should probably ensure this another way...
    ein->_des = eout->_des = _des;
}

void delt::layout_complete(dcontext &dcx, double dx, double dy)
{
    if (dedisp_visible(_display)) {
	dx += _dess->padding[3];
	dy += _markup_height + _dess->padding[0];
    }

    for (iterator e = begin_contents(); e; ++e) {
	double unadjusted_x = dx, unadjusted_y = dy;
	if (e->visible()) {
	    unadjusted_x += e->_x;
	    unadjusted_y += e->_y;
	    e->_x = floor(unadjusted_x);
	    e->_y = floor(unadjusted_y);
	}
	if (e->_elt.size() && dedisp_children_visible(e->_display))
	    e->layout_complete(dcx, unadjusted_x, unadjusted_y);
    }

    if (dedisp_visible(_display) && _elt.size())
	layout_compound_ports(dcx.cr);

    if (root() || dedisp_children_visible(_display))
	for (std::vector<dconn *>::iterator it = _conn.begin();
	     it != _conn.end(); ++it)
	    (*it)->layout();
}

void delt::layout_main(dcontext &dcx)
{
    delt fake_child(this, 0);
    _des = dcx.cr->ccss()->elt_style(dcx.cr, &fake_child);
    _dess = dcx.cr->ccss()->elt_size_style(dcx.cr, &fake_child);
    _display = dedisp_expanded;
    layout_contents(dcx);
    layout_complete(dcx, _dess->margin[3], _dess->margin[0]);
    assign(0, 0, _contents_width + _dess->margin[1] + _dess->margin[3],
	   _contents_height + _dess->margin[0] + _dess->margin[2]);
}

void delt::insert_all(rect_search<dwidget> &rects)
{
    if (visible())
	rects.insert(this);
    if (dedisp_children_visible(_display)) {
	for (iterator e = begin_contents(); e; ++e)
	    e->insert_all(rects);
	for (std::vector<dconn *>::iterator ci = _conn.begin();
	     ci != _conn.end(); ++ci)
	    if ((*ci)->visible())
		rects.insert(*ci);
    }
}


/*****
 *
 *
 *
 */

bool dconn::layout()
{
    if (!visible())
	return false;
    point op = _elt[1]->output_position(_port[1], 0);
    point ip = _elt[0]->input_position(_port[0], 0);
    if (_elt[1]->orientation() == 0 && _elt[0]->orientation() == 0) {
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
    if (_route.size()) {
	affine t = affine::mapping(point(0, 0), op, _route.back(), ip);
	for (const point *pp = _route.begin(); pp != _route.end(); ++pp)
	    *this |= t * *pp;
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
	assign(0, 0, 0, 0);
	union_bounds(*this, false);
	_contents_width = _width;
	_contents_height = _height;
	expand(_dess->margin[0], _dess->margin[1],
	       _dess->margin[2], _dess->margin[3]);
    }
}

void delt::union_bounds(rectangle &r, bool self) const
{
    if (self && visible())
	r |= *this;
    if (dedisp_children_visible(_display)) {
	for (iterator e = begin_contents(); e; ++e)
	    e->union_bounds(r, true);
	for (std::vector<dconn *>::const_iterator ci = _conn.begin();
	     ci != _conn.end(); ++ci)
	    if ((*ci)->visible())
		r |= **ci;
    }
}

void delt::remove(rect_search<dwidget> &rects, rectangle &bounds)
{
    if (!visible())
	return;

    bounds |= *this;
    rects.remove(this);

    delt *p = _parent;
    while (!p->root() && p->display() == dedisp_expanded)
	p = p->parent();
    for (std::vector<dconn *>::iterator it = p->_conn.begin();
	 it != p->_conn.end(); ++it)
	if ((*it)->_elt[1]->_e == _e || (*it)->_elt[0]->_e == _e) {
	    bounds |= **it;
	    rects.remove(*it);
	}

    if (_parent && _elt.size() && _display == dedisp_normal) {
	delt *ein = _elt[0], *eout = _elt[1];
	ein->remove(rects, bounds);
	eout->remove(rects, bounds);
    }
}

void delt::insert(rect_search<dwidget> &rects, crouter *cr,
		  rectangle &bounds)
{
    if (!visible())
	return;

    bounds |= *this;
    rects.insert(this);

    delt *p = _parent;
    while (!p->root() && p->display() == dedisp_expanded)
	p = p->parent();
    for (std::vector<dconn *>::iterator it = p->_conn.begin();
	 it != p->_conn.end(); ++it)
	if ((*it)->_elt[1]->_e == _e || (*it)->_elt[0]->_e == _e) {
	    (*it)->layout();
	    bounds |= **it;
	    rects.insert(*it);
	}

    if (_parent && _elt.size() && _display == dedisp_normal) {
	layout_compound_ports(cr);
	delt *ein = _elt[0], *eout = _elt[1];
	ein->insert(rects, cr, bounds);
	eout->insert(rects, cr, bounds);
    }
}

dconn *delt::find_connection(bool isoutput, int port)
{
    delt *e = find_port_container(isoutput, port);
    delt *p = _parent;
    while (!p->root() && p->display() == dedisp_expanded)
	p = p->_parent;
    for (std::vector<dconn *>::iterator it = p->_conn.begin();
	 it != p->_conn.end(); ++it)
	if ((*it)->elt(isoutput) == e
	    && (*it)->port(isoutput) == port)
	    return *it;
    return 0;
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

    return side_length * _portoff[isoutput][port]
		 / _portoff[isoutput][_e->nports(isoutput)];
}

point delt::input_position(int port, dport_style *dps, bool here) const
{
    if (_flow_split && !here)
	return find_flow_split(false, port)->input_position(port, dps, true);
    else if (_port_split && !(_split_type & desplit_inputs) && _split && !here)
	return _split->input_position(port, 0, true);

    double off = port_position(false, port, side_length(orientation() ^ 1));

    double pos = side(orientation());
    if (dps) {
	double dd = (_des->style == destyle_queue ? 0 : _dess->border_width);
	if (dps->shape == dpshape_triangle)
	    dd = std::max(dd - dps->border_width / 2, dps->border_width / 2);
	pos += (side_greater(orientation() ^ 2) ? dd : -dd);
    }

    if (side_horizontal(orientation()))
	return point(pos, _y + off);
    else
	return point(_x + off, pos);
}

point delt::output_position(int port, dport_style *dps, bool here) const
{
    if (_flow_split && !here)
	return find_flow_split(true, port)->output_position(port, dps, true);
    else if (_port_split && (_split_type & desplit_inputs) && _split && !here)
	return _split->output_position(port, 0, true);

    double off = port_position(true, port, side_length(orientation() ^ 1));

    double pos = side(orientation() ^ 2);
    if (dps) {
	double dd = _dess->border_width;
	if (dps->shape == dpshape_triangle)
	    dd = std::max(dd - dps->border_width / 2, dps->border_width / 2);
	pos += (side_greater(orientation() ^ 2) ? -dd : dd);
    }

    if (side_horizontal(orientation()))
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

void delt::draw_port(dcontext &dcx, dport_style *dps, point p,
		     int port, bool isoutput, double opacity)
{
    // align position
    if (dcx.scale_step == 0 && _aligned) {
	double dd = (dps->shape == dpshape_triangle ? 0.5 : 0);
	if (side_vertical(orientation()))
	    p._x = round(p._x - dd) + dd;
	else
	    p._y = round(p._y - dd) + dd;
    }

    cairo_matrix_t original_matrix;
    cairo_get_matrix(dcx, &original_matrix);
    cairo_translate(dcx, p.x(), p.y());
    int port_orientation = orientation() ^ (isoutput ? 2 : 0);
    if (port_orientation & 1)
	cairo_rotate(dcx, M_PI_2);

    double l = dps->length * _dess->scale;
    double w = dps->width * _dess->scale;
    if (port_orientation & 2)
	l = -l, w = -w;

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

	cairo_set_source_rgba(dcx, color[0], color[1], color[2],
			      color[3] * opacity);

	double offset;
	if (i > 1)
	    offset = (dps->shape == dpshape_triangle ? 2 * M_SQRT2 : 2)
		* dps->border_width;
	else
	    offset = 0;
	if (offset > fabs(l / 2))
	    continue;
	if (port_orientation & 2)
	    offset = -offset;

	if (i == 1)
	    cairo_set_border(dcx, 0, dps->border_style, dps->border_width);
	else if (i == 3)
	    cairo_set_line_width(dcx, 0.75 * dps->border_width);

	if (dps->shape == dpshape_triangle) {
	    cairo_move_to(dcx, -l / 2 + offset, 0);
	    cairo_line_to(dcx, 0, w - 1.15 * offset);
	    cairo_line_to(dcx, l / 2 - offset, 0);
	    if (i < 3)
		cairo_close_path(dcx);
	} else {
	    cairo_move_to(dcx, -l / 2 + offset, 0);
	    cairo_line_to(dcx, -l / 2 + offset, w - offset);
	    cairo_line_to(dcx, l / 2 - offset, w - offset);
	    cairo_line_to(dcx, l / 2 - offset, 0);
	    if (!(i & 1))
		cairo_close_path(dcx);
	}

	if (i & 1)
	    cairo_stroke(dcx);
	else
	    cairo_fill(dcx);
    }

    if (dps->text) {
	String markup = parse_markup(dps->text, dcx.cr, port, 0);
	if (dcx.pl_font != dps->font)
	    dcx.set_font_description(dps->font);
	pango_layout_set_alignment(dcx, PANGO_ALIGN_CENTER);
	pango_layout_set_width(dcx, -1);
	pango_layout_set_markup(dcx, markup.data(), markup.length());
	PangoRectangle rect;
	pango_layout_get_pixel_extents(dcx, NULL, &rect);
	cairo_move_to(dcx, -rect.width / 2, w + (port_orientation & 2 ? -rect.height - 1 : 1));
	pango_cairo_show_layout(dcx, dcx);
    }

    cairo_set_matrix(dcx, &original_matrix);
}

void delt::draw_ports(dcontext &dcx)
{
    const char *pcpos = _processing_code.begin();
    int pcode;
    ref_ptr<dport_style> dps;

    if (!_port_split || (_split_type & desplit_inputs))
	for (int i = 0; i < _e->ninputs(); i++) {
	    pcpos = ProcessingT::processing_code_next
		(pcpos, _processing_code.end(), pcode);
	    dps = dcx.cr->ccss()->port_style(dcx.cr, this, false, i, pcode);
	    if (dps->display & dpdisp_inputs) {
		double opacity = (flow_split_char(false, i) != _split_type ? 0.25 : 1);
		draw_port(dcx, dps.get(), input_position(i, dps.get(), true),
			  i, false, opacity);
	    }
	}
    pcpos = ProcessingT::processing_code_output(_processing_code.begin(), _processing_code.end(), pcpos);
    if (!_port_split || !(_split_type & desplit_inputs))
	for (int i = 0; i < _e->noutputs(); i++) {
	    pcpos = ProcessingT::processing_code_next
		(pcpos, _processing_code.end(), pcode);
	    dps = dcx.cr->ccss()->port_style(dcx.cr, this, true, i, pcode);
	    if (dps->display & dpdisp_outputs) {
		double opacity = (flow_split_char(true, i) != _split_type ? 0.25 : 1);
		draw_port(dcx, dps.get(), output_position(i, dps.get(), true),
			  i, true, opacity);
	    }
	}
}

static void cairo_jagged_edge(cairo_t *cr, double x0, double y0,
			      double x1, double y1, int spo, double bwd)
{
    // adjust for border width so fill & stroke have same jags
    if (bwd) {
	if (spo & 1 ? y1 < y0 : x1 < x0)
	    bwd = -bwd;
	if (spo & 1)
	    y0 -= bwd, y1 += bwd;
	else
	    x0 -= bwd, x1 += bwd;
    }

    int n = (int) ceil(fabs(spo & 1 ? y1 - y0 : x1 - x0) / 20);
    double delta = (spo & 1 ? y1 - y0 : x1 - x0) / (2 * n);
    double shift = (rectangle::side_greater(spo) ? -5 : 5);
    for (int i = 1; i < 2*n; i++) {
	double my_shift = (i & 1 ? shift : 0);
	double x = (spo & 1 ? x0 + my_shift : x0 + i * delta);
	double y = (spo & 1 ? y0 + i * delta : y0 + my_shift);
	cairo_line_to(cr, x, y);
    }
}

void delt::draw_background(dcontext &dcx)
{
    double pos[4];
    double bwd = floor(_dess->border_width / 2);

    // figure out positions
    pos[3] = _x + bwd;
    pos[0] = _y + bwd;
    pos[1] = _x + _width - bwd;
    pos[2] = _y + _height - bwd;
    if (_des->style == destyle_queue)
	pos[orientation()] = side(orientation());

    // background
    if (_des->background_color[3]) {
	const double *color = _des->background_color;
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	if (!_port_split) {
	    cairo_move_to(dcx, pos[3], pos[0]);
	    cairo_line_to(dcx, pos[1], pos[0]);
	    cairo_line_to(dcx, pos[1], pos[2]);
	    cairo_line_to(dcx, pos[3], pos[2]);
	} else {
	    int spo = orientation();
	    if (_split_type & desplit_inputs)
		spo = (spo + 2) & 3;
	    cairo_move_to(dcx, pos[((spo + 3) & 2) + 1], pos[spo & 2]);
	    cairo_line_to(dcx, pos[((spo + 2) & 2) + 1], pos[(spo + 3) & 2]);
	    cairo_line_to(dcx, pos[((spo + 1) & 2) + 1], pos[(spo + 2) & 2]);
	    cairo_line_to(dcx, pos[(spo & 2) + 1], pos[(spo + 1) & 2]);
	    cairo_jagged_edge(dcx, pos[(spo & 2) + 1], pos[(spo + 1) & 2],
			      pos[((spo + 3) & 2) + 1], pos[spo & 2], spo, bwd);
	}
	cairo_close_path(dcx);
	cairo_fill(dcx);
    }

    if (_decor)
	ddecor::draw_list(_decor, this, pos, dcx);

    // queue lines
    if (_des->style == destyle_queue) {
	cairo_set_border(dcx, _des->queue_stripe_color, _des->queue_stripe_style, _des->queue_stripe_width);
	int o = orientation();
	double qls = _dess->queue_stripe_spacing;
	int num_lines = (int) ((side_length(o) - std::max(2.0, _dess->padding[o])) / qls);
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

void delt::clip_to_border(dcontext &dcx) const
{
    double x1 = _x, y1 = _y;
    double x2 = _x + _width, y2 = _y + _height;
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

void delt::draw_text(dcontext &dcx)
{
    // text
    const double *color = _des->color;
    cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
    pango_layout_set_wrap(dcx, PANGO_WRAP_CHAR);
    pango_layout_set_alignment(dcx, PANGO_ALIGN_CENTER);
    if (dcx.pl_font != _des->font)
	dcx.set_font_description(_des->font);
    if (_markup_width <= -1024)
	dimension_markup(dcx);

    double space[4];
    space[0] = space[1] = space[2] = space[3] = _dess->border_width;
    space[orientation()] += MAX(_ports_width[0], _dess->padding[orientation()]);
    space[orientation() ^ 2] += MAX(_ports_width[1], _dess->padding[orientation() ^ 2]);
    bool saved = false;
    double awidth = _width - space[1] - space[3];
    double aheight = _height - space[0] - space[2];

    if (awidth < _markup_width
	&& aheight >= _markup_width
	&& !_elt.size()) {
	// vertical layout
	saved = true;
	cairo_save(dcx);
	if (_markup_height > _width - 2)
	    clip_to_border(dcx);

	double dy = MAX((awidth - _markup_height) / 2, 0);
	cairo_translate(dcx, _x + space[3] + dy, _y + _height - space[2]);
	cairo_rotate(dcx, -M_PI / 2);
	cairo_move_to(dcx, 0, 0);
	cairo_scale(dcx, _dess->scale, _dess->scale);

	pango_layout_set_width(dcx, (int) (aheight / _dess->scale * PANGO_SCALE));
	pango_layout_set_markup(dcx, _markup.data(), _markup.length());
	pango_cairo_show_layout(dcx, dcx);

    } else {
	// normal horizontal layout, no text wrapping
	if (_markup_width > awidth
	    || _markup_height > aheight) {
	    saved = true;
	    cairo_save(dcx);
	    clip_to_border(dcx);
	} else if (_dess->scale != 1) {
	    saved = true;
	    cairo_save(dcx);
	}

	pango_layout_set_width(dcx, (int) (awidth / _dess->scale * PANGO_SCALE));
	pango_layout_set_markup(dcx, _markup.data(), _markup.length());

	double name_width, name_height;
	if (awidth >= _markup_width)
	    name_width = _markup_width, name_height = _markup_height;
	else {
	    name_width = awidth;
	    PangoRectangle rect;
	    pango_layout_get_pixel_extents(dcx, NULL, &rect);
	    name_height = rect.height * _dess->scale;
	}

	double dy = MAX((aheight - name_height - _contents_height) / 2, 1);
	if (_dess->scale == 1)
	    cairo_move_to(dcx, _x + space[3], _y + dy + space[0]);
	else {
	    cairo_translate(dcx, _x + space[3], _y + dy + space[0]);
	    cairo_move_to(dcx, 0, 0);
	    cairo_scale(dcx, _dess->scale, _dess->scale);
	}
	pango_cairo_show_layout(dcx, dcx);
    }

    if (saved)
	cairo_restore(dcx);
}

void delt::draw_drop_shadow(dcontext &dcx)
{
    double shift = (_highlight & (1 << dhlt_pressed)) ? 1 : 0;
    int spo = -1;
    if (_port_split) {
	spo = orientation();
	if (_split_type & desplit_inputs)
	    spo = (spo + 2) & 3;
    }
    double sw = _des->shadow_width;
    dcx.penumbra = std::max(dcx.penumbra, sw);
    if (spo != 1 && spo != 2) {
	double x0 = _x + sw - shift, y0 = _y + _height + (sw - shift) / 2;
	double x1 = _x + _width + (sw - shift) / 2, y1 = _y + sw - shift;
	cairo_set_line_width(dcx, sw - shift);
	cairo_move_to(dcx, x0, y0);
	cairo_line_to(dcx, x1, y0);
	cairo_line_to(dcx, x1, y1);
	cairo_stroke(dcx);
    } else {
	cairo_save(dcx);

	// clipping region
	double x0 = _x, y0 = _y + _height;
	double x1 = _x + _width, y1 = _y;
	cairo_move_to(dcx, x0, y0);
	if (spo == 2)
	    cairo_jagged_edge(dcx, x0, y0, x1, y0, spo, 0);
	cairo_line_to(dcx, x1, y0);
	if (spo == 1)
	    cairo_jagged_edge(dcx, x1, y0, x1, y1, spo, 0);
	cairo_line_to(dcx, x1, y1);
	cairo_line_to(dcx, x1 + sw, y1);
	cairo_line_to(dcx, x1 + sw, y0 + sw);
	cairo_line_to(dcx, x0, y0 + sw);
	cairo_close_path(dcx);
	cairo_clip(dcx);

	// actual path
	x0 += sw - shift;
	x1 += sw - shift;
	y0 += sw - shift;
	y1 += sw - shift;
	cairo_move_to(dcx, x0, y0);
	if (spo == 2)
	    cairo_jagged_edge(dcx, x0, y0, x1, y0, spo, 0);
	cairo_line_to(dcx, x1, y0);
	if (spo == 1)
	    cairo_jagged_edge(dcx, x1, y0, x1, y1, spo, 0);
	cairo_line_to(dcx, x1, y1);
	double delta = sw + 5;
	cairo_line_to(dcx, x1 - delta, y1);
	cairo_line_to(dcx, x1 - delta, y0 - delta);
	cairo_line_to(dcx, x0, y0 - delta);
	cairo_close_path(dcx);
	cairo_fill(dcx);

	cairo_restore(dcx);
    }
}

void delt::draw_outline(dcontext &dcx)
{
    // shadow
    if (_des->shadow_style != dshadow_none) {
	const double *color = _des->shadow_color;
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	cairo_set_dash(dcx, 0, 0, 0);
	if (_des->shadow_style == dshadow_drop)
	    draw_drop_shadow(dcx);
	else {
	    double sw = _des->shadow_width;
	    if (_des->shadow_style == dshadow_unscaled_outline)
		sw /= dcx.scale;
	    dcx.penumbra = std::max(dcx.penumbra, sw);
	    cairo_set_line_width(dcx, sw);
	    double x = _x - sw / 2, y = _y - sw / 2;
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
			 _dess->border_width);
	double pos[4];
	double bwd = _dess->border_width / 2;
	pos[3] = _x + bwd;
	pos[0] = _y + bwd;
	pos[1] = _x + _width - bwd;
	pos[2] = _y + _height - bwd;
	int o = orientation(), open = (_des->style == destyle_queue);
	if (_port_split) {
	    open = (open && (_split_type & desplit_inputs) ? 2 : 1);
	    o = (_split_type & desplit_inputs ? (o + 2) & 3 : o);
	}
	if (open)
	    pos[o] = side(o);
	if (_des->style == destyle_queue)
	    pos[orientation()] = side(orientation());
	cairo_move_to(dcx, pos[((o + 3) & 2) + 1], pos[o & 2]);
	cairo_line_to(dcx, pos[((o + 2) & 2) + 1], pos[(o + 3) & 2]);
	if (open == 2)
	    cairo_move_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 2) & 2]);
	else
	    cairo_line_to(dcx, pos[((o + 1) & 2) + 1], pos[(o + 2) & 2]);
	cairo_line_to(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2]);
	if (!open)
	    cairo_close_path(dcx);
	cairo_stroke(dcx);

	if (_port_split) {
	    const double *color = _des->border_color;
	    cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3] * 0.25);
	    cairo_set_line_width(dcx, 0.5);
	    cairo_move_to(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2]);
	    cairo_jagged_edge(dcx, pos[(o & 2) + 1], pos[(o + 1) & 2],
			      pos[((o + 3) & 2) + 1], pos[o & 2], o, bwd);
	    cairo_line_to(dcx, pos[((o + 3) & 2) + 1], pos[o & 2]);
	    cairo_stroke(dcx);
	}
    }
}

void delt::draw(dcontext &dcx)
{
    if (visible()) {
	int change = 0;
	if (_highlight != _drawn_highlight)
	    change |= dsense_highlight;
	if (dcx.generation != _generation)
	    change |= dsense_always;
	reccss(dcx.cr, change);
	_drawn_highlight = _highlight;
	_generation = dcx.generation;

	if (_highlight & (1 << dhlt_pressed))
	    cairo_translate(dcx, 1, 1);
	draw_background(dcx);
	draw_text(dcx);
	draw_ports(dcx);
	draw_outline(dcx);
	if (_decor)
	    ddecor::draw_list(_decor, this, 0, dcx);
	if (_highlight & (1 << dhlt_pressed))
	    cairo_translate(dcx, -1, -1);
    }
}

void dconn::draw(dcontext &dcx)
{
    int cdisp = change_display(_count_change);
    double width = 1;
    switch (cdisp) {
    case 0:
	cairo_set_source_rgb(dcx, 0.5, 0.5, 0.5);
	break;
    case 1:
	cairo_set_source_rgb(dcx, 0, 0, 0);
	break;
    default: {
	double scale = (double) cdisp / (sizeof(unsigned) * 8 - 1);
	cairo_set_source_rgb(dcx, scale, 0, 0);
	width = 8 * scale + 1;
	break;
    }
    }
    cairo_set_line_width(dcx, width);
    cairo_set_dash(dcx, 0, 0, 0);

    point op = _elt[1]->output_position(_port[1], 0);
    point ip = _elt[0]->input_position(_port[0], 0);
    point next_to_last;

    if (_elt[1]->vertical())
	cairo_move_to(dcx, op.x(), op.y() - 0.5);
    else
	cairo_move_to(dcx, op.x() - 0.5, op.y());

    if ((_elt[1]->vertical() && _elt[0]->vertical()
	 && fabs(ip.x() - op.x()) <= 6)
	|| (!_elt[1]->vertical() && !_elt[0]->vertical()
	    && fabs(ip.y() - op.y()) <= 6)) {
	/* no curves */
	cairo_line_to(dcx, ip.x(), ip.y());
	next_to_last = op;

    } else if (_route.size()) {
	const point *r = _route.begin();
	assert((_route.size() % 3) == 0);
	affine t = affine::mapping(point(0, 0), op, _route.back(), ip);
	for (; r != _route.end(); r += 3)
	    cairo_curve_to_points(dcx, t * r[0], t * r[1], t * r[2]);
	next_to_last = t * _route[_route.size() - 2];

    } else {
	point curvea;
	if (_elt[1]->vertical()) {
	    cairo_line_to(dcx, op.x(), op.y() + 3);
	    curvea = point(op.x(), op.y() + 10);
	} else {
	    cairo_line_to(dcx, op.x() + 3, op.y());
	    curvea = point(op.x() + 10, op.y());
	}
	if (_elt[0]->vertical()) {
	    cairo_curve_to(dcx, curvea.x(), curvea.y(),
			   ip.x(), ip.y() - 12, ip.x(), ip.y() - 7);
	    next_to_last = point(ip.x(), ip.y() - 7);
	} else {
	    cairo_curve_to(dcx, curvea.x(), curvea.y(),
			   ip.x() - 12, ip.y(), ip.x() - 7, ip.y());
	    next_to_last = point(ip.x() - 7, ip.y());
	}
	cairo_line_to(dcx, ip.x(), ip.y());

    }

    cairo_stroke(dcx);

    double epx = ip.x(), epy = ip.y();
    (_elt[0]->vertical() ? epy : epx) += 0.25;
    double angle = (ip - next_to_last).angle();
    cairo_move_to(dcx, epx, epy);
    double arrow_width = 3 * (width + 3) / 4;
    cairo_rel_line_to_point(dcx, point(-5.75, -arrow_width).rotated(angle));
    cairo_rel_line_to_point(dcx, point(+2.00, +arrow_width).rotated(angle));
    cairo_rel_line_to_point(dcx, point(-2.00, +arrow_width).rotated(angle));
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
    if (_visible) {
	_xrect = *this;
	for (iterator e = begin_contents(); e; ++e)
	    e->drag_prepare();
    }
}

void delt::drag_shift(wdiagram *d, const point &delta)
{
    if (_visible) {
	rectangle bounds = *this;
	remove(d->rects(), bounds);
	_x = _xrect._x + delta.x();
	_y = _xrect._y + delta.y();
	insert(d->rects(), d->main(), bounds);
	d->redraw(bounds);
	for (iterator e = begin_contents(); e; ++e)
	    e->drag_shift(d, delta);
    }
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
    insert(d->rects(), d->main(), bounds);
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

handler_value *delt::handler_interest(crouter *cr, const String &hname,
				      bool autorefresh,
				      int autorefresh_period,
				      bool always)
{
    return cr->hvalues().find_placeholder
	(_flat_name + "." + hname,
	 hflag_notify_delt | (autorefresh ? hflag_autorefresh : 0)
	 | (always ? hflag_always_notify_delt : 0),
	 autorefresh_period);
}

void delt::notify_read(wdiagram *d, handler_value *hv)
{
    for (delt *e = this; e; e = e->next_split(this)) {
	ddecor::notify_list(e->_decor, d->main(), e, hv);
	if (e->reccss(d->main(), dsense_handler))
	    d->redraw(*e);
    }
}

bool dconn::change_count(unsigned new_count)
{
    if (_count_last == ~0U && _count_change == ~0U) {
	_count_last = new_count;
	return false;
    } else {
	unsigned old_change = _count_change;
	_count_change = new_count - _count_last;
	_count_last = new_count;
	return change_display(old_change) != change_display(_count_change);
    }
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

    double attach = MAX(2.0, d->scale() * _dess->border_width);
    if (d->scale_step() > -5
	&& r.width() >= 18
	&& r.height() >= 18
	&& (window_x - r.x1() < attach
	    || window_y - r.y1() < attach
	    || r.x2() - window_x - 1 < attach
	    || r.y2() - window_y - 1 < attach)) {
	int gnum = deg_element;
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

    return deg_element;
}

}
