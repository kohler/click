#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "dstyle.hh"
#include "dwidget.hh"
#include "wdiagram.hh"
#include "hvalues.hh"
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include <clicktool/processingt.hh>
#include <clicktool/elementmap.hh>
#include <list>
#include <math.h>
#include <string.h>
#include "crouter.hh"
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
    { "none", 4, 0xFF000000 },
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

const char default_css[] = "port.input {\n\
    port-shape: triangle;\n\
    port-length: 11px;\n\
    port-width: 7px;\n\
    port-margin: 1px;\n\
    port-edge-padding: 2px;\n\
}\n\
port.output {\n\
    port-shape: rectangle;\n\
    port-length: 9px;\n\
    port-width: 5.5px;\n\
    port-margin: 1px;\n\
    port-edge-padding: 2px;\n\
}\n\
port.push, port.push.agnostic {\n\
    port-color: black;\n\
}\n\
port.pull, port.pull.agnostic {\n\
    port-color: white;\n\
}\n\
port.agnostic {\n\
    port-color: gray;\n\
}\n\
port.push, port.pull {\n\
    port-border: black 1px solid;\n\
}\n\
port.agnostic, port.push.agnostic, port.pull.agnostic {\n\
    port-border: black 1px inset;\n\
}\n\
port.push.error, port.push.agnostic.error {\n\
    port-color: red;\n\
}\n\
port.pull.error, port.pull.agnostic.error {\n\
    port-color: rgb(100%, 75%, 75%);\n\
}\n\
port.push.error, port.pull.error {\n\
    port-border: red 1px solid;\n\
}\n\
port.agnostic.error, port.push.agnostic.error, port.pull.agnostic.error {\n\
    port-border: red 1px inset;\n\
}\n\
* {\n\
    background: rgb(100%, 100%, 87%);\n\
    color: black;\n\
    border: 1px solid black;\n\
    padding: 8px 12px;\n\
    margin: 20px 14px;\n\
    shadow: drop rgba(50%, 50%, 45%, 50%) 3px;\n\
    orientation: vertical;\n\
    /* min-height: 30px; */\n\
    /* height-step: 4px; */\n\
    scale: 100%;\n\
    queue-stripe: 1px solid rgb(87%, 87%, 50%);\n\
    queue-stripe-spacing: 12px;\n\
    text: \"%n\\n<small>%c</small>\";\n\
    display: normal;\n\
    port-display: both;\n\
    port-font: 6;\n\
    @media print {\n\
	font: Times;\n\
	port-font: Times 6;\n\
    }\n\
    /* @media screen { font: URW Palladio L italic 20; } */\n\
}\n\
*.anonymous {\n\
    text: \"%n\";\n\
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
@media screen {\n\
*:hover {\n\
    background: #fffff2;\n\
}\n\
*:active, *:active:hover {\n\
    background: #ffff94;\n\
    shadow: unscaled-outline rgba(90%, 20%, 95%, 50%) 3px;\n\
}\n\
*:pressed, *:active:pressed {\n\
    shadow: drop rgba(50%, 50%, 45%, 50%) 3px;\n\
}\n\
}\n\
*Queue {\n\
    min-length: 49.6px;\n\
    style: queue;\n\
}\n\
ClickyInfo {\n\
    display: none;\n\
}\n\
fullness {\n\
    style: fullness;\n\
    length: length;\n\
    capacity: capacity;\n\
    color: rgba(0%, 0%, 100%, 20%);\n\
    autorefresh: length 0.1s;\n\
}\n\
activity {\n\
    style: activity;\n\
    handler: count;\n\
    decay: 0.2s;\n\
    max-value: 1000;\n\
    type: rate;\n\
    autorefresh: 0.1s;\n\
    color: none, red;\n\
}";


/*****
 * property definitions
 */

static PermString::Initializer permstring_initializer;
static HashTable<PermString, int> property_map;

static dcss_propmatch port_pm[] = {
    { "port-shape", 0 },
    { "port-length", 0 },
    { "port-width", 0 },
    { "port-color", 0 },
    { "port-border-style", 0 },
    { "port-border-width", 0 },
    { "port-border-color", 0 },
    { "port-margin-top", 0 },
    { "port-margin-right", 0 },
    { "port-margin-bottom", 0 },
    { "port-margin-left", 0 },
    { "port-edge-padding", 0 },
    { "port-display", 0 },
    { "port-text", 0 },
    { "port-font", 0 }
};

static dcss_propmatch elt_pm[] = {
    { "color", 0 },
    { "background-color", 0 },
    { "border-style", 0 },
    { "border-color", 0 },
    { "shadow-style", 0 },
    { "shadow-width", 0 },
    { "shadow-color", 0 },
    { "style", 0 },
    { "text", 0 },
    { "display", 0 },
    { "font", 0 },
    { "decorations", 0 },
    { "queue-stripe-style", 0 },
    { "queue-stripe-width", 0 },
    { "queue-stripe-color", 0 },
    { "port-split", 0 },
    { "flow-split", 0 }
};

static dcss_propmatch elt_size_pm[] = {
    { "border-width", 0 },
    { "padding-top", 0 },
    { "padding-right", 0 },
    { "padding-bottom", 0 },
    { "padding-left", 0 },
    { "min-width", 0 },
    { "min-height", 0 },
    { "height-step", 0 },
    { "margin-top", 0 },
    { "margin-right", 0 },
    { "margin-bottom", 0 },
    { "margin-left", 0 },
    { "queue-stripe-spacing", 0 },
    { "scale", 0 },
    { "orientation", 0 },
    { "min-length", 0 }
};

static dcss_propmatch handler_pm[] = {
    { "autorefresh", 0 },
    { "autorefresh-period", 0 },
    { "display", 0 },
    { "allow-refresh", 0 }
};

static dcss_propmatch fullness_pm[] = {
    { "length", 0 },
    { "capacity", 0 },
    { "color", 0 },
    { "autorefresh", 0 },
    { "autorefresh-period", 0 }
};

static dcss_propmatch activity_pm[] = {
    { "handler", 0 },
    { "color", 0 },
    { "autorefresh", 0 },
    { "autorefresh-period", 0 },
    { "type", 0 },
    { "max-value", 0 },
    { "min-value", 0 },
    { "decay", 0 }
};

enum {
    num_port_pm = sizeof(port_pm) / sizeof(port_pm[0]),
    pflag_port = 1,

