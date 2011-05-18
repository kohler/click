#include <click/config.h>
#include "gathererror.hh"
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <algorithm>

const unsigned char *
skip_utf8_char(const unsigned char *s, const unsigned char *end)
{
    int c = *s;
    if (c > 0 && c < 0x80)
        return s + 1;
    else if (c < 0xC2)
	/* zero, or bad/overlong encoding */;
    else if (c < 0xE0) {	// 2 bytes: U+80-U+7FF
        if (likely(s + 1 < end
                   && s[1] >= 0x80 && s[1] < 0xC0))
            return s + 2;
    } else if (c < 0xF0) {	// 3 bytes: U+800-U+FFFF
        if (likely(s + 2 < end
                   && s[1] >= 0x80 && s[1] < 0xC0
		   && s[2] >= 0x80 && s[2] < 0xC0
                   && (c != 0xE0 || s[1] >= 0xA0) /* not overlong encoding */
                   && (c != 0xED || s[1] < 0xA0) /* not surrogate */))
            return s + 3;
    } else if (c < 0xF5) {	// 4 bytes: U+10000-U+10FFFF
        if (likely(s + 3 < end
                   && s[1] >= 0x80 && s[1] < 0xC0
		   && s[2] >= 0x80 && s[2] < 0xC0
		   && s[3] >= 0x80 && s[3] < 0xC0
                   && (c != 0xF0 || s[1] >= 0x90) /* not overlong encoding */
                   && (c != 0xF4 || s[1] < 0x90) /* not >U+10FFFF */))
            return s + 4;
    }
    return s;
}

int
utf8_length(const unsigned char *begin, const unsigned char *end)
{
    int n = 0;
    const unsigned char *t;
    while (begin != end) {
	++n;
	if (*begin > 0 && *begin < 128)
	    ++begin;
	else if ((t = skip_utf8_char(begin, end)) != begin)
	    begin = t;
	else
	    ++begin;
    }
    return n;
}

int
utf8_length(const char *begin, const char *end)
{
    return utf8_length(reinterpret_cast<const unsigned char *>(begin),
		       reinterpret_cast<const unsigned char *>(end));
}

GatherErrorHandler::GatherErrorHandler(bool utf8)
    : _end_offset(0), _next_errpos1(0), _next_errpos2(0), _nwarnings(0),
      _utf8(utf8)
{
}

String
GatherErrorHandler::vformat(const char *fmt, va_list val)
{
    return vxformat(_utf8 ? cf_utf8 : 0, fmt, val);
}

void *GatherErrorHandler::emit(const String &str, void *, bool more)
{
    int xlevel = 1000;
    String landmark;
    const char *s = parse_anno(str, str.begin(), str.end(), "l", &landmark,
			       "#l1", &_next_errpos1, "#l2", &_next_errpos2,
			       "#<>", &xlevel, (const char *) 0);
    String x = clean_landmark(landmark, true) + str.substring(s, str.end())
	+ "\n";
    _v.push_back(Message(x, xlevel, _end_offset, _next_errpos1, _next_errpos2));
    if (!more)
	_next_errpos1 = _next_errpos2 = 0;
    _end_offset = _v.back().offset2;
    return 0;
}

void GatherErrorHandler::account(int level)
{
    ErrorHandler::account(level);
    if (level == el_warning)
	++_nwarnings;
}

void GatherErrorHandler::clear()
{
    ErrorHandler::clear();
    _v.clear();
    _end_offset = 0;
    _nwarnings = 0;
}

GatherErrorHandler::iterator GatherErrorHandler::erase(iterator i)
{
    i = _v.erase(i);
    for (iterator j = i; j != _v.end(); j++) {
	int olen = j->offset2 - j->offset1;
	if (j == _v.begin())
	    j->offset1 = 0;
	else
	    j->offset1 = j[-1].offset2;
	j->offset2 = j->offset1 + olen;
    }
    if (_v.size())
	_end_offset = _v.back().offset2;
    else
	_end_offset = 0;
    return i;
}

GatherErrorHandler::iterator GatherErrorHandler::erase(iterator begin, iterator end)
{
    begin = _v.erase(begin, end);
    for (iterator j = begin; j != _v.end(); j++) {
	int olen = j->offset2 - j->offset1;
	if (j == _v.begin())
	    j->offset1 = 0;
	else
	    j->offset1 = j[-1].offset1 + j[-1].message.length();
	j->offset2 = j->offset1 + olen;
    }
    if (_v.size())
	_end_offset = _v.back().offset2;
    else
	_end_offset = 0;
    return begin;
}

void GatherErrorHandler::translate_prefix(const String &from, const String &to, int beginpos)
{
    assert(beginpos >= 0 && beginpos <= _v.size());

    int offset1 = 0;
    if (beginpos > 0)
	offset1 = _v[beginpos - 1].offset2;

    for (iterator i = _v.begin() + beginpos; i != _v.end(); i++) {
	i->offset1 = offset1;
	if (i->message.length() >= from.length()
	    && i->message.substring(0, from.length()) == from)
	    i->message = to + i->message.substring(from.length());
	i->offset2 = offset1 = offset1 + utf8_length(i->message);
    }
    _end_offset = offset1;
}

void GatherErrorHandler::run_dialog(GtkWindow *w, int beginpos)
{
    assert(beginpos >= 0 && beginpos <= _v.size());

    StringAccum sa;
    for (iterator i = _v.begin() + beginpos; i != _v.end(); i++)
	sa << i->message;

    gtk_widget_show(GTK_WIDGET(w));
    GtkWidget *dialog = gtk_message_dialog_new(w,
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    "%s",
				    sa.c_str());

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

namespace {
bool geh_offset_search(const GatherErrorHandler::Message &m, int o)
{
    return m.offset2 <= o;
}
}

GatherErrorHandler::iterator GatherErrorHandler::find_offset(int offset)
{
    return std::lower_bound(_v.begin(), _v.end(), offset, geh_offset_search);
}

GatherErrorHandler::const_iterator GatherErrorHandler::find_offset(int offset) const
{
    return std::lower_bound(_v.begin(), _v.end(), offset, geh_offset_search);
}

String GatherErrorHandler::message_string(const_iterator begin, const_iterator end)
{
    StringAccum sa;
    for (; begin != end; ++begin)
	sa << begin->message;
    return sa.take_string();
}
