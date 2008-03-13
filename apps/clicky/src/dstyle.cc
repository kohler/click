#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "dwidget.hh"
#include "dstyle.hh"
#include "diagram.hh"
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include <clicktool/processingt.hh>
#include <clicktool/elementmap.hh>
#include <list>
#include <math.h>
#include <string.h>
#include "wrouter.hh"
#include "whandler.hh"
extern "C" {
#include "support.h"
}
namespace clicky {

struct colordef {
    const char *name;
    int length;
    uint32_t value;
};

static const colordef colordefs[] = {
    { "aqua", 4, 0x00FFFF },
    { "black", 5, 0x000000 },
    { "blue", 4, 0x0000FF },
    { "fuchsia", 7, 0xFF00FF },
    { "gray", 4, 0x808080 },
    { "green", 5, 0x008000 },
    { "lime", 4, 0x00FF00 },
    { "maroon", 6, 0x800000 },
    { "navy", 4, 0x000080 },
    { "olive", 5, 0x808000 },
    { "orange", 6, 0xFFA500 },
    { "purple", 6, 0x800080 },
    { "red", 3, 0xFF0000 },
    { "silver", 6, 0xC0C0C0 },
    { "teal", 4, 0x008080 },
    { "white", 5, 0xFFFFFF },
    { "yellow", 6, 0xFFFF00 }
};

const double white_color[4] = { 1, 1, 1, 1 };

const char default_css[] = "~port~.input {\n\
    shape: triangle;\n\
    length: 11px;\n\
    width: 7px;\n\
    margin: 100%;\n\
}\n\
~port~.output {\n\
    shape: rectangle;\n\
    length: 9px;\n\
    width: 5.5px;\n\
    margin: 100%;\n\
}\n\
~port~.push, ~port~.push.agnostic {\n\
    color: black;\n\
}\n\
~port~.pull, ~port~.pull.agnostic {\n\
    color: white;\n\
}\n\
~port~.agnostic {\n\
    color: gray;\n\
}\n\
~port~.push, ~port~.pull {\n\
    border: black 1px solid;\n\
}\n\
~port~.agnostic, ~port~.push.agnostic, ~port~.pull.agnostic {\n\
    border: black 1px inset;\n\
}\n\
* {\n\
    background: rgb(100%, 100%, 87%);\n\
    color: black;\n\
    border: 1px solid black;\n\
    orientation-padding: 5.4px;\n\
    padding: 7.2px 12px;\n\
    margin: 12px;\n\
    ports-padding: 4.8px;\n\
    shadow: drop rgba(10%, 10%, 20%, 50%) 3px;\n\
    min-height: 30px;\n\
    orientation: vertical;\n\
    /* height-step: 4px; */\n\
    scale: 100%;\n\
    queue-stripe: 1px solid rgb(87%, 87%, 50%);\n\
    queue-stripe-spacing: 12px;\n\
}\n\
* * {\n\
    background: rgb(98%, 98%, 81%);\n\
}\n\
* * * {\n\
    background: rgb(96%, 96%, 75%);\n\
}\n\
* * * * {\n\
    background: rgb(94%, 94%, 69%);\n\
}\n\
*:hover {\n\
    background: #fffff2;\n\
}\n\
*:active, *:active:hover {\n\
    background: #ffffb4;\n\
    shadow: halo rgba(90%, 20%, 95%, 50%) 3px;\n\
}\n\
*Queue {\n\
    min-width: 17.6px;\n\
    min-height: 49.6px;\n\
    style: queue;\n\
}";


/*****
 *
 */

bool cp_pixel(const String &str, double *v)
{
    double scale = 1;
    const char *s = str.begin(), *send = str.end();
    if (s + 2 < send) {
	if (send[-2] == 'p' && send[-1] == 'x')
	    send -= 2;
	else if (send[-2] == 'i' && send[-1] == 'n')
	    scale = 96, send -= 2;
	else if (send[-2] == 'c' && send[-1] == 'm')
	    scale = 96.0 / 2.54, send -= 2;
	else if (send[-2] == 'm' && send[-1] == 'm')
	    scale = 96.0 / 25.4, send -= 2;
	else if (send[-2] == 'p' && send[-1] == 't')
	    scale = 96.0 / 72.0, send -= 2;
	else if (send[-2] == 'p' && send[-1] == 'c')
	    scale = 96.0 / 6.0, send -= 2;
	/* XXX em, ex? */
    }
    double value;
    if (cp_double(str.substring(s, send), &value)) {
	*v = value * scale;
	return true;
    } else
	return false;
}

bool cp_relative(const String &str, double *v)
{
    if (str && str.back() == '%' && cp_double(str.substring(0, -1), v)) {
	*v /= 100;
	return true;
    } else
	return cp_double(str, v);
}

bool cp_color(const String &str, double *r, double *g, double *b, double *a)
{
    const char *s = str.begin(), *send = str.end();
    if (s == send || (s[0] == '#' && s + 4 != send && s + 7 != send))
	return false;

    // hex specification?
    if (s[0] == '#') {
	uint32_t v = 0;
	for (++s; s != send; ++s)
	    if (*s >= '0' && *s <= '9')
		v = (v << 4) + *s - '0';
	    else if (*s >= 'A' && *s <= 'F')
		v = (v << 4) + *s - 'A' + 10;
	    else if (*s >= 'a' && *s <= 'f')
		v = (v << 4) + *s - 'a' + 10;
	    else
		return false;
	if (str.length() == 4) {
	    *r = (v >> 8) / 15.;
	    *g = ((v >> 4) & 0xF) / 15.;
	    *b = (v & 0xF) / 15.;
	    if (a)
		*a = 1;
	    return true;
	} else {
	    *r = (v >> 16) / 255.;
	    *g = ((v >> 8) & 0xFF) / 255.;
	    *b = (v & 0xFF) / 255.;
	    if (a)
		*a = 1;
	    return true;
	}
    }

    // rgb specification?
    if (s + 5 < send && (memcmp(s, "rgb(", 4) == 0
			 || memcmp(s, "rgba(", 5) == 0)) {
	double x[4];
	int maxi = 3;
	if (s[3] == 'a')
	    ++s, ++maxi;
	s += 4;
	x[3] = 1;
	for (int i = 0; i < maxi; ++i) {
	    s = cp_skip_comment_space(s, send);
	    const char *n = s;
	    while (s != send && (isdigit((unsigned char) *s) || *s == '.'
				 || *s == '+' || *s == '-'))
		++s;
	    if (s == send || n == s)
		return false;
	    x[i] = strtod(n, 0);
	    if (*s == '%') {
		x[i] /= 100.;
		++s;
	    } else if (i < 3)
		x[i] /= 255.;
	    if (x[i] < 0)
		x[i] = 0;
	    else if (x[i] > 1)
		x[i] = 1;
	    if (i < maxi - 1) {
		if (*s != ',')
		    s = cp_skip_comment_space(s, send);
		if (*s != ',')
		    return false;
		++s;
	    }
	}
	s = cp_skip_comment_space(s, send);
	if (s + 1 != send || s[0] != ')')
	    return false;
	*r = x[0];
	*g = x[1];
	*b = x[2];
	if (a)
	    *a = x[3];
	return true;
    }

    // color name?
    const colordef *cl = colordefs;
    const colordef *cr = colordefs + sizeof(colordefs) / sizeof(colordefs[0]);
    while (cl < cr) {
	const colordef *cm = cl + (cr - cl) / 2;
	if (*s == cm->name[0] && s + cm->length == send
	    && memcmp(s, cm->name, cm->length) == 0) {
	    *r = (cm->value >> 16) / 255.;
	    *g = ((cm->value >> 8) & 0xFF) / 255.;
	    *b = (cm->value & 0xFF) / 255.;
	    if (a)
		*a = 1;
	    return true;
	}
	int cmp;
	if (*s < cm->name[0])
	    cmp = -1;
	else if (*s > cm->name[0])
	    cmp = 1;
	else {
	    int len = (send - s < cm->length ? send - s : cm->length);
	    if (!(cmp = memcmp(s, cm->name, len)))
		cmp = (cm->length == len ? 1 : -1);
	}
	if (cmp < 0)
	    cr = cm;
	else
	    cl = cm + 1;
    }
    return false;
}


/*****
 *
 */

inline bool int_match_string(const char *begin, const char *end, int i)
{
    if (i >= 0 && i <= 9)
	return begin + 1 == end && begin[0] == i + '0';
    else {
	int j;
	return cp_integer(begin, end, 10, &j) && j == i;
    }
}

bool dcss_selector::match(const delt *e) const
{
    if (e->root())
	return !_type && !_name && !_klasses.size() && !_highlight_match;
    if (_type && _type != e->type_name())
	if (!_type_glob || !glob_match(e->type_name(), _type))
	    return false;
    if (_name && _name != e->name())
	if (!_name_glob || !glob_match(e->name(), _name))
	    return false;
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	if (k->length() > 3 && (*k)[0] == 'i' && (*k)[1] == 'n'
	    && (*k)[2] == '=') {
	    if (!int_match_string(k->begin() + 3, k->end(), e->ninputs()))
		return false;
	} else if (k->length() > 4 && (*k)[0] == 'o' && (*k)[1] == 'u'
		   && (*k)[2] == 't' && (*k)[3] == '=') {
	    if (!int_match_string(k->begin() + 4, k->end(), e->noutputs()))
		return false;
	} else if (k->equals("primitive", 9)) {
	    if (!e->primitive())
		return false;
	} else if (k->equals("compound", 8)) {
	    if (e->primitive())
		return false;
	} else if (k->equals("anonymous", 9)) {
	    const String &name = e->name(), &type_name = e->type_name();
	    if (name.length() < type_name.length() + 2
		|| name[type_name.length()] != '@'
		|| memcmp(name.begin(), type_name.begin(), type_name.length()))
		return false;
	}
    if ((e->highlights() & _highlight_match) != _highlight)
	return false;
    return true;
}

bool dcss_selector::klasses_match(const Vector<String> &klasses) const
{
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	if (std::find(klasses.begin(), klasses.end(), *k) == klasses.end())
	    return false;
    return true;
}

bool dcss_selector::klasses_match_port(bool isoutput, int port, int processing) const
{
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	if (k->equals("input", 5)) {
	    if (isoutput)
		return false;
	} else if (k->equals("output", 6)) {
	    if (!isoutput)
		return false;
	} else if (k->equals("push", 4)) {
	    if ((processing & ~ProcessingT::VAFLAG) != ProcessingT::VPUSH)
		return false;
	} else if (k->equals("pull", 4)) {
	    if ((processing & ~ProcessingT::VAFLAG) != ProcessingT::VPULL)
		return false;
	} else if (k->equals("agnostic", 8)) {
	    if (processing != ProcessingT::VAGNOSTIC
		&& (processing & ProcessingT::VAFLAG) == 0)
		return false;
	} else
	    return false;
    if (_name && port >= 0
	&& !int_match_string(_name.begin(), _name.end(), port))
	return false;
    return true;
}

const char *dcss_selector::parse(const String &str, const char *s)
{
    _type = _name = String();
    _type_glob = _name_glob = _highlight = _highlight_match = 0;
    _klasses.clear();
    StringAccum sa;

    char start = 1;
    const char *send = str.end();
    while (s != send && *s != '>' && *s != '{' && *s != ',' && *s != '}'
	   && !isspace((unsigned char) *s)) {
	const char *n = s;
	
	if (*n == '[') {
	    s = ++n;
	    const char *last = s;
	    sa.clear();
	    for (; s != send && *s != '\n' && *s != '\r' && *s != '\f'
		     && *s != ']'; ++s)
		if (isspace(*s)) {
		    sa.append(last, s);
		    last = s + 1;
		}
	    if (sa) {
		sa.append(last, s);
		_klasses.push_back(sa.take_string());
	    } else
		_klasses.push_back(str.substring(last, s));
	    if (s == send || *s != ']')
		break;
	    ++s;
	    start = 0;
	    continue;
	}

	if (*n == '.' || *n == '#' || *n == ':') {
	    start = *n;
	    s = ++n;
	} else if (!start)
	    break;

	int glob = 0;
	while (s != send && !isspace((unsigned char) *s) && *s != '.'
	       && *s != '>' && *s != '#' && *s != ':' && *s != ',' && *s != '{'
	       && *s != '}' && *s != '[' && *s != ']'
	       && (*s != '/' || s + 1 != send || (s[1] != '/' && s[1] != '*'))) {
	    if (*s == '*' || *s == '?' || *s == '\\')
		glob = 1;
	    if (*s == '\\' && s + 1 != send)
		++s;
	    ++s;
	}
	if (s == n)
	    break;

	if (n + 1 == s && *n == '*') {
	    n = s;
	    glob = 0;
	}
	
	if (start == '.')
	    _klasses.push_back(str.substring(n, s));
	else if (start == '#') {
	    _name = str.substring(n, s);
	    _name_glob = glob;
	} else if (start == ':') {
	    String x = str.substring(n, s);
	    if (x.equals("hover", 5))
		_highlight |= (1<<dhlt_hover), _highlight_match |= (1<<dhlt_hover);
	    else if (x.equals("active", 6))
		_highlight |= (1<<dhlt_click), _highlight_match |= (1<<dhlt_click);
	    else
		_klasses.push_back(x);
	} else {
	    _type = str.substring(n, s);
	    _type_glob = glob;
	}
	start = 0;
    }

    return s;
}

String dcss_selector::unparse() const
{
    StringAccum sa;
    if (_type)
	sa << _type;
    if (_name)
	sa << '#' << _name;
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	sa << '.' << *k;
    if (!sa)
	sa << '*';
    return sa.take_string();
}


/*****
 *
 */

const dcss_property dcss_property::null_property("", "");
const double dcss_property::transparent_color[4] = { 0, 0, 0, 1 };

bool dcss_property::hard_change_type(int t) const
{
    if (_t == t_color)
	delete[] _v.dp;
    
    _t = t_none;
    if (!_vstr)
	return t == t_none;
    
    switch (t) {

      case t_numeric:
	if (cp_double(_vstr, &_v.d))
	    _t = t_numeric;
	break;

      case t_pixel:
	if (cp_pixel(_vstr, &_v.d))
	    _t = t_pixel;
	break;

      case t_relative:
	if (cp_relative(_vstr, &_v.d))
	    _t = t_relative;
	break;

      case t_color: {
	  double c[4];
	  if (cp_color(_vstr, &c[0], &c[1], &c[2], &c[3])) {
	      _v.dp = new double[4];
	      memcpy(_v.dp, &c, sizeof(double) * 4);
	      _t = t_color;
	  }
	  break;
      }

      case t_border_style:
	if (_vstr.equals("none", 4)) {
	    _v.i = dborder_none;
	    _t = t_border_style;
	} else if (_vstr.equals("solid", 5)) {
	    _v.i = dborder_solid;
	    _t = t_border_style;
	} else if (_vstr.equals("inset", 5)) {
	    _v.i = dborder_inset;
	    _t = t_border_style;
	} else if (_vstr.equals("dashed", 6)) {
	    _v.i = dborder_dashed;
	    _t = t_border_style;
	} else if (_vstr.equals("dotted", 6)) {
	    _v.i = dborder_dotted;
	    _t = t_border_style;
	}
	break;

      case t_shadow_style:
	if (_vstr.equals("none", 4)) {
	    _v.i = dshadow_none;
	    _t = t_shadow_style;
	} else if (_vstr.equals("drop", 4)) {
	    _v.i = dshadow_drop;
	    _t = t_shadow_style;
	} else if (_vstr.equals("halo", 4)) {
	    _v.i = dshadow_halo;
	    _t = t_shadow_style;
	}
	break;

    }
    
    return _t == t;
}

bool dcss_property::hard_change_relative_pixel() const
{
    if (_t == t_color)
	delete[] _v.dp;
	
    _t = t_none;
    if (!_vstr)
	return false;

    if (_vstr.back() != '%' && !isalpha((unsigned char) _vstr[0]))
	if (cp_pixel(_vstr, &_v.d)) {
	    _t = t_pixel;
	    return true;
	}

    if (cp_relative(_vstr, &_v.d)) {
	_t = t_relative;
	return true;
    } else
	return false;
}


/*****
 *
 */

dcss::dcss()
    : _sorted(false), _next(0)
{
}

static bool operator<(const dcss_property &a, const dcss_property &b)
{
    return a.name() < b.name();
}

void dcss::sort() const
{
    std::sort(_de.begin(), _de.end());
    _sorted = true;
}

const dcss_property *dcss::find(PermString name) const
{
    if (_sorted) {
	dcss_property *l = _de.begin(), *r = _de.end();
	while (l < r) {
	    dcss_property *m = l + (r - l) / 2;
	    if (m->name() == name)
		return m;
	    else if (m->name() < name)
		l = m + 1;
	    else
		r = m;
	}
    } else
	for (dcss_property *p = _de.begin(); p != _de.end(); ++p)
	    if (p->name() == name)
		return p;
    return 0;
}

void dcss::add(PermString name, const String &value)
{
    if (dcss_property *dx = find(name))
	*dx = dcss_property(name, value);
    else {
	_sorted = false;
	_de.push_back(dcss_property(name, value));
    }
}

bool dcss::hard_match_context(const delt *e) const
{
    const dcss_selector *d = _context.end();
    if (e)
	for (; d != _context.begin() && e->parent(); e = e->parent())
	    if (d[-1].match(e))
		--d;
    return d == _context.begin();
}

const char *dcss::parse(const String &str, const char *s)
{
    const char *send = str.end();
    while (1) {
	s = cp_skip_comment_space(s, send);
	const char *n = s;
	while (s != send && !isspace((unsigned char) *s)
	       && *s != ':' && *s != ';' && *s != '{' && *s != '}'
	       && (*s != '/' || s + 1 == send || (s[1] != '/' && s[1] != '*')))
	    ++s;
	const char *n_end = s;
	s = cp_skip_comment_space(s, send);
	if (s == send || *s == '{' || *s == '}')
	    break;
	else if (*s == ';') {
	    ++s;
	    continue;
	}
	// have n..n_end ':'
	++s;			// skip ':'
	s = cp_skip_comment_space(s, send);
	const char *v = s;
	const char *v_ew0 = s;
	const char *v_ew1 = s;
	for (; s != send; ++s)
	    if (*s == '\'' || *s == '\"') {
		int quote = *s;
		for (++s; s != send; ++s)
		    if (*s == '\\') {
			if (s + 1 != send && s[1] == '\r')
			    s += (s + 2 != send && s[2] == '\n' ? 2 : 1);
			else
			    ++s;
		    } else if (*s == quote)
			break;
		    else if (*s == '\n' || *s == '\r' || *s == '\f')
			goto unexpected_end_of_string;
		v_ew0 = s + 1;
	    } else if (*s == ';' || *s == '{' || *s == '}' || *s == ':')
		break;
	    else if (*s == '/' && s + 1 != send && (s[1] == '/' || s[1] == '*')) {
		if (v_ew0 == s)
		    v_ew1 = v_ew0;
		s = cp_skip_comment_space(s, send) - 1;
	    } else if (!isspace((unsigned char) *s))
		v_ew0 = s + 1;
	    else if (v_ew0 == s)
		v_ew1 = v_ew0;

	if (s != send && *s == ':')
	    v_ew0 = v_ew1;

	if (n == n_end || v == v_ew0)
	    /* do nothing */;
	else if (n + 6 == n_end && memcmp(n, "border", 6) == 0)
	    parse_border(str, v, v_ew0, "border");
	else if (n + 12 == n_end && memcmp(n, "queue-stripe", 12) == 0)
	    parse_border(str, v, v_ew0, "queue-stripe");
	else if (n + 6 == n_end && memcmp(n, "shadow", 6) == 0)
	    parse_shadow(str, v, v_ew0);
	else if (n + 10 == n_end && memcmp(n, "background", 10) == 0)
	    parse_background(str, v, v_ew0);
	else if (n + 6 == n_end && memcmp(n, "margin", 6) == 0)
	    parse_box(str, v, v_ew0, "margin");
	else if (n + 7 == n_end && memcmp(n, "padding", 7) == 0)
	    parse_box(str, v, v_ew0, "padding");
	else
	    add(str.substring(n, n_end), str.substring(v, v_ew0));

	s = v_ew0;
	if (s == send)
	    break;
      unexpected_end_of_string:
	;
    }

    return s;
}

void dcss::parse_border(const String &str, const char *s, const char *send,
			const String &prefix)
{
    double d;
    
    while (s != send) {
	s = cp_skip_comment_space(s, send);
	const char *n = s;
	while (s != send && !isspace((unsigned char) *s)
	       && (*s != '/' || s + 1 == send || (s[1] != '/' && s[1] != '*')))
	    if (*s == '(') {
		for (++s; s != send && *s != ')'; ++s)
		    /* */;
		if (s != send)
		    ++s;
	    } else
		++s;

	if ((n + 4 == s && memcmp(n, "none", 4) == 0)
	    || (n + 5 == s && memcmp(n, "solid", 5) == 0)
	    || (n + 5 == s && memcmp(n, "inset", 5) == 0)
	    || (n + 6 == s && memcmp(n, "dashed", 6) == 0)
	    || (n + 6 == s && memcmp(n, "dotted", 6) == 0))
	    add(prefix + "-style", str.substring(n, s));
	else if (n < s && (isdigit((unsigned char) *n) || *n == '+' || *n == '.')
		 && cp_pixel(str.substring(n, s), &d))
	    add(prefix + "-width", str.substring(n, s));
	else if (cp_color(str.substring(n, s), &d, &d, &d, &d))
	    add(prefix + "-color", str.substring(n, s));
    }
}

void dcss::parse_shadow(const String &str, const char *s, const char *send)
{
    double d;
    
    while (s != send) {
	s = cp_skip_comment_space(s, send);
	const char *n = s;
	while (s != send && !isspace((unsigned char) *s)
	       && (*s != '/' || s + 1 == send || (s[1] != '/' && s[1] != '*')))
	    if (*s == '(') {
		for (++s; s != send && *s != ')'; ++s)
		    /* */;
		if (s != send)
		    ++s;
	    } else
		++s;

	if ((n + 4 == s && memcmp(n, "none", 4) == 0)
	    || (n + 4 == s && memcmp(n, "drop", 4) == 0)
	    || (n + 4 == s && memcmp(n, "halo", 4) == 0))
	    add(String::stable_string("shadow-style"), str.substring(n, s));
	else if (n < s && (isdigit((unsigned char) *n) || *n == '+' || *n == '.')
		 && cp_pixel(str.substring(n, s), &d))
	    add(String::stable_string("shadow-width"), str.substring(n, s));
	else if (cp_color(str.substring(n, s), &d, &d, &d, &d))
	    add(String::stable_string("shadow-color"), str.substring(n, s));
    }
}

void dcss::parse_background(const String &str, const char *s, const char *send)
{
    double d;
    
    while (s != send) {
	s = cp_skip_comment_space(s, send);
	const char *n = s;
	while (s != send && !isspace((unsigned char) *s)
	       && (*s != '/' || s + 1 == send || (s[1] != '/' && s[1] != '*')))
	    if (*s == '(') {
		for (++s; s != send && *s != ')'; ++s)
		    /* */;
		if (s != send)
		    ++s;
	    } else
		++s;

	if (cp_color(str.substring(n, s), &d, &d, &d, &d))
	    add(String::stable_string("background-color"), str.substring(n, s));
    }
}

void dcss::parse_box(const String &str, const char *s, const char *send, const String &prefix)
{
    double d;
    String x[4];
    int pos = 0;
    
    while (s != send) {
	s = cp_skip_comment_space(s, send);
	const char *n = s;
	while (s != send && !isspace((unsigned char) *s)
	       && (*s != '/' || s + 1 == send || (s[1] != '/' && s[1] != '*')))
	    ++s;

	if (pos < 4 && (cp_pixel(str.substring(n, s), &d)
			|| cp_relative(str.substring(n, s), &d)))
	    x[pos++] = str.substring(n, s);
	else
	    break;
    }

    if (pos == 1)
	x[1] = x[0], ++pos;
    if (pos == 2)
	x[2] = x[0], ++pos;
    if (pos == 3)
	x[3] = x[1], ++pos;
    if (pos == 4) {
	add(prefix + "-top", x[0]);
	add(prefix + "-right", x[1]);
	add(prefix + "-bottom", x[2]);
	add(prefix + "-left", x[3]);
    }
}

String dcss::unparse_selector() const
{
    if (!_context.size())
	return _selector.unparse();
    else {
	StringAccum sa;
	for (const dcss_selector *sp = _context.end(); sp != _context.begin(); )
	    sa << (--sp)->unparse() << ' ';
	sa << _selector.unparse();
	return sa.take_string();
    }
}

static bool dcsspp_compare(dcss *a, dcss *b)
{
    return *b < *a;
}

static bool propmatchp_compare(dcss_propmatch *a, dcss_propmatch *b)
{
    return a->name < b->name;
}

int dcss::assign(dcss_propmatch **begin, dcss_propmatch **end) const
{
    if (!_sorted)
	sort();
    int assigned = 0;
    for (dcss_property *p = _de.begin(); p != _de.end() && begin != end; )
	if (p->name() == (*begin)->name) {
	    if (!(*begin)->property) {
		(*begin)->property = p;
		++assigned;
	    }
	    ++begin, ++p;
	} else if (p->name() < (*begin)->name)
	    ++p;
	else {
	    dcss_propmatch **l = begin, **r = end;
	    while (l != r) {
		dcss_propmatch **m = l + (r - l) / 2;
		if (p->name() == (*m)->name)
		    l = r = m;
		else if (p->name() < (*m)->name)
		    r = m;
		else
		    l = m + 1;
	    }
	    begin = l;
	}
    return assigned;
}

void dcss::assign_all(dcss_propmatch *props, dcss_propmatch **pp, int n,
		      dcss **begin, dcss **end)
{
    if (!*pp) {
	for (int i = 0; i < n; ++i)
	    pp[i] = props + i;
	std::sort(pp, pp + n, propmatchp_compare);
    }
    for (int i = 0; i < n; ++i)
	pp[i]->property = 0;

    int done;
    for (done = 0; begin != end && done != n; ++begin)
	done += (*begin)->assign(pp, pp + n);

    if (done != n)
	for (int i = 0; i < n; ++i)
	    if (!pp[i]->property)
		pp[i]->property = &dcss_property::null_property;
}


/*****
 *
 */

dcss_set::dcss_set(dcss_set *below)
    : _below(below), _frozen(false)
{
    if (_below)
	_selector_index = _below->_selector_index + 100000;
    else
	_selector_index = 0;
    _s.push_back(0);
    _s.push_back(0);
    _all_generic_ports = (below ? below->_all_generic_ports : true);
}

void dcss_set::mark_change()
{
    if (_frozen) {
	for (int i = 0; i < 14; ++i)
	    _generic_port_styles[i] = ref_ptr<dport_style>();
	for (int i = 0; i < 16; ++i)
	    _generic_elt_styles[i] = ref_ptr<delt_style>();
	_etable.clear();
	_frozen = false;
    }
}

dcss_set::~dcss_set()
{
    for (dcss **s = _s.begin(); s != _s.end(); ++s)
	while (*s) {
	    dcss *next = (*s)->_next;
	    delete *s;
	    *s = next;
	}
    mark_change();
}

void dcss_set::parse(const String &str)
{
    const char *s = str.begin(), *send = str.end();
    
    while (1) {
	s = cp_skip_comment_space(s, send);
	Vector<dcss *> cs;
	if (s == send)
	    return;

	int nsel = 0;
	while (s != send && *s != '{') {
	    dcss_selector sel;
	    const char *n = s;
	    s = sel.parse(str, s);
	    if (s == n)
		goto skip_braces;
	    if (nsel == 0)
		cs.push_back(new dcss);
	    else
		cs.back()->_context.push_back(cs.back()->_selector);
	    cs.back()->_selector = sel;
	    ++nsel;

	    if (s != send && isspace((unsigned char) *s))
		s = cp_skip_comment_space(s, send);
	    if (s != send && *s == ',') {
		s = cp_skip_comment_space(s + 1, send);
		nsel = 0;
	    } else if (s != send && *s == '>')
		s = cp_skip_comment_space(s + 1, send);
	}

	if (cs.size() && s != send && *s == '{') {
	    const char *n = s + 1;
	    s = cs[0]->parse(str, n);
	    if (cs[0]->_de.size()) {
		for (dcss **cspp = cs.begin() + 1; cspp != cs.end(); ++cspp)
		    (*cspp)->parse(str, n);
		for (dcss **cspp = cs.begin(); cspp != cs.end(); ++cspp)
		    add(*cspp);
		cs.clear();
	    }
	}

      skip_braces:
	while (s != send)
	    if (*s == '/' && s + 1 != send && (s[1] == '/' || s[1] == '*'))
		s = cp_skip_comment_space(s, send);
	    else if (*s == '\"' || *s == '\'') {
		char quote = *s;
		for (++s; s != send && *s != quote; ++s)
		    if (*s == '\\') {
			if (s + 2 < send && s[1] == '\r' && s[2] == '\n')
			    s += 2;
			else if (s + 1 != send)
			    ++s;
		    } else if (*s == '\n' || *s == '\r' || *s == '\f')
			break;
		if (s != send)
		    ++s;
	    } else if (*s == '}') {
		++s;
		break;
	    } else
		++s;
	
	for (dcss **cspp = cs.begin(); cspp != cs.end(); ++cspp)
	    delete *cspp;
    }
}

void dcss_set::add(dcss *s)
{
    s->_selector_index = _selector_index;
    ++_selector_index;
    
    int si;
    if (!s->type() || s->type()[0] == '*' || s->type()[0] == '?'
	|| s->type()[0] == '[' || s->type()[0] == '\\')
	si = 0;
    else if (s->type().equals("~port~", 6))
	si = 1;
    else
	for (si = 2; si < _s.size(); ++si)
	    if (_s[si]->type()[0] == s->type()[0])
		break;
    if (si == _s.size())
	_s.push_back(0);
    s->_next = _s[si];
    _s[si] = s;

    if (_frozen)
	mark_change();
    if (si == 1 && (!s->selector().generic_port() || s->has_context()))
	_all_generic_ports = false;
}

dcss_set *dcss_set::default_set()
{
    static dcss_set *x;
    if (!x) {
	x = new dcss_set(0);
	x->parse(default_css);
    }
    return x;
}


static PermString::Initializer initializer;

static dcss_propmatch port_pm[] = {
    { "shape", 0 },
    { "length", 0 },
    { "width", 0 },
    { "color", 0 },
    { "border-style", 0 },
    { "border-width", 0 },
    { "border-color", 0 },
    { "margin-top", 0 },
    { "margin-right", 0 },
    { "margin-bottom", 0 },
    { "margin-left", 0 }
};

enum {
    num_port_pm = sizeof(port_pm) / sizeof(port_pm[0])
};

static dcss_propmatch *port_pmp[num_port_pm];

void dcss_set::collect_port_styles(const delt *e, bool isoutput, int port,
				   int processing, Vector<dcss *> &result,
				   int &generic)
{
    for (dcss *s = _s[1]; s; s = s->_next) {
	if (!s->match_context(e))
	    continue;
	if (s->selector().match_port(isoutput, port, processing)) {
	    if (!s->selector().generic_port())
		generic = 0;
	    else if (s->has_context() && generic >= 1)
		generic = 1;
	    result.push_back(s);
	} else if (!s->selector().generic_port() && generic >= 2
		   && s->selector().match_port(isoutput, -1, processing))
	    generic = 1;
    }
    if (_below)
	_below->collect_port_styles(e, isoutput, port, processing,
				    result, generic);
}

ref_ptr<dport_style> dcss_set::hard_port_style(const delt *e, bool isoutput,
					       int port, int processing)
{
    Vector<dcss *> sv;
    int generic = 2;
    collect_port_styles(e, isoutput, port, processing, sv, generic);

    if (generic >= 2 && _generic_port_styles[7*isoutput + processing])
	return _generic_port_styles[7*isoutput + processing];

    dport_style *sty = new dport_style;

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    dcss::assign_all(port_pm, port_pmp, num_port_pm, sv.begin(), sv.end());
    
    String s = port_pm[0].vstring("shape");
    sty->shape = (s.equals("triangle", 8) ? dpshape_triangle : dpshape_rectangle);
    sty->length = port_pm[1].vpixel("length");
    sty->width = port_pm[2].vpixel("width");
    port_pm[3].vcolor(sty->color, "color");
    sty->border_style = port_pm[4].vborder_style("border-style");
    sty->border_width = port_pm[5].vpixel("border-width");
    port_pm[6].vcolor(sty->border_color, "border-color");
    if (sty->border_color[3] == 0 || sty->border_width <= 0)
	sty->border_style = dborder_none;
    sty->margin[0] = port_pm[7].vrelative("margin-top");
    sty->margin[1] = port_pm[8].vrelative("margin-right");
    sty->margin[2] = port_pm[9].vrelative("margin-bottom");
    sty->margin[3] = port_pm[10].vrelative("margin-left");
    sty->uniform_style = generic >= 1;

    if (generic >= 2) {
	_frozen = true;
	_generic_port_styles[7*isoutput + processing] = ref_ptr<dport_style>(sty);
	return _generic_port_styles[7*isoutput + processing];
    } else
	return ref_ptr<dport_style>(sty);
}


/*****
 *
 */

static dcss_propmatch elt_pm[] = {
    { "color", 0 },
    { "background-color", 0 },
    { "border-style", 0 },
    { "border-width", 0 },
    { "border-color", 0 },
    { "shadow-style", 0 },
    { "shadow-width", 0 },
    { "shadow-color", 0 },
    { "padding-top", 0 },
    { "padding-right", 0 },
    { "padding-bottom", 0 },
    { "padding-left", 0 },
    { "orientation-padding", 0 },
    { "ports-padding", 0 },
    { "min-width", 0 },
    { "min-height", 0 },
    { "height-step", 0 },
    { "orientation", 0 },
    { "style", 0 },
    { "scale", 0 },
    { "margin-top", 0 },
    { "margin-right", 0 },
    { "margin-bottom", 0 },
    { "margin-left", 0 }
};

enum {
    num_elt_pm = sizeof(elt_pm) / sizeof(elt_pm[0])
};

static dcss_propmatch *elt_pmp[num_elt_pm];

void dcss_set::collect_elt_styles(const delt *e, Vector<dcss *> &result,
				  bool &generic) const
{
    if (!e->root()) {
	for (dcss * const *sp = _s.begin() + 2; sp != _s.end(); ++sp)
	    if ((*sp)->type()[0] == e->type_name()[0])
		for (dcss *s = *sp; s; s = s->_next)
		    if (s->selector().match(e) && s->match_context(e->parent())) {
			generic = false;
			result.push_back(s);
		    }
    }
    for (dcss *s = _s[0]; s; s = s->_next)
	if (s->selector().match(e) && s->match_context(e->parent())) {
	    if (!s->selector().generic_elt() || s->has_context())
		generic = false;
	    result.push_back(s);
	}
    if (_below)
	_below->collect_elt_styles(e, result, generic);
}

ref_ptr<delt_style> dcss_set::elt_style(const delt *e)
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_elt_styles(e, sv, generic);

