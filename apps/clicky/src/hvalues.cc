#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "hvalues.hh"
#include "wdriver.hh"
#include "diagram.hh"
#include <gdk/gdkkeysyms.h>
#include <click/confparse.hh>
extern "C" {
#include "interface.h"
#include "support.h"
}
namespace clicky {

/*****
 *
 * handler_value, including autorefresh
 *
 */

static String::Initializer string_initializer;
const String handler_value::no_hvalue_string = String::stable_string("???", 3);

namespace {
struct autorefresher {
    handler_value *hv;
    wmain *w;
    int period;
    autorefresher(handler_value *hv_, wmain *w_, int period_)
	: hv(hv_), w(w_), period(period_) {
    }
};

extern "C" {
static gboolean on_autorefresh(gpointer user_data)
{
    autorefresher *wa = reinterpret_cast<autorefresher *>(user_data);
    return wa->hv->on_autorefresh(wa->w, wa->period);
}

static void destroy_autorefresh(gpointer user_data) {
    autorefresher *wa = reinterpret_cast<autorefresher *>(user_data);
    delete wa;
}
}}

void handler_value::refresh(wmain *w)
{
    int read_flags = (_flags & hflag_raw ? 0 : wdriver::dflag_nonraw);
    w->driver()->do_read(_hname, _hparam, read_flags);
}

void handler_value::create_autorefresh(wmain *w)
{
    autorefresher *a = new autorefresher(this, w, _autorefresh_period);
    _autorefresh_source = g_timeout_add_full
	(G_PRIORITY_DEFAULT, _autorefresh_period,
	 clicky::on_autorefresh, a, destroy_autorefresh);
}

gboolean handler_value::on_autorefresh(wmain *w, int period)
{
    if ((_flags & hflag_autorefresh) != 0
	&& (_flags & hflag_autorefresh_outstanding) == 0
	&& readable()
	&& w->driver()) {
	_flags |= hflag_autorefresh_outstanding;
	refresh(w);
	if (period != _autorefresh_period) {
	    create_autorefresh(w);
	    return FALSE;
	} else
	    return TRUE;
    } else {
	_autorefresh_source = 0;
	return FALSE;
    }
}

void handler_value::set_flags(wmain *w, int new_flags)
{
    assert((new_flags & hflag_mandatory_driver_mask) == (_driver_flags & hflag_mandatory_driver_mask));
    
    if (_autorefresh_source
	&& ((new_flags & hflag_autorefresh) == 0
	    || (new_flags & hflag_r) == 0)) {
	g_source_remove(_autorefresh_source);
	_autorefresh_source = 0;
    } else if (_autorefresh_source == 0
	       && (new_flags & hflag_autorefresh) != 0
	       && (new_flags & hflag_autorefresh_outstanding) == 0
	       && (new_flags & hflag_r))
	create_autorefresh(w);

    if ((new_flags & hflag_have_hvalue) == 0)
	_hvalue = (new_flags & hflag_r ? no_hvalue_string : String());

    _flags = new_flags;
    _driver_mask &= ~(_flags ^ _driver_flags); 
}


/*****
 *
 * handler_values
 *
 */

handler_values::handler_values(wmain *w)
    : _w(w)
{
}

handler_values::iterator handler_values::begin(const String &ename)
{
    HashMap<handler_value>::iterator iter = _hv.find(ename + ".handlers");
    if (iter && (iter->_driver_flags & hflag_dead) == 0)
	return iterator(iter.operator->());
    else
	return iterator(0);
}

handler_value *handler_values::set(const String &hname, const String &hparam, const String &hvalue, bool &changed)
{
    if (hname.length() > 9 && memcmp(hname.end() - 9, ".handlers", 9) == 0)
	set_handlers(hname, hparam, hvalue);
    
    handler_value *hv = _hv.find_force(hname).get();
    changed = (!hv->have_hvalue() || hparam != hv->_hparam
	       || hvalue != hv->_hvalue);
    hv->_hparam = hparam;
    hv->_hvalue = hvalue;
    hv->_flags |= hflag_have_hvalue;
    if (hv->_flags & hflag_autorefresh_outstanding)
	hv->set_flags(_w, hv->_flags & ~hflag_autorefresh_outstanding);
    return hv;
}

void handler_values::set_handlers(const String &hname, const String &, const String &hvalue)
{
    assert(hname.length() > 9 && memcmp(hname.end() - 9, ".handlers", 9) == 0);

    handler_value *handlers = _hv.find_force(hname).get();
    if (handlers && handlers->hvalue() == hvalue)
	return;

    Vector<handler_value *> old_handlers;
    for (handler_value *v = handlers; v; v = v->_next) {
	v->_driver_flags |= hflag_dead;
	old_handlers.push_back(v);
    }
    
    handlers->_next = 0;
    
    // parse handler data into _hinfo
    const char *s = hvalue.begin();
    bool syntax_error = false;
    while (s != hvalue.end()) {
	const char *name_start = s;
	while (s != hvalue.end() && !isspace((unsigned char) *s))
	    ++s;
	if (s == name_start || s == hvalue.end() || *s != '\t') {
	    syntax_error = true;
	    break;
	}
	String name = hvalue.substring(name_start, s);

	while (s != hvalue.end() && isspace((unsigned char) *s))
	    ++s;

	int flags = 0;
	for (; s != hvalue.end() && !isspace((unsigned char) *s); ++s)
	    switch (*s) {
	      case 'r':
		flags |= hflag_r;
		break;
	      case 'w':
		flags |= hflag_w;
		break;
	      case '+':
		flags |= hflag_rparam;
		break;
	      case '%':
		flags |= hflag_raw;
		break;
	      case '.':
		flags |= hflag_calm;
		break;
	      case '$':
		flags |= hflag_expensive;
		break;
	      case 'b':
		flags |= hflag_button;
		break;
	      case 'c':
		flags |= hflag_checkbox;
		break;
	    }
	if (!(flags & hflag_r))
	    flags &= ~hflag_rparam;
	if (flags & hflag_r)
	    flags &= ~hflag_button;
	if (flags & hflag_rparam)
	    flags &= ~hflag_checkbox;
	
	// default appearance
	if (name == "class" || name == "name")
	    flags |= hflag_special;
	else if (name == "config")
	    flags |= hflag_multiline | hflag_special;
	else if (name == "ports")
	    flags |= hflag_collapse | hflag_visible;
	else if (name == "handlers")
	    flags |= hflag_collapse;
	else
	    flags |= hflag_visible;
	if (handler_value::default_refreshable(flags))
	    flags |= hflag_refresh;
	
	String full_name = hname.substring(0, hname.length() - 8) + name;
	handler_value *v = _hv.find_force(full_name).get();
	if (v != handlers) {
	    v->_next = handlers->_next;
	    handlers->_next = v;
	}
	v->set_driver_flags(_w, flags);
	if (v->notify_delt())
	    _w->diagram()->notify_read(v);
	
	while (s != hvalue.end() && *s != '\r' && *s != '\n')
	    ++s;
	if (s + 1 < hvalue.end() && *s == '\r' && s[1] == '\n')
	    s += 2;
	else if (s != hvalue.end())
	    ++s;
    }

    for (handler_value **hv = old_handlers.begin(); hv != old_handlers.end();
	 ++hv)
	if ((*hv)->_flags & hflag_dead)
	    _hv.remove((*hv)->_hname);
}

handler_value *handler_values::hard_find_placeholder(const String &hname)
{
    int dot = hname.find_right('.');
    if (dot < 0 || !_w->driver()
	|| find(hname.substring(0, dot + 1) + "handlers"))
	return 0;
    else
	return _hv.find_force(hname).get();
}

}