    num_elt_pm = sizeof(elt_pm) / sizeof(elt_pm[0]),
    pflag_elt = 2,

    num_elt_size_pm = sizeof(elt_size_pm) / sizeof(elt_size_pm[0]),
    pflag_elt_size = 4,

    num_handler_pm = sizeof(handler_pm) / sizeof(handler_pm[0]),
    pflag_handler = 8,

    num_fullness_pm = sizeof(fullness_pm) / sizeof(fullness_pm[0]),
    pflag_fullness = 16,

    num_activity_pm = sizeof(activity_pm) / sizeof(activity_pm[0]),
    pflag_activity = 32,

    pflag_no_below = 64
};


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
	if (s[3] == 'a')
	    ++s;
	s += 4;
	x[3] = 1;
	for (int i = 0; i < 4; ++i) {
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
	    if (i < 3) {
		if (s != send && *s != ',')
		    s = cp_skip_comment_space(s, send);
		if (s != send && *s == ')' && i >= 2)
		    break;
		else if (s != send && *s == ',')
		    ++s;
		else if (s != send)
		    return false;
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
	    *r = ((cm->value >> 16) & 0xFF) / 255.;
	    *g = ((cm->value >> 8) & 0xFF) / 255.;
	    *b = (cm->value & 0xFF) / 255.;
	    if (a)
		*a = (255 - (cm->value >> 24)) / 255.;
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

static const char *ccss_skip_braces(const char *s, const char *send,
				    bool one_word = false)
{
    int nopen = 0;
    for (; s != send; ++s)
	if (*s == '/' && s + 1 != send && (s[1] == '/' || s[1] == '*'))
	    s = cp_skip_comment_space(s, send) - 1;
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
	    if (s == send)
		break;
	    if (one_word && !nopen) {
		++s;
		break;
	    }
	} else if (*s == '}' && --nopen < 0)
	    return s + 1;
	else if (*s == '{')
	    ++nopen;
	else if (one_word && (isspace((unsigned char) *s) || *s == ';'))
	    break;
    return s;
}

static String ccss_pop_commavec(String &str)
{
    const char *s = str.begin(), *send = str.end();
    int parens = 0, quotes = 0;
    const char *sarg = cp_skip_comment_space(s, send);
    const char *last_nonsp = sarg - 1;
    for (; s != send; ++s) {
	if (*s == ',' && !parens && !quotes)
	    break;
	if (!isspace((unsigned char) *s))
	    last_nonsp = s;
	if (*s == '\"')
	    quotes = !quotes;
	else if (*s == '\\' && s + 1 != send)
	    ++s;
	else if (*s == '(' && !quotes)
	    ++parens;
	else if (*s == ')' && !quotes && parens)
	    --parens;
    }
    String arg = str.substring(sarg, last_nonsp + 1);
    if (s != send)
	s = cp_skip_comment_space(s + 1, send);
    str = str.substring(s, send);
    return arg;
}


/*****
 *
 */

inline bool comparison_char(char c)
{
    return c == '=' || c == '!' || c == '>' || c == '<';
}

inline bool int_match_string(const char *begin, const char *end, int x,
			     bool require_comparator = true)
{
    int comparator = *begin++;
    if (begin < end && *begin == '=') {
	if (comparator == '>')
	    comparator = 'G';
	else if (comparator == '<')
	    comparator = 'L';
	++begin;
    } else if (comparator == '!')
	return false;
    else if (!require_comparator && isdigit((unsigned char) comparator)) {
	comparator = '=';
	--begin;
    }

    int i;
    if (begin + 1 == end)
	i = *begin - '0';
    else if (!cp_integer(begin, end, 10, &i))
	return false;

    switch (comparator) {
    case '=':
	return x == i;
    case '!':
	return x != i;
    case '>':
	return x > i;
    case '<':
	return x < i;
    case 'G':
	return x >= i;
    case 'L':
	return x <= i;
    default:
	return false;
    }
}

bool dcss_selector::match(crouter *cr, const delt *e, int *sensitivity) const
{
    bool answer = true;
    int senses = 0;

    if (e->root())
	return false;

    if (_type && _type != e->type_name())
	if (!_type_glob || !glob_match(e->type_name(), _type))
	    return false;

    if (_name && _name != e->name())
	if (!_name_glob || !glob_match(e->name(), _name))
	    return false;

    const char *s;
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	if (k->starts_with("in", 2) && k->length() > 2
	    && comparison_char((*k)[2])) {
	    if (e->fake()
		|| !int_match_string(k->begin() + 2, k->end(), e->ninputs()))
		return false;
	} else if (k->starts_with("out", 3) && k->length() > 3
		   && comparison_char((*k)[3])) {
	    if (e->fake()
		|| !int_match_string(k->begin() + 3, k->end(), e->noutputs()))
		return false;
	} else if (k->equals("primitive", 9)) {
	    if (e->fake() || !e->primitive())
		return false;
	} else if (k->equals("compound", 8)) {
	    if (e->fake() || e->primitive())
		return false;
	} else if (k->equals("passthrough", 11)) {
	    if (e->fake() || e->primitive() || !e->passthrough())
		return false;
	} else if (k->equals("anonymous", 9)) {
	    const String &name = e->name(), &type_name = e->type_name();
	    if (name.length() < type_name.length() + 2
		|| name[type_name.length()] != '@'
		|| memcmp(name.begin(), type_name.begin(), type_name.length()))
		return false;
	} else if (k->equals("live", 4)) {
	    if (!cr->driver())
		return false;
	} else if (k->starts_with("name*=", 6)) {
	    if (e->name().find_left(k->substring(6)) < 0)
		return false;
	} else if (k->starts_with("downstream=", 11)) {
	    if (e->fake())
		return false;
	    const crouter::reachable_t &ex = cr->downstream(k->substring(11));
	    if (!ex(e->parent()->flat_name(), e->eindex()))
		return false;
	} else if (k->starts_with("downstream!=", 12)) {
	    if (e->fake())
		return false;
	    const crouter::reachable_t &ex = cr->downstream(k->substring(12));
	    if (ex(e->parent()->flat_name(), e->eindex()))
		return false;
	} else if (k->starts_with("upstream=", 9)) {
	    if (e->fake())
		return false;
	    const crouter::reachable_t &ex = cr->upstream(k->substring(9));
	    if (!ex(e->parent()->flat_name(), e->eindex()))
		return false;
	} else if (k->starts_with("upstream!=", 10)) {
	    if (e->fake())
		return false;
	    const crouter::reachable_t &ex = cr->upstream(k->substring(10));
	    if (ex(e->parent()->flat_name(), e->eindex()))
		return false;
	} else if (k->starts_with("reachable=", 10)) {
	    if (e->fake())
		return false;
	    const crouter::reachable_t &ex1 = cr->downstream(k->substring(10));
	    const crouter::reachable_t &ex2 = cr->upstream(k->substring(10));
	    if (!ex1(e->parent()->flat_name(), e->eindex())
		&& !ex2(e->parent()->flat_name(), e->eindex()))
		return false;
	} else if (k->starts_with("reachable!=", 11)) {
	    if (e->fake())
		return false;
	    const crouter::reachable_t &ex1 = cr->downstream(k->substring(11));
	    const crouter::reachable_t &ex2 = cr->upstream(k->substring(11));
	    if (ex1(e->parent()->flat_name(), e->eindex())
		|| ex2(e->parent()->flat_name(), e->eindex()))
		return false;
	} else if ((s = find(*k, '=')) != k->end()) {
	    if (!cr->driver() || !e->flat_name() || !e->primitive())
		return false;
	    const char *hend = s;
	    if (hend > k->begin() && hend[-1] == '!')
		--hend;
	    handler_value *hv = cr->hvalues().find_placeholder(e->flat_name() + "." + k->substring(k->begin(), hend), hflag_notify_delt);
	    if (!hv)
		return false;
	    // XXX fix this
	    if (!hv->have_hvalue()) {
		hv->refresh(cr);
		answer = false;
	    } else if ((hv->hvalue() == k->substring(s + 1, k->end()))
		       == (*hend == '!'))
		answer = false;
	    senses |= dsense_handler;
	} else
	    // generic class match would go here
	    return false;

    if (_highlight_match) {
	senses |= dsense_highlight;
	if ((e->highlights() & _highlight_match) != _highlight)
	    answer = false;
    }

    if (sensitivity)
	*sensitivity |= senses;
    return answer;
}

bool dcss_selector::match_port(bool isoutput, int port, int processing) const
{
    if (_type && !_type.equals("port", 4))
	if (!_type_glob || !glob_match(_type, "port"))
	    return false;

    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	if (k->equals("input", 5)) {
	    if (isoutput)
		return false;
	} else if (k->equals("output", 6)) {
	    if (!isoutput)
		return false;
	} else if (k->equals("push", 4)) {
	    if ((processing & ProcessingT::ppush) == 0)
		return false;
	} else if (k->equals("pull", 4)) {
	    if ((processing & ProcessingT::ppull) == 0)
		return false;
	} else if (k->equals("agnostic", 8)) {
	    if ((processing & ProcessingT::pagnostic) == 0)
		return false;
	} else if (k->equals("error", 5)) {
	    if ((processing & ProcessingT::perror) == 0)
		return false;
	} else
	    return false;

    if (_name && port >= 0
	&& !int_match_string(_name.begin(), _name.end(), port, false))
	return false;

    return true;
}

bool dcss_selector::match(const handler_value *hv) const
{
    if (!_type.equals("handler", 7))
	return false;
    if (_name && _name != hv->handler_name())
	if (!_name_glob || !glob_match(hv->handler_name(), _name))
	    return false;
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	if (k->equals("read", 4)) {
	    if (!hv->readable())
		return false;
	} else if (k->equals("write", 5)) {
	    if (!hv->writable())
		return false;
	} else if (k->equals("param", 5)) {
	    if (!hv->read_param())
		return false;
	} else if (k->equals("calm", 4)) {
	    if (!(hv->flags() & hflag_calm))
		return false;
	} else if (k->equals("expensive", 9)) {
	    if (!(hv->flags() & hflag_expensive))
		return false;
	} else if (k->equals("deprecated", 10)) {
	    if (!(hv->flags() & hflag_deprecated))
		return false;
	}
    return true;
}

bool dcss_selector::type_glob_match(PermString str) const
{
    return glob_match(str, _type);
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
	    int nest = 0, equals = 0;
	    for (; s != send && *s != '\n' && *s != '\r' && *s != '\f'
		     && (*s != ']' || nest); ++s)
		if (isspace((unsigned char) *s)) {
		    sa.append(last, s);
		    for (++s; s != send && isspace((unsigned char) *s); ++s)
			/* nada */;
		    if (!sa || s == send)
			/* nada */;
		    else if (*s == '=' && !equals) {
			sa << '=';
			++equals;
			for (++s; s != send && isspace((unsigned char) *s); ++s)
			    /* nada */;
		    } else
			sa << ' ';
		    last = s;
		    --s;
		} else if (*s == '[')
		    ++nest;
		else if (*s == ']')
		    --nest;
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
      retry:
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
	if (s == n && start == '#' && *s == '#' && !_name && !_type) {
	    _type = String::make_stable("handler", 7);
	    s = ++n;
	    goto retry;
	} else if (s == n && start) {
	    --s;
	    break;
	} else if (s == n)
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
	    else if (x.equals("pressed", 7))
		_highlight |= (1<<dhlt_pressed), _highlight_match |= (1<<dhlt_pressed);
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

void dcss_selector::unparse(StringAccum &sa) const
{
    int length = sa.length();
    if (_type)
	sa << _type;
    if (_name)
	sa << '#' << _name;
    for (const String *k = _klasses.begin(); k != _klasses.end(); ++k)
	sa << '.' << *k;
    if (_highlight & (1<<dhlt_click))
	sa << ":active";
    if (_highlight & (1<<dhlt_hover))
	sa << ":hover";
    if (sa.length() == length)
	sa << '*';
}

String dcss_selector::unparse() const
{
    StringAccum sa;
    unparse(sa);
    return sa.take_string();
}


/*****
 *
 */

const dcss_property dcss_property::null_property("", "");
const double dcss_property::transparent_color[4] = { 0, 0, 0, 0 };

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

      case t_seconds:
	if (cp_seconds(_vstr, &_v.d))
	    _t = t_seconds;
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
	} else if (_vstr.equals("outline", 7) || _vstr.equals("halo", 4)) {
	    _v.i = dshadow_outline;
	    _t = t_shadow_style;
	} else if (_vstr.equals("unscaled-outline", 16)) {
	    _v.i = dshadow_unscaled_outline;
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

double dcss_property::vpixel(crouter *cr, PermString relative_to,
			     const delt *e) const
{
    change_relative_pixel();
    if (_t == t_pixel)
	return _v.d;
    else if (_t == t_relative)
	return _v.d * cr->ccss()->vpixel(relative_to, cr, e);
    else
	return 0;
}


/*****
 *
 */

dcss::dcss()
    : _pflags(0), _sorted(false), _next(0)
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
	_pflags |= property_map[name];
	_de.push_back(dcss_property(name, value));
    }
}

