#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "crouter.hh"
#include "cdriver.hh"
#include "dstyle.hh"
#include "scopechain.hh"
#include <clicktool/routert.hh>
#include <clicktool/lexert.hh>
#include <clicktool/lexertinfo.hh>
#include <clicktool/toolutils.hh>
#include <clicktool/elementmap.hh>
#include <clicktool/processingt.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/pathvars.h>
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

crouter::crouter(dcss_set *ccss)
    : _r(0), _emap(0), _selected_driver(-1), _processing(0),
      _hvalues(this), _driver(0), _driver_active(false), _driver_process(0),
      _ccss(ccss), _router_ccss(false), _gerrh(true), _throbber_count(0)
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

bool crouter::element_exists(const String &ename, bool only_primitive) const
{
    ScopeChain chain(_r);
    ElementT *e = chain.push_element(ename);
    return e && (!only_primitive || chain.resolved_type(e)->primitive());
}

ElementClassT *crouter::element_type(const String &ename) const
{
    ScopeChain chain(_r);
    ElementT *e = chain.push_element(ename);
    return (e ? chain.resolved_type(e) : 0);
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
    if (_driver_process)
	kill(_driver_process, SIGHUP);
    _driver = 0;
    _driver_active = false;
    _driver_process = 0;

    // XXX _hvalues.clear();
}

void crouter::set_landmark(const String &landmark)
{
    _landmark = landmark;
    on_landmark_changed();
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
	if (ArchiveElement *ae = ArchiveElement::find(archive, "config"))
	    _conf = ae->data;
	else {
	    _gerrh.error("archive has no %<config%> section");
	    _conf = String();
	}
    }

    // read router
    if (!_conf.length())
	_gerrh.warning("empty configuration");
    LexerT lexer(&_gerrh, false);
    lexer.expand_groups(true);
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
	calculate_router_ccss();
    } else {
	delete r;
	delete emap;
	delete processing;
    }

    on_config_changed(replace, lexinfo);

    delete lexinfo;
}

void
crouter::calculate_router_ccss()
{
    if (_router_ccss) {
	dcss_set *old_ccss = _ccss;
	_ccss = old_ccss->below();
	delete old_ccss;
	_router_ccss = false;
    }

    StringAccum sa;
    for (RouterT::type_iterator it = _r->begin_elements(ElementClassT::base_type("ClickyInfo"));
	 it != _r->end_elements(); ++it) {
	String s;
	(void) Args().push_back_args(it->config())
	    .read_p("STYLE", AnyArg(), s).execute();
	if (s && s[0] == '\"')
	    s = cp_unquote(s);
	if (s)
	    sa << s << '\n';
    }

    if (sa) {
	_ccss = new dcss_set(_ccss);
	_ccss->parse(dcss_set::expand_imports(sa.take_string(), _landmark, 0));
    }
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
    on_driver_changed();
}

void crouter::on_driver_connected()
{
    _hvalues.clear();
}

void crouter::kill_driver()
{
    if (_driver)
	delete _driver;
    if (_driver_process)
	kill(_driver_process, SIGHUP);
    _driver = 0;
    _driver_active = false;
    _driver_process = 0;
    _hvalues.clear();
    on_driver_changed();
}

void crouter::run(ErrorHandler *errh)
{
    kill_driver();

    int configpipe[2], ctlsocket[2];
    if (pipe(configpipe) == -1)
	assert(0);
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, ctlsocket) == -1)
	assert(0);

    _driver_process = fork();
    if (_driver_process == -1)
	assert(0);
    else if (_driver_process == 0) {
	close(0);
	dup2(configpipe[0], 0);
	close(configpipe[0]);
	close(configpipe[1]);
	close(ctlsocket[0]);

	String arg = String(ctlsocket[1]);
	execlp("click", "click", "-R", "--socket", arg.c_str(), (const char *) 0);
	assert(0);
    }

    close(configpipe[0]);
    close(ctlsocket[1]);
    if (csocket_cdriver::make_nonblocking(ctlsocket[0], errh) != 0)
	assert(0);

    int pos = 0;
    while (pos != _conf.length()) {
	ssize_t r = write(configpipe[1], _conf.begin() + pos, _conf.length() - pos);
	if (r == 0 || (r == -1 && errno != EAGAIN && errno != EINTR)) {
	    if (r == -1)
		errh->message("%s while writing configuration", strerror(errno));
	    break;
	} else if (r != -1)
	    pos += r;
    }
    if (pos != _conf.length()) {
	kill_driver();
	close(configpipe[1]);
	close(ctlsocket[0]);
    } else {
	close(configpipe[1]);
	GIOChannel *channel = g_io_channel_unix_new(ctlsocket[0]);
	g_io_channel_set_encoding(channel, NULL, NULL);
	new csocket_cdriver(this, channel, true);
    }
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
    if (status == 200 || hname.equals("handlers", 8)
	|| hname.equals("active_ports", 12))
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
    for (RouterT::conn_iterator cit = _router->find_connections_touching(PortT(e, port), _forward);
	 cit != _router->end_connections(); ++cit)
	set_seed(*cit);
}

bool crouter::reachable_match_t::add_matches(reachable_t &reach, ErrorHandler *debug_errh)
{
    for (RouterT::iterator it = _router->begin_elements();
	 it != _router->end_elements(); ++it)
	if (glob_match(it->name(), _name)
	    || glob_match(it->type_name(), _name)
	    || (_name && _name[0] == '#' && glob_match(it->name(), _name.substring(1))))
	    set_seed_connections(it.get(), _port);
	else if (it->resolved_router(_processing->scope())) {
	    reachable_match_t sub_match(*this, it.get());
	    RouterT *sub_router = sub_match._router;
	    if (sub_match.add_matches(reach, debug_errh)) {
		assert(!reach.compound.get_pointer(sub_match._router_name));
		sub_match.export_matches(reach, debug_errh);
		assert(sub_router->element(0)->name() == "input"
		       && sub_router->element(1)->name() == "output");
		for (int p = 0; p < sub_router->element(_forward)->nports(!_forward); ++p)
		    if (sub_match.get_seed(_forward, p))
			set_seed_connections(it.get(), p);
	    }
	}
    if (_seed.size()) {
	_processing->follow_reachable(_seed, !_forward, _forward, 0, debug_errh);
	return true;
    } else
	return false;
}

void crouter::reachable_match_t::export_matches(reachable_t &reach, ErrorHandler *debug_errh)
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
	    reachable_match_t sub_match(*this, it.get());
	    RouterT *sub_router = sub_match._router;
	    for (int p = 0; p < it->nports(!_forward); ++p)
		if (_seed[pidx + p])
		    sub_match.set_seed_connections(sub_router->element(!_forward), p);
	    if (sub_match._seed.size()) {
		sub_match._processing->follow_reachable(sub_match._seed, !_forward, _forward, 0, debug_errh);
		sub_match.export_matches(reach, debug_errh);
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
    match.add_matches(reach, 0);
    match.export_matches(reach, 0);
}

}