    if (generic && _generic_elt_styles[e->highlights() & 7])
	return _generic_elt_styles[e->highlights() & 7];

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<delt_style> &style_cache = _etable.find_force(sa.take_string());

    if (!style_cache) {
	dcss::assign_all(elt_pm, elt_pmp, num_elt_pm, sv.begin(), sv.end());

	double scale = elt_pm[19].vrelative("scale");

	delt_style *sty = new delt_style;
	elt_pm[0].vcolor(sty->color, "color");
	elt_pm[1].vcolor(sty->background_color, "background-color");
	sty->border_style = elt_pm[2].vborder_style("border-style");
	sty->border_width = elt_pm[3].vpixel("border-width", this, e) * scale;
	elt_pm[4].vcolor(sty->border_color, "border-color");
	if (sty->border_color[3] == 0 || sty->border_width <= 0)
	    sty->border_style = dborder_none;
	sty->shadow_style = elt_pm[5].vshadow_style("shadow-style");
	sty->shadow_width = elt_pm[6].vpixel("shadow-width", this, e) * scale;
	elt_pm[7].vcolor(sty->shadow_color, "shadow-color");
	if (sty->shadow_color[3] == 0 || sty->shadow_width <= 0)
	    sty->shadow_style = dshadow_none;
	sty->padding[0] = elt_pm[8].vpixel("padding-top", this, e) * scale;
	sty->padding[1] = elt_pm[9].vpixel("padding-right", this, e) * scale;
	sty->padding[2] = elt_pm[10].vpixel("padding-bottom", this, e) * scale;
	sty->padding[3] = elt_pm[11].vpixel("padding-left", this, e) * scale;
	sty->orientation_padding = elt_pm[12].vpixel("orientation-padding", this, e) * scale;
	sty->ports_padding = elt_pm[13].vpixel("ports-padding", this, e) * scale;
	sty->min_width = elt_pm[14].vpixel("min-width", this, e) * scale;
	sty->min_height = elt_pm[15].vpixel("min-height", this, e) * scale;
	sty->height_step = elt_pm[16].vpixel("height-step", this, e) * scale;
	String s = elt_pm[17].vstring("orientation");
	sty->orientation = 0;
	if (s.find_left("horizontal") >= 0)
	    sty->orientation = (sty->orientation + 3) & 3;
	if (s.find_left("reverse") >= 0)
	    sty->orientation ^= 2;
	s = elt_pm[18].vstring("style");
	sty->style = (s.equals("queue", 5) ? destyle_queue : destyle_normal);
	sty->margin[0] = elt_pm[20].vpixel("margin-top", this, e) * scale;
	sty->margin[1] = elt_pm[21].vpixel("margin-right", this, e) * scale;
	sty->margin[2] = elt_pm[22].vpixel("margin-bottom", this, e) * scale;
	sty->margin[3] = elt_pm[23].vpixel("margin-left", this, e) * scale;

	style_cache = ref_ptr<delt_style>(sty);
	if (generic) {
	    _frozen = true;
	    _generic_elt_styles[e->highlights() & 7] = style_cache;
	}
    }
    