bool dcss::hard_match_context(crouter *cr, const delt *e, int *sensitivity,
			      bool strict) const
{
    if (!e)
	return _context.begin() == _context.end();

    int sense = 0;
    const dcss_selector *sel_precise = _context.end();
    const dcss_selector *sel_approx = _context.end();

    if (strict && _context.begin() != _context.end()) {
	if (!_context.back().match(cr, e, sensitivity))
	    return false;
	--sel_precise;
	--sel_approx;
    }

    while (!e->root() && sel_approx != _context.begin()) {
	if (sel_precise != sel_approx && sel_precise != _context.begin()
	    && sel_precise[-1].match(cr, e, 0))
	    --sel_precise;

	int ts = 0;
	bool m = sel_approx[-1].match(cr, e, &ts);
	if (m && sel_precise == sel_approx)
	    --sel_precise;
	if (m || ts)
	    --sel_approx;
	sense |= ts;

	e = e->parent();
    }

    if (sel_approx == _context.begin() && sensitivity)
	*sensitivity |= sense;
    return sel_precise == _context.begin();
}

const char *dcss::parse(const String &str, const String &media, const char *s)
{
    const char *send = str.end();
    Vector<int> mediastk;

    while (1) {
	s = cp_skip_comment_space(s, send);
	const char *n = s;
	while (s != send && !isspace((unsigned char) *s)
	       && *s != ':' && *s != ';' && *s != '{' && *s != '}'
	       && (*s != '/' || s + 1 == send || (s[1] != '/' && s[1] != '*')))
	    ++s;
	const char *n_end = s;
	s = cp_skip_comment_space(s, send);

	// check for media
	if (n_end - n == 6 && memcmp(n, "@media", 6) == 0 && s > n_end) {
	    const char *z = s;
	    while (s != send && isalnum((unsigned char) *s))
		++s;
	    mediastk.push_back(media.equals(z, s - z));
	    s = cp_skip_comment_space(s, send);
	    if (s != send && *s == '{')
		++s;
	    continue;
	}

	if (s == send || *s == '{' || (*s == '}' && mediastk.size() == 0))
	    break;
	else if (*s == ';') {
	    ++s;
	    continue;
	} else if (*s == '}') {
	    mediastk.pop_back();
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

	if (n == n_end || v == v_ew0 || (mediastk.size() && !mediastk.back()))
	    /* do nothing */;
	else if (n + 6 == n_end && memcmp(n, "border", 6) == 0)
	    parse_border(str, v, v_ew0, "border");
	else if (n + 11 == n_end && memcmp(n, "port-border", 11) == 0)
	    parse_border(str, v, v_ew0, "port-border");
	else if (n + 12 == n_end && memcmp(n, "queue-stripe", 12) == 0)
	    parse_border(str, v, v_ew0, "queue-stripe");
	else if (n + 6 == n_end && memcmp(n, "shadow", 6) == 0)
	    parse_shadow(str, v, v_ew0);
	else if (n + 10 == n_end && memcmp(n, "background", 10) == 0)
	    parse_background(str, v, v_ew0);
	else if (n + 6 == n_end && memcmp(n, "margin", 6) == 0)
	    parse_box(str, v, v_ew0, "margin");
	else if (n + 11 == n_end && memcmp(n, "port-margin", 11) == 0)
	    parse_box(str, v, v_ew0, "port-margin");
	else if (n + 7 == n_end && memcmp(n, "padding", 7) == 0)
	    parse_box(str, v, v_ew0, "padding");
	else if (n + 5 == n_end && memcmp(n, "split", 5) == 0)
	    parse_split(str, v, v_ew0);
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
	    || (n + 4 == s && memcmp(n, "halo", 4) == 0)
	    || (n + 7 == s && memcmp(n, "outline", 7) == 0)
	    || (n + 16 == s && memcmp(n, "unscaled-outline", 16) == 0))
	    add("shadow-style", str.substring(n, s));
	else if (n < s && (isdigit((unsigned char) *n) || *n == '+' || *n == '.')
		 && cp_pixel(str.substring(n, s), &d))
	    add("shadow-width", str.substring(n, s));
	else if (cp_color(str.substring(n, s), &d, &d, &d, &d))
	    add("shadow-color", str.substring(n, s));
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
	    add("background-color", str.substring(n, s));
    }
}

