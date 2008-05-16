#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "crouter.hh"
#include "cdriver.hh"
#include "dstyle.hh"
#include <clicktool/routert.hh>
#include <clicktool/lexert.hh>
#include <clicktool/lexertinfo.hh>
#include <clicktool/toolutils.hh>
#include <clicktool/elementmap.hh>
#include <clicktool/processingt.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/pathvars.h>
#include <click/vector.cc>
#include <math.h>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
namespace clicky {

String g_click_to_utf8(const String &str)
{
    for (const char *s = str.begin(); s != str.end(); ++s)
	if (((unsigned char) *s) > 126 || *s == '\0') {
	    gsize bytes_read, bytes_written;
	    GError *err = NULL;
	    char *x = g_convert(str.data(), str.length(), "UTF-8", "ISO-8859-1", &bytes_read, &bytes_written, &err);
	    String new_str(x, bytes_written);
	    g_free(x);
	    return new_str;
	}
    return str;
}

crouter::crouter()
    : _r(0), _emap(0), _selected_driver(-1), _processing(0),
      _hvalues(this), _driver(0), _driver_active(false),
      _ccss(new dcss_set(dcss_set::default_set("screen"))),
      _throbber_count(0)
{
}

crouter::~crouter()
{
    clear(false);
}

bool crouter::empty() const
{
    return (!_r && !_conf && (!_driver || !_driver->active()));
}

bool crouter::element_exists(const String &ename) const
{
    Vector<ElementT *> path;
    return (_r && _r->element_path(ename, path));
}

ElementClassT *crouter::element_type(const String &ename) const
{
    Vector<ElementT *> path;
    if (_r && _r->element_path(ename, path))
	return path.back()->type();
    else
	return 0;
}

void crouter::clear(bool alive)
{
    delete _r;
    delete _emap;
    delete _processing;
    _r = 0;
    _emap = 0;
    _selected_driver = -1;
    _processing = 0;
    _gerrh.clear();
    _conf = _landmark = String();

    // kill throbber before removing csocket messages, so if destroying window
    // while callbacks are outstanding, won't touch throbber widget
    while (alive && _throbber_count)
	throbber(false);
    _throbber_count = 0;

    if (_driver)
	delete _driver;
    _driver = 0;
    _driver_active = false;

    // XXX _hvalues.clear();
}

void crouter::set_landmark(const String &landmark)
{
    _landmark = landmark;
    on_landmark_changed();
}

String crouter::ccss_text() const
{
    return _ccss->text();
}

void crouter::set_ccss_text(const String &str)
{
    if (_ccss->text() != str) {
	String media = _ccss->media();
	delete _ccss;
	_ccss = new dcss_set(dcss_set::default_set(media));
	// XXX ++_pango_generation;
	_ccss->parse(str);
	on_ccss_changed();
    }
}

void crouter::set_ccss_media(const String &media)
{
    _ccss = _ccss->remedia(media);
}


/*****
 *
 * Throbber
 *
 */

void crouter::throbber(bool show)
{
    if (show) {
	if (++_throbber_count == 1)
	    on_throbber_changed(true);
    } else {
	if (_throbber_count > 0 && --_throbber_count == 0)
	    on_throbber_changed(false);
    }
}

extern "C" {
static gboolean throb_after_timeout(gpointer user_data)
{
    crouter::throb_after *ta = reinterpret_cast<crouter::throb_after *>(user_data);
    ta->_timeout = 0;
    ta->_cr->throbber(true);
    return FALSE;
}
}

crouter::throb_after::throb_after(crouter *cr, int timeout)
    : _cr(cr), _timeout(g_timeout_add(timeout, throb_after_timeout, this))
{
}

crouter::throb_after::~throb_after()
{
    if (_timeout)
	g_source_remove(_timeout);
    else
	_cr->throbber(false);
}


/*****
 *
 * initializing configuration
 *
 */

void crouter::set_config(const String &conf, bool replace)
{
    _gerrh.clear();
    
    // check for archive
    _conf = conf;
    Vector<ArchiveElement> archive;
    if (_conf.length() && _conf[0] == '!'
	&& ArchiveElement::parse(_conf, archive, &_gerrh) >= 0) {
	int found = ArchiveElement::arindex(archive, "config");
	if (found >= 0)
	    _conf = archive[found].data;
	else {
	    _gerrh.error("archive has no 'config' section");
	    _conf = String();
	}
    }

    // read router
    if (!_conf.length())
	_gerrh.warning("empty configuration");
    LexerT lexer(&_gerrh, false);
    lexer.reset(_conf, archive, (_driver ? "config" : _landmark));
    LexerTInfo *lexinfo = on_config_changed_prepare();
    if (lexinfo)
	lexer.set_lexinfo(lexinfo);
    while (lexer.ystatement())
	/* nada */;
    RouterT *r = lexer.finish(global_scope);
    r->check();

    // if router, read element map
    ElementMap *emap = new ElementMap;
    ProcessingT *processing = 0;
    if (r) {
	emap->parse_all_files(r, CLICK_DATADIR, &_gerrh);
	if (_driver)
	    emap->set_driver_mask(_driver->driver_mask());
	else {
	    int driver = emap->pick_driver(_selected_driver, r, 0);
	    emap->set_driver_mask(1 << driver);
	}
	processing = new ProcessingT(r, emap, &_gerrh);
	processing->check_types(&_gerrh);
    }

    // save results
    if (replace) {
	delete _r;
	delete _emap;
	delete _processing;
	_r = r;
	_emap = emap;
	_processing = processing;
	_downstreams.clear();
	_upstreams.clear();
    } else {
	delete r;
	delete emap;
	delete processing;
    }
    
    on_config_changed(replace, lexinfo);

    delete lexinfo;
}


/*****
 *
 * driver connection
 *
 */

void crouter::set_driver(cdriver *driver, bool active)
{
    assert(driver && (!_driver || driver == _driver));
    _driver = driver;
    _driver_active = active;
}

void crouter::on_handler_read(const String &hname, const String &hparam,
			      const String &hvalue,
			      int status, messagevector &messages)
{
    if (hname == "config")
	set_config(hvalue, true);
    else {
	bool changed;
	handler_value *hv = _hvalues.set(hname, hparam, hvalue, changed);
	if ((changed && hv->notify_whandlers()) || hv->notify_delt(changed))
	    on_handler_read(hv, changed);
    }
    if (status == 200)
	messages.clear();
}

void crouter::on_handler_write(const String &hname, const String &hvalue,
			       int status, messagevector &messages)
{
    (void) hvalue;
    if (hname == "hotconfig") {
	if (status < 300)
	    messages.erase(messages.begin());
    } else if (status == 200)
	messages.clear();
}

void crouter::on_handler_check_write(const String &hname,
				     int status, messagevector &messages)
{
    (void) hname, (void) status;
    messages.clear();
}


/*****
 *
 * reachability
 *
 */

static const char *parse_port(const char *s, const char *end, int &port)
{
    if (s != end && *s == '[') {
	s = cp_integer(cp_skip_space(s + 1, end), end, 10, &port);
	s = cp_skip_space(s, end);
	if (s != end && *s == ']')
	    s = cp_skip_space(s + 1, end);
    }
    return s;
}

static void parse_port(const String &str, String &name, int &port)
{
    const char *s = str.begin(), *end = str.end();
    
    port = -1;
    s = parse_port(s, end, port);
    if (s != end && end[-1] == ']') {
	const char *t = end - 1;
	while (t != s && (isdigit((unsigned char) t[-1])
			  || isspace((unsigned char) t[-1])))
	    --t;
	if (t != s && t[-1] == '[') {
	    parse_port(t - 1, end, port);
	    for (end = t - 1; end != s && isspace((unsigned char) end[-1]); --end)
		/* nada */;
	}
    }
    
    name = str.substring(s, end);
}


crouter::reachable_match_t::reachable_match_t(const String &name, int port,
					      bool forward, RouterT *router,
					      ProcessingT *processing)
    : _name(name), _port(port), _forward(forward),
      _router(router), _router_name(), _processing(processing)
{
}

crouter::reachable_match_t::reachable_match_t(const reachable_match_t &m,
					      ElementT *e)
    : _name(m._name), _port(m._port), _forward(m._forward),
      _router_name(m._router_name),
      _processing(new ProcessingT(*m._processing, e))
{
    _router = _processing->router();
    if (_router_name)
	_router_name += '/';
    _router_name += e->name();
}

crouter::reachable_match_t::~reachable_match_t()
{
    if (_router_name)
	delete _processing;
}

bool crouter::reachable_match_t::get_seed(int eindex, int port) const
{
    if (!_seed.size())
	return false;
    else
	return _seed[_processing->pidx(eindex, port, !_forward)];
}

void crouter::reachable_match_t::set_seed(const ConnectionT &conn)
{
    if (!_seed.size())
	_seed.assign(_processing->npidx(!_forward), false);
    _seed[_processing->pidx(conn, !_forward)] = true;
}

void crouter::reachable_match_t::set_seed_connections(ElementT *e, int port)
{
    assert(port >= -1 && port < e->nports(_forward));
    for (RouterT::conn_iterator cit = _router->begin_connections_touching(PortT(e, port), _forward);
	 cit != _router->end_connections(); ++cit)
	set_seed(*cit);
}

bool crouter::reachable_match_t::add_matches(reachable_t &reach)
{
    for (RouterT::iterator it = _router->begin_elements();
	 it != _router->end_elements(); ++it)
	if (glob_match(it->name(), _name)
	    || glob_match(it->type_name(), _name)
	    || (_name && _name[0] == '#' && glob_match(it->name(), _name.substring(1))))
	    set_seed_connections(it, _port);
	else if (it->resolved_router(_processing->scope())) {
	    reachable_match_t sub_match(*this, it);
	    RouterT *sub_router = sub_match._router;
	    if (sub_match.add_matches(reach)) {
		assert(!reach.compound.get_pointer(sub_match._router_name));
		sub_match.export_matches(reach);
		assert(sub_router->element(0)->name() == "input"
		       && sub_router->element(1)->name() == "output");
		for (int p = 0; p < sub_router->element(_forward)->nports(!_forward); ++p)
		    if (sub_match.get_seed(_forward, p))
			set_seed_connections(it, p);
	    }
	}
    if (_seed.size()) {
	_processing->follow_reachable(_seed, !_forward, _forward);
	return true;
    } else
	return false;
}

void crouter::reachable_match_t::export_matches(reachable_t &reach)
{
    Bitvector &x = (_router_name ? reach.compound[_router_name] : reach.main);
    if (!x.size())
	x.assign(_router->nelements(), false);
    assert(x.size() == _router->nelements());
    if (!_seed.size())
	return;

    for (RouterT::iterator it = _router->begin_elements();
	 it != _router->end_elements(); ++it) {
	if (it->tunnel())
	    continue;
	int pidx = _processing->pidx(it->eindex(), 0, !_forward);
	bool any = false;
	for (int p = 0; p < it->nports(!_forward); ++p)
	    if (_seed[pidx + p]) {
		x[it->eindex()] = any = true;
		break;
	    }
	if (any && it->resolved_router(_processing->scope())) {
	    // spread matches from inputs through the compound
	    reachable_match_t sub_match(*this, it);
	    RouterT *sub_router = sub_match._router;
	    for (int p = 0; p < it->nports(!_forward); ++p)
		if (_seed[pidx + p])
		    sub_match.set_seed_connections(sub_router->element(!_forward), p);
	    if (sub_match._seed.size()) {
		sub_match._processing->follow_reachable(sub_match._seed, !_forward, _forward);
		sub_match.export_matches(reach);
	    }
	}
    }
}

void crouter::calculate_reachable(const String &str, bool forward, reachable_t &reach)
{
    String ename;
    int port;
    parse_port(str, ename, port);

    reachable_match_t match(ename, port, forward, _r, _processing);
    match.add_matches(reach);
    match.export_matches(reach);
}

}