    return style_cache;
}


static dcss_propmatch queue_pm[] = {
    { "queue-stripe-style", 0 },
    { "queue-stripe-width", 0 },
    { "queue-stripe-color", 0 },
    { "queue-stripe-spacing", 0 }
};

enum {
    num_queue_pm = sizeof(queue_pm) / sizeof(queue_pm[0])
};

static dcss_propmatch *queue_pmp[num_queue_pm];

ref_ptr<dqueue_style> dcss_set::queue_style(const delt *e)
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_elt_styles(e, sv, generic);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<dqueue_style> &style_cache = _qtable.find_force(sa.take_string());

    if (!style_cache) {
	dcss::assign_all(queue_pm, queue_pmp, num_queue_pm, sv.begin(), sv.end());

	dqueue_style *sty = new dqueue_style;
	sty->queue_stripe_style = queue_pm[0].vborder_style("queue-stripe-style");
	sty->queue_stripe_width = queue_pm[1].vpixel("queue-stripe-width", this, e);
	queue_pm[2].vcolor(sty->queue_stripe_color, "queue-stripe-color");
	sty->queue_stripe_spacing = queue_pm[3].vpixel("queue-stripe-spacing", this, e);

	style_cache = ref_ptr<dqueue_style>(sty);
    }
    
    return style_cache;
}


double dcss_set::vpixel(PermString name, const delt *e) const
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_elt_styles(e, sv, generic);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);

    dcss_propmatch pm = { name, 0 }, *pmp = &pm;
    dcss::assign_all(&pm, &pmp, 1, sv.begin(), sv.end());
    
    return pm.vpixel(name.c_str(), this, name, e->parent());
}

}

#include <click/vector.cc>