void dcss::parse_split(const String &str, const char *s, const char *send)
{
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

	if (s == n + 4 && memcmp(n, "none", 4) == 0) {
	    add("port-split", str.substring(n, s));
	    add("flow-split", str.substring(n, s));
	} else if (s > n + 4 && memcmp(n, "flow(", 5) == 0)
	    add("flow-split", str.substring(n, s));
	else if ((s == n + 4 && memcmp(n, "both", 4) == 0)
		 || (s == n + 6 && memcmp(n, "inputs", 6) == 0)
		 || (s == n + 7 && memcmp(n, "outputs", 7) == 0))
	    add("port-split", str.substring(n, s));
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

void dcss::unparse_selector(StringAccum &sa) const
{
    if (_context.empty())
	sa << _selector.unparse();
    else {
	for (const dcss_selector *sp = _context.end(); sp != _context.begin(); )
	    sa << (--sp)->unparse() << ' ';
	sa << _selector.unparse();
    }
}

String dcss::unparse_selector() const
{
    StringAccum sa;
    unparse_selector(sa);
    return sa.take_string();
}

void dcss::unparse(StringAccum &sa) const
{
    unparse_selector(sa);
    sa << " {\n";
    for (dcss_property *it = _de.begin(); it != _de.end(); ++it)
	sa << '\t' << it->name() << ": " << it->vstring() << ";\n";
    sa << "}\n";
}

String dcss::unparse() const
{
    StringAccum sa;
    unparse(sa);
    return sa.take_string();
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

String dcss_set::expand_imports(const String &text, const String &filename,
				ErrorHandler *errh)
{
    LocalErrorHandler lerrh(errh);
    const char *s = text.begin(), *send = text.end();
    StringAccum expansion;

    while (1) {
	s = cp_skip_comment_space(s, send);
	if (s == send || s + 7 >= send || memcmp(s, "@import", 7) != 0)
	    break;
	const char *fnbegin = cp_skip_comment_space(s + 7, send);
	const char *fnend = s = ccss_skip_braces(fnbegin, send, true);
	if (fnbegin == fnend || *fnbegin == '{') {
	    lerrh.error("%s: bad @import rule", filename.c_str());
	    break;
	}

	// get @media
	String atmedia;
	s = cp_skip_comment_space(s, send);
	if (s != send && *s != ';') {
	    const char *atmedia_start = s;
	    while (s != send && *s != ';')
		if (*s == '/' && s + 1 != send && (s[1] == '/' || s[1] == '*'))
		    s = cp_skip_comment_space(s, send);
		else
		    ++s;
	    atmedia = text.substring(atmedia_start, s);
	}
	// skip past ';'
	if (s != send)
	    ++s;

	String fn;
	if (!cp_filename(text.substring(fnbegin, fnend), &fn) || !fn) {
	    lerrh.error("%s: bad @import rule", filename.c_str());
	    break;
	}
	if (fn[0] != '/' && filename && filename[0] != '<') {
	    String prefix = filename;
	    while (prefix && prefix.back() != '/')
		prefix = prefix.substring(0, prefix.length() - 1);
	    fn = prefix + fn;
	}

	String text = file_string(fn, &lerrh);
	if (atmedia)
	    expansion << "@media " << atmedia << " {\n";
	expansion << expand_imports(text, fn, &lerrh) << '\n';
	if (atmedia)
	    expansion << "}\n";
    }

    if (expansion) {
	expansion << text.substring(s, text.end());
	return expansion.take_string();
    } else
	return text.substring(s, text.end());
}

dcss_set::dcss_set(const String &text, const String &media)
    : _text(), _media(media), _media_next(0), _below(0),
      _selector_index(0), _frozen(false)
{
    _s.push_back(0);
    _s.push_back(0);

    if (!property_map.size()) {
	for (const dcss_propmatch *pm = port_pm; pm != port_pm + num_port_pm; ++pm)
	    property_map[pm->name] |= pflag_port;
	for (const dcss_propmatch *pm = elt_pm; pm != elt_pm + num_elt_pm; ++pm)
	    property_map[pm->name] |= pflag_elt;
	for (const dcss_propmatch *pm = elt_size_pm; pm != elt_size_pm + num_elt_size_pm; ++pm)
	    property_map[pm->name] |= pflag_elt_size;
	for (const dcss_propmatch *pm = handler_pm; pm != handler_pm + num_handler_pm; ++pm)
	    property_map[pm->name] |= pflag_handler;
	for (const dcss_propmatch *pm = fullness_pm; pm != fullness_pm + num_fullness_pm; ++pm)
	    property_map[pm->name] |= pflag_fullness;
	for (const dcss_propmatch *pm = activity_pm; pm != activity_pm + num_activity_pm; ++pm)
	    property_map[pm->name] |= pflag_activity;
    }

    parse(text);
}

dcss_set::dcss_set(dcss_set *below)
    : _text(), _media(below->media()), _media_next(0), _below(below),
      _selector_index(below->_selector_index + 100000), _frozen(false)
{
    _s.push_back(0);
    _s.push_back(0);
}

dcss_set *dcss_set::remedia(const String &m)
{
    dcss_set *p = this;
    dcss_set **pprev = &p;
    while (*pprev && (*pprev)->media() != m)
	pprev = &(*pprev)->_media_next;
    if (!*pprev) {
	if (_below) {
	    *pprev = new dcss_set(_below->remedia(m));
	    (*pprev)->parse(_text);
	} else
	    *pprev = new dcss_set(_text, m);
    }
    return *pprev;
}

void dcss_set::mark_change()
{
    if (_frozen) {
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
    if (_media_next)
	delete _media_next;
    mark_change();
}

void dcss_set::parse(const String &str)
{
    const char *s = str.begin(), *send = str.end();
    _text += str;

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
	    if (nsel == 0 && s == n + 6 && memcmp(n, "@media", 6) == 0) {
		s = cp_skip_comment_space(s, send);
		bool right_media = send - s > _media.length()
		    && memcmp(s, _media.data(), _media.length()) == 0
		    && !isalnum((unsigned char) s[_media.length()]);
		while (s != send && *s != '{')
		    ++s;	// cp_skip_comment_space
		if (s == send)
		    goto skip_braces;
		else if (!right_media) {
		    ++s;
		    goto skip_braces;
		} else {
		    s = cp_skip_comment_space(s + 1, send);
		    continue;
		}
	    }
	    if (nsel == 0)
		cs.push_back(new dcss);
	    else
		cs.back()->_context.push_back(cs.back()->_selector);
	    cs.back()->_selector = sel;
	    ++nsel;

	    s = cp_skip_comment_space(s, send);
	    if (s != send && *s == ',') {
		s = cp_skip_comment_space(s + 1, send);
		nsel = 0;
	    } else if (s != send && *s == '>')
		s = cp_skip_comment_space(s + 1, send);
	}

	if (cs.size() && s != send && *s == '{') {
	    const char *n = s + 1;
	    s = cs[0]->parse(str, _media, n);
	    if (cs[0]->_de.size()) {
		for (dcss **cspp = cs.begin() + 1; cspp != cs.end(); ++cspp)
		    (*cspp)->parse(str, _media, n);
		for (dcss **cspp = cs.begin(); cspp != cs.end(); ++cspp)
		    add(*cspp);
		cs.clear();
	    }
	}

      skip_braces:
	s = ccss_skip_braces(s, send);
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
}

dcss_set *dcss_set::default_set(const String &media)
{
    static dcss_set *x;
    if (!x)
	x = new dcss_set(default_css, media);
    return x->remedia(media);
}


/*****
 *
 */

static dcss_propmatch *port_pmp[num_port_pm];

dcss *dcss_set::ccss_list(const String &str) const
{
    if (str)
	for (dcss * const *sp = _s.begin() + 2; sp != _s.end(); ++sp)
	    if ((*sp)->type()[0] == str[0])
		return *sp;
    return 0;
}

void dcss_set::collect_port_styles(crouter *cr, const delt *e, bool isoutput,
				   int port, int processing,
				   Vector<dcss *> &result)
{
    assert(!e->root());
    for (dcss *s = ccss_list("port"); s; s = s->_next)
	if ((s->pflags() & pflag_port)
	    && s->selector().match_port(isoutput, port, processing)
	    && s->strict_match_context(cr, e))
	    result.push_back(s);
    for (dcss *s = _s[0]; s; s = s->_next)
	if ((s->pflags() & pflag_port)
	    && s->selector().match_port(isoutput, port, processing)
	    && s->strict_match_context(cr, e))
	    result.push_back(s);
    collect_elt_styles(cr, e, pflag_port | pflag_no_below, result, 0);
    if (_below)
	_below->collect_port_styles(cr, e, isoutput, port, processing, result);
}

ref_ptr<dport_style> dcss_set::port_style(crouter *cr, const delt *e,
					  bool isoutput, int port, int processing)
{
    Vector<dcss *> sv;
    collect_port_styles(cr, e, isoutput, port, processing, sv);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<dport_style> &style_cache = _ptable[sa.take_string()];

    if (!style_cache) {
	dcss::assign_all(port_pm, port_pmp, num_port_pm, sv.begin(), sv.end());

	dport_style *sty = new dport_style;
	String s = port_pm[0].vstring("port-shape");
	sty->shape = (s.equals("triangle", 8) ? dpshape_triangle : dpshape_rectangle);
	sty->length = port_pm[1].vpixel("port-length");
	sty->width = port_pm[2].vpixel("port-width");
	port_pm[3].vcolor(sty->color, "port-color");
	sty->border_style = port_pm[4].vborder_style("port-border-style");
	sty->border_width = port_pm[5].vpixel("port-border-width");
	port_pm[6].vcolor(sty->border_color, "port-border-color");
	if (sty->border_color[3] == 0 || sty->border_width <= 0)
	    sty->border_style = dborder_none;
	sty->margin[0] = port_pm[7].vpixel("port-margin-top");
	sty->margin[1] = port_pm[8].vpixel("port-margin-right");
	sty->margin[2] = port_pm[9].vpixel("port-margin-bottom");
	sty->margin[3] = port_pm[10].vpixel("port-margin-left");
	sty->edge_padding = port_pm[11].vpixel("port-edge-padding");
	s = port_pm[12].vstring("port-display");
	if (s.equals("none", 4))
	    sty->display = dpdisp_none;
	else if (s.equals("inputs", 6))
	    sty->display = dpdisp_inputs;
	else if (s.equals("outputs", 7))
	    sty->display = dpdisp_outputs;
	else
	    sty->display = dpdisp_both;
	sty->text = cp_unquote(port_pm[13].vstring("port-text"));
	sty->font = cp_unquote(port_pm[14].vstring("port-font"));

	style_cache = ref_ptr<dport_style>(sty);
    }

    return style_cache;
}


/*****
 *
 */

static dcss_propmatch *elt_pmp[num_elt_pm];

void dcss_set::collect_elt_styles(crouter *cr, const delt *e, int pflag,
				  Vector<dcss *> &result, int *sensitivity) const
{
    assert(!e->root());
    for (dcss *s = ccss_list(e->type_name()); s; s = s->_next)
	if ((s->pflags() & pflag)
	    && s->selector().match(cr, e, sensitivity)
	    && s->match_context(cr, e, sensitivity))
	    result.push_back(s);
    for (dcss *s = _s[0]; s; s = s->_next)
	if ((s->pflags() & pflag)
	    && s->selector().match(cr, e, sensitivity)
	    && s->match_context(cr, e, sensitivity))
	    result.push_back(s);
    //if (e->parent() && !e->parent()->root())
    //	collect_elt_styles(cr, e->parent(), pflag | pflag_no_below, result, sensitivity);
    if (_below && !(pflag & pflag_no_below))
	_below->collect_elt_styles(cr, e, pflag, result, sensitivity);
}

static String parse_flow_split(const char *begin, const char *end)
{
    StringAccum sa;
    bool output = false;

    for (begin = cp_skip_space(begin, end); begin < end; ++begin)
	if (*begin == '/') {
	    if (output || !sa.length())
		return String();
	    output = true;
	    sa << '/';
	} else if (isalpha((unsigned char) *begin))
	    sa << *begin;
	else
	    break;

    begin = cp_skip_space(begin, end);
    if (begin != end || !sa.length() || !output || sa.back() == '/')
	return String();
    else
	return sa.take_string();
}

ref_ptr<delt_style> dcss_set::elt_style(crouter *cr, const delt *e, int *sensitivity)
{
    if (sensitivity)
	*sensitivity = 0;
    Vector<dcss *> sv;
    collect_elt_styles(cr, e, pflag_elt, sv, sensitivity);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<delt_style> &style_cache = _etable[sa.take_string()];

    if (!style_cache) {
	dcss::assign_all(elt_pm, elt_pmp, num_elt_pm, sv.begin(), sv.end());

	delt_style *sty = new delt_style;
	elt_pm[0].vcolor(sty->color, "color");
	elt_pm[1].vcolor(sty->background_color, "background-color");
	sty->border_style = elt_pm[2].vborder_style("border-style");
	elt_pm[3].vcolor(sty->border_color, "border-color");
	if (sty->border_color[3] == 0)
	    sty->border_style = dborder_none;
	sty->shadow_style = elt_pm[4].vshadow_style("shadow-style");
	sty->shadow_width = elt_pm[5].vpixel("shadow-width", cr, e);
	elt_pm[6].vcolor(sty->shadow_color, "shadow-color");
	if (sty->shadow_color[3] == 0 || sty->shadow_width <= 0)
	    sty->shadow_style = dshadow_none;
	String s = elt_pm[7].vstring("style");
	sty->style = (s.equals("queue", 5) ? destyle_queue : destyle_normal);
	sty->text = cp_unquote(elt_pm[8].vstring("text"));
	s = elt_pm[9].vstring("display");
	sty->display = dedisp_normal;
	if (s.equals("none", 4))
	    sty->display = dedisp_none;
	else if (s.equals("closed", 6))
	    sty->display = dedisp_closed;
	else if (s.equals("passthrough", 11))
	    sty->display = dedisp_passthrough;
	else if (s.equals("expanded", 8) || s.equals("open", 4))
	    sty->display = dedisp_expanded;
	sty->font = elt_pm[10].vstring("font");
	sty->decorations = elt_pm[11].vstring("decorations");
	sty->queue_stripe_style = elt_pm[12].vborder_style("queue-stripe-style");
	sty->queue_stripe_width = elt_pm[13].vpixel("queue-stripe-width", cr, e);
	elt_pm[14].vcolor(sty->queue_stripe_color, "queue-stripe-color");
	s = elt_pm[15].vstring("port-split");
	sty->port_split = dpdisp_none;
	if (s.equals("inputs", 6))
	    sty->port_split = dpdisp_inputs;
	else if (s.equals("outputs", 7))
	    sty->port_split = dpdisp_outputs;
	else if (s.equals("both", 4))
	    sty->port_split = dpdisp_both;
	s = elt_pm[16].vstring("flow-split");
	sty->flow_split = parse_flow_split(s.begin(), s.end());

	style_cache = ref_ptr<delt_style>(sty);
    }

    return style_cache;
}


static dcss_propmatch *elt_size_pmp[num_elt_size_pm];

ref_ptr<delt_size_style> dcss_set::elt_size_style(crouter *cr, const delt *e, int *sensitivity)
{
    if (sensitivity)
	*sensitivity = 0;
    Vector<dcss *> sv;
    collect_elt_styles(cr, e, pflag_elt_size, sv, sensitivity);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<delt_size_style> &style_cache = _estable[sa.take_string()];

    if (!style_cache) {
	dcss::assign_all(elt_size_pm, elt_size_pmp, num_elt_size_pm, sv.begin(), sv.end());

	double scale = elt_size_pm[13].vrelative("scale");
	if (scale <= 0)
	    scale = 1;

	delt_size_style *sty = new delt_size_style;
	sty->scale = scale;
	sty->border_width = elt_size_pm[0].vpixel("border-width", cr, e);
	sty->padding[0] = elt_size_pm[1].vpixel("padding-top", cr, e) * scale;
	sty->padding[1] = elt_size_pm[2].vpixel("padding-right", cr, e) * scale;
	sty->padding[2] = elt_size_pm[3].vpixel("padding-bottom", cr, e) * scale;
	sty->padding[3] = elt_size_pm[4].vpixel("padding-left", cr, e) * scale;
	sty->min_width = elt_size_pm[5].vpixel("min-width", cr, e) * scale;
	sty->min_height = elt_size_pm[6].vpixel("min-height", cr, e) * scale;
	sty->min_length = elt_size_pm[15].vpixel("min-length", cr, e) * scale;
	sty->height_step = elt_size_pm[7].vpixel("height-step", cr, e) * scale;
	sty->margin[0] = elt_size_pm[8].vpixel("margin-top", cr, e) * scale;
	sty->margin[1] = elt_size_pm[9].vpixel("margin-right", cr, e) * scale;
	sty->margin[2] = elt_size_pm[10].vpixel("margin-bottom", cr, e) * scale;
	sty->margin[3] = elt_size_pm[11].vpixel("margin-left", cr, e) * scale;
	sty->queue_stripe_spacing = elt_size_pm[12].vpixel("queue-stripe-spacing", cr, e) * scale;
	String s = elt_size_pm[14].vstring("orientation");
	sty->orientation = 0;
	if (s.find_left("horizontal") >= 0)
	    sty->orientation = 3;
	if (s.find_left("reverse") >= 0)
	    sty->orientation ^= 2;

	style_cache = ref_ptr<delt_size_style>(sty);
    }

    return style_cache;
}


/*****
 *
 */

static int parse_autorefresh(String str, const char *medium, int *period)
{
    int on = -1;
    double d;
    while (String x = cp_shift_spacevec(str))
	if (x.equals("on", 2) || (medium && x.equals("both", 4)))
	    on = (medium ? 2 : 1);
	else if (x.equals("off", 3))
	    on = 0;
	else if (medium && x.equals(medium, -1))
	    on = 1;
	else if (cp_seconds(x, &d) && d >= 0) {
	    if (on < 0)
		on = (medium ? 2 : 1);
	    *period = (int) (d * 1000);
	}
    return (on < 0 ? 0 : on);
}


static dcss_propmatch *handler_pmp[num_handler_pm];

void dcss_set::collect_handler_styles(crouter *cr, const handler_value *hv,
				      const delt *e, Vector<dcss *> &result,
				      bool &generic) const
{
    for (dcss * const *sp = _s.begin() + 2; sp != _s.end(); ++sp)
	if ((*sp)->type()[0] == 'h')
	    for (dcss *s = *sp; s; s = s->_next)
		if (s->selector().match(hv) && s->match_context(cr, e)) {
		    if (!s->selector().generic_handler() || s->has_context())
			generic = false;
		    result.push_back(s);
		}
    for (dcss *s = _s[0]; s; s = s->_next)
	if (s->selector().match(hv) && s->match_context(cr, e)) {
	    if (!s->selector().generic_handler() || s->has_context())
		generic = false;
	    result.push_back(s);
	}
    if (_below)
	_below->collect_handler_styles(cr, hv, e, result, generic);
}

ref_ptr<dhandler_style> dcss_set::handler_style(crouter *cr, const delt *e, const handler_value *hv)
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_handler_styles(cr, hv, e, sv, generic);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<dhandler_style> &style_cache = _htable[sa.take_string()];

    if (!style_cache) {
	dcss::assign_all(handler_pm, handler_pmp, num_handler_pm, sv.begin(), sv.end());

	dhandler_style *sty = new dhandler_style;
	sty->autorefresh_period = (int) (handler_pm[1].vseconds("autorefresh-period") * 1000);
	sty->flags_mask = hflag_autorefresh | hflag_refresh | hflag_visible
	    | hflag_collapse;
	sty->flags = 0;
	String s = handler_pm[0].vstring("autorefresh");
	if (parse_autorefresh(s, 0, &sty->autorefresh_period))
	    sty->flags |= hflag_autorefresh;
	if (!s)
	    sty->flags_mask &= ~hflag_autorefresh;
	s = handler_pm[2].vstring("display");
	if (s && !s.equals("none", 4))
	    sty->flags |= hflag_visible;
	if (s.equals("collapse", 8))
	    sty->flags |= hflag_collapse;
	if (!s)
	    sty->flags_mask &= ~(hflag_visible | hflag_collapse);
	s = handler_pm[3].vstring("allow-refresh");
	if (s.equals("yes", 3) || s.equals("true", 4))
	    sty->flags |= hflag_refresh;
	if (!s)
	    sty->flags_mask &= ~hflag_refresh;

	style_cache = ref_ptr<dhandler_style>(sty);
    }

    return style_cache;
}


/*****
 *
 *
 *
 */

static dcss_propmatch *fullness_pmp[num_fullness_pm];

void dcss_set::collect_decor_styles(PermString decor, crouter *cr, const delt *e,
				    Vector<dcss *> &result, bool &generic) const
{
    for (dcss * const *sp = _s.begin() + 2; sp != _s.end(); ++sp)
	if ((*sp)->type()[0] == decor[0])
	    for (dcss *s = *sp; s; s = s->_next)
		if (s->selector().match_decor(decor) && s->match_context(cr, e)) {
		    if (!s->selector().generic_decor() || s->has_context())
			generic = false;
		    result.push_back(s);
		}
    for (dcss *s = _s[0]; s; s = s->_next)
	if (s->selector().match_decor(decor) && s->match_context(cr, e)) {
	    if (!s->selector().generic_decor() || s->has_context())
		generic = false;
	    result.push_back(s);
	}
    if (_below)
	_below->collect_decor_styles(decor, cr, e, result, generic);
}

ref_ptr<dfullness_style> dcss_set::fullness_style(PermString decor, crouter *cr, const delt *e)
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_decor_styles(decor, cr, e, sv, generic);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<dfullness_style> &style_cache = _ftable[sa.take_string()];

    if (!style_cache) {
	dcss::assign_all(fullness_pm, fullness_pmp, num_fullness_pm, sv.begin(), sv.end());

	dfullness_style *sty = new dfullness_style;
	sty->length = fullness_pm[0].vstring("length");
	sty->capacity = fullness_pm[1].vstring("capacity");
	fullness_pm[2].vcolor(sty->color, "color");
	sty->autorefresh_period = (int) (fullness_pm[4].vseconds("autorefresh-period") * 1000);
	String s = fullness_pm[3].vstring("autorefresh");
	sty->autorefresh = parse_autorefresh(s, "length", &sty->autorefresh_period);

	style_cache = ref_ptr<dfullness_style>(sty);
    }

    return style_cache;
}


