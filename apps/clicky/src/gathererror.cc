#include <click/config.h>
#include "gathererror.hh"
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <algorithm>

GatherErrorHandler::GatherErrorHandler()
    : _end_offset(0), _next_errpos1(0), _next_errpos2(0)
{
}

String GatherErrorHandler::decorate_text(Seriousness, const String &landmark, const String &text)
{
  String new_text = text;

  // skip initial special-purpose portion of a landmark ('\<...>')
  int first_lpos = 0;
  if (landmark.length() > 2 && landmark[0] == '\\' && landmark[1] == '<') {
      const char *slash1 = landmark.begin() + 2;
      const char *slash2 = find(slash1, landmark.end(), '>');
      if (slash2 != landmark.end()) {
	  const char *comma = find(slash1, slash2, ',');
	  unsigned off1, off2;
	  if (comma != slash2 && cp_integer(slash1, comma, 10, &off1)
	      && cp_integer(comma + 1, slash2, 10, &off2)) {
	      _next_errpos1 = off1;
	      _next_errpos2 = off2;
	  }
	  first_lpos = slash2 + 1 - landmark.begin();
      }
  }
  
  // handle landmark
  if (first_lpos < landmark.length()) {
    // fix landmark: skip trailing spaces and trailing colon
    int i, len = landmark.length();
    for (i = len - 1; i >= first_lpos; i--)
      if (!isspace((unsigned char) landmark[i]))
	break;
    if (i >= first_lpos && landmark[i] == ':')
      i--;

    // prepend landmark, unless all spaces
    if (i >= first_lpos)
      new_text = prepend_lines(landmark.substring(first_lpos, i + 1 - first_lpos) + ": ", new_text);
  }

  return new_text;
}

void GatherErrorHandler::handle_text(Seriousness seriousness, const String &str)
{
    _v.push_back(Message(seriousness, str, _end_offset, _next_errpos1, _next_errpos2));
    if (!str || str.back() != '\n')
	_v.back().message += "\n";
    _next_errpos1 = _next_errpos2 = 0;
    _end_offset += _v.back().message.length();
}

void GatherErrorHandler::clear()
{
    _v.clear();
    _end_offset = 0;
    reset_counts();
}

GatherErrorHandler::iterator GatherErrorHandler::erase(iterator i)
{
    i = _v.erase(i);
    for (iterator j = i; j != _v.end(); j++)
	if (j == _v.begin())
	    j->offset1 = 0;
	else
	    j->offset1 = j[-1].offset1 + j[-1].message.length();
    if (_v.size())
	_end_offset = _v.back().offset1 + _v.back().message.length();
    else
	_end_offset = 0;
    return i;
}

GatherErrorHandler::iterator GatherErrorHandler::erase(iterator begin, iterator end)
{
    begin = _v.erase(begin, end);
    for (iterator j = begin; j != _v.end(); j++)
	if (j == _v.begin())
	    j->offset1 = 0;
	else
	    j->offset1 = j[-1].offset1 + j[-1].message.length();
    if (_v.size())
	_end_offset = _v.back().offset1 + _v.back().message.length();
    else
	_end_offset = 0;
    return begin;
}

void GatherErrorHandler::translate_prefix(const String &from, const String &to, int beginpos)
{
    assert(beginpos >= 0 && beginpos <= _v.size());
    
    int offset1 = 0;
    if (beginpos > 0)
	offset1 = _v[beginpos - 1].offset1 + _v[beginpos - 1].message.length();
    
    for (iterator i = _v.begin() + beginpos; i != _v.end(); i++) {
	i->offset1 = offset1;
	if (i->message.length() >= from.length()
	    && i->message.substring(0, from.length()) == from)
	    i->message = to + i->message.substring(from.length());
	offset1 += i->message.length();
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
    return m.offset1 + m.message.length() <= o;
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

#include <click/vector.cc>