/*****
 *
 *
 *
 */

static dcss_propmatch *activity_pmp[num_activity_pm];

ref_ptr<dactivity_style> dcss_set::activity_style(PermString decor, crouter *cr, const delt *e)
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_decor_styles(decor, cr, e, sv, generic);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);
    StringAccum sa(sizeof(unsigned) * sv.size());
    for (dcss **sp = sv.begin(); sp != sv.end(); ++sp)
	*reinterpret_cast<unsigned *>(sa.extend(sizeof(unsigned))) = (*sp)->selector_index();
    ref_ptr<dactivity_style> &style_cache = _atable[sa.take_string()];

    if (!style_cache) {
	dcss::assign_all(activity_pm, activity_pmp, num_activity_pm, sv.begin(), sv.end());

	dactivity_style *sty = new dactivity_style;
	sty->handler = activity_pm[0].vstring("handler");
	sty->autorefresh_period = (int) (activity_pm[3].vseconds("autorefresh-period") * 1000);
	String s = activity_pm[2].vstring("autorefresh");
	sty->autorefresh = parse_autorefresh(s, "", &sty->autorefresh_period);
	s = activity_pm[4].vstring("type");
	if (s.starts_with("rate", 4))
	    sty->type = dactivity_rate;
	else
	    sty->type = dactivity_absolute;
	sty->max_value = activity_pm[5].vnumeric("max-value");
	sty->min_value = activity_pm[6].vnumeric("min-value");
	sty->max_value = std::max(sty->min_value, sty->max_value);
	sty->decay = activity_pm[7].vseconds("decay");

	// parse color series
	Vector<String> vs;
	s = activity_pm[1].vstring("color");
	while (String sx = ccss_pop_commavec(s))
	    vs.push_back(sx);
	if (vs.size() == 0)
	    vs.push_back("none");
	if (vs.size() == 1) {
	    vs.push_back(vs[0]);
	    vs[0] = "none";
	}
	for (String *vsp = vs.begin(); vsp != vs.end(); ++vsp) {
	    int x = sty->colors.size();
	    sty->colors.resize(sty->colors.size() + 5);
	    sty->colors[x] = -1;
	    if (*vsp && isdigit((unsigned char) (*vsp)[0])) {
		if (!cp_relative(cp_shift_spacevec(*vsp), &sty->colors[x])
		    || sty->colors[x] < 0 || sty->colors[x] > 1)
		    sty->colors[x] = -1;
	    }
	    if (!cp_color(*vsp, &sty->colors[x+1], &sty->colors[x+2], &sty->colors[x+3], &sty->colors[x+4]))
		memcpy(&sty->colors[x+1], dcss_property::transparent_color, sizeof(double) * 4);
	}
	sty->colors[0] = 0.;
	sty->colors[sty->colors.size() - 5] = 1.;
	for (int i = 5; i < sty->colors.size(); i += 5) {
	    if (sty->colors[i] < 0) {
		int n;
		for (n = 2; sty->colors[i + 5*(n-1)] < 0; ++n)
		    /* nada */;
		sty->colors[i] =
		    (sty->colors[i-5] * (n - 1) + sty->colors[i + 5*(n-1)]) / n;
	    } else if (sty->colors[i] < sty->colors[i - 5])
		sty->colors[i] = sty->colors[i - 5] + 0.001;
	}

	//if (sty->type == dactivity_rate)
	//sty->decay_begin = std::max(sty->decay_begin, 10U);
	//sty->decay_end = std::max(sty->decay_begin, sty->decay_end);

	style_cache = ref_ptr<dactivity_style>(sty);
    }

    return style_cache;
}


/*****
 *
 *
 *
 */

double dcss_set::vpixel(PermString name, crouter *cr, const delt *e) const
{
    Vector<dcss *> sv;
    collect_elt_styles(cr, e, pflag_elt | pflag_elt_size, sv, 0);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);

    dcss_propmatch pm = { name, 0 }, *pmp = &pm;
    dcss::assign_all(&pm, &pmp, 1, sv.begin(), sv.end());

    return pm.vpixel(name.c_str(), cr, name, e->parent());
}

String dcss_set::vstring(PermString name, PermString decor, crouter *cr, const delt *e) const
{
    Vector<dcss *> sv;
    bool generic = true;
    collect_decor_styles(decor, cr, e, sv, generic);

    std::sort(sv.begin(), sv.end(), dcsspp_compare);

    dcss_propmatch pm = { name, 0 }, *pmp = &pm;
    dcss::assign_all(&pm, &pmp, 1, sv.begin(), sv.end());

    return pm.vstring(name.c_str());
}

}
