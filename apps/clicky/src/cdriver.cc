#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "cdriver.hh"
#include <click/confparse.hh>
#include <click/llrpc.h>
#include <clicktool/etraits.hh>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
namespace clicky {

int cdriver::check_handler_name(const String &inname, String &ename, String &hname, ErrorHandler *errh)
{
    const char *dot = find(inname, '.');
    if (dot == inname.end()) {
	ename = String();
	hname = inname;
    } else if (dot == inname.begin()) {
	ename = String();
	hname = inname.substring(1);
    } else {
	ename = inname.substring(inname.begin(), dot);
	hname = inname.substring(dot + 1, inname.end());
    }

    if (ename && !cp_is_click_id(ename))
	goto error;
    dot = hname.begin();
    if (dot == hname.end()
	|| (dot + 1 == hname.end() && dot[0] == '.')
	|| (dot + 2 == hname.end() && dot[0] == '.' && dot[1] == '.'))
	goto error;
    for (dot = hname.begin(); dot != hname.end(); ++dot)
	if (*dot < 32 || *dot >= 127 || *dot == '/')
	    goto error;
    return 0;

  error:
    return errh->error("Bad handler name '%#s'", inname.printable().c_str());
}

void cdriver::transfer_messages(crouter *cr, int status, const messagevector &messages)
{
    if (messages.size()) {
	const char *err;
	if (status <= 219)
	    err = ErrorHandler::e_info;
	else if (status <= 299)
	    err = ErrorHandler::e_warning_annotated;
	else
	    err = ErrorHandler::e_error;
	for (messagevector::const_iterator i = messages.begin(); i != messages.end(); ++i)
	    cr->error_handler()->xmessage(i->first, err, i->second);
	cr->on_error(false, String());
    }
}


/*****
 *
 * control socket connection
 *
 */

extern "C" {
static gboolean csocket_watch(GIOChannel *, GIOCondition condition, gpointer user_data)
{
    csocket_cdriver *wd = reinterpret_cast<csocket_cdriver *>(user_data);
    return wd->csocket_event(condition);
}
}

int csocket_cdriver::make_nonblocking(int fd, ErrorHandler *errh)
{
    int flags = 1;
    if (::ioctl(fd, FIONBIO, &flags) != 0) {
	flags = ::fcntl(fd, F_GETFL);
	if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	    return errh->error("%s", strerror(errno));
    }
    return 0;
}

GIOChannel *csocket_cdriver::start_connect(IPAddress addr, uint16_t port, bool *ready, ErrorHandler *errh)
{
    // open socket, set options
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	errh->error("%s", strerror(errno));
	return 0;
    }

    // make socket nonblocking
    if (make_nonblocking(fd, errh) != 0)
	return 0;

    // connect to port
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr = addr;
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != -1)
	*ready = true;
    else if (errno == EINPROGRESS)
	*ready = false;
    else {
	errh->error("%s", strerror(errno));
	return 0;
    }

    // attach file descriptor
    GIOChannel *channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(channel, NULL, NULL);
    return channel;
}

csocket_cdriver::csocket_cdriver(crouter *cr, GIOChannel *socket, bool ready)
    : _cr(cr), _csocket(socket), _csocket_watch(0)
{
    _cr->set_driver(this, true);
    if (ready) {
	_csocket_state = csocket_initial;
	(void) csocket_event(G_IO_IN);
    } else {
	_csocket_state = csocket_connecting;
	_csocket_watch = g_io_add_watch(_csocket, (GIOCondition) (G_IO_OUT | G_IO_ERR | G_IO_HUP), csocket_watch, this);
    }
}

csocket_cdriver::~csocket_cdriver()
{
    if (_csocket) {
	g_io_channel_shutdown(_csocket, FALSE, 0);
	g_io_channel_unref(_csocket);
	if (_csocket_watch && _csocket_watch != (guint) -1)
	    g_source_remove(_csocket_watch);
    }
    while (_csocket_msgq.size()) {
	delete _csocket_msgq.back();
	_csocket_msgq.pop_back();
    }
    _csocket = 0;
    _csocket_watch = 0;
    _csocket_state = csocket_failed;
}

bool csocket_cdriver::active() const
{
    return _csocket_state != csocket_failed;
}

int csocket_cdriver::driver_mask() const
{
    return 1 << Driver::USERLEVEL;
}

gboolean csocket_cdriver::kill_with_dialog(GatherErrorHandler *gerrh, int begin_pos, const char *format, ...)
{
    _cr->set_driver(this, false);
    _csocket_state = csocket_failed;
    if (format) {
	va_list val;
	va_start(val, format);
	gerrh->xmessage(ErrorHandler::e_error, format, val);
	va_end(val);
    }

    _cr->on_error(true, gerrh->message_string(gerrh->begin() + begin_pos, gerrh->end()));

    return FALSE;
}

gboolean csocket_cdriver::csocket_event(GIOCondition)
{
    int fd = (_csocket ? g_io_channel_unix_get_fd(_csocket) : -1);
    _csocket_watch = (guint) -1;
    GatherErrorHandler *gerrh = _cr->error_handler();
    int gerrh_pos = gerrh->size();

    if (_csocket_state == csocket_connecting) {
	int r = do_fd_connected(fd, gerrh);
	if (r < 0)
	    return kill_with_dialog(gerrh, gerrh_pos, 0);
	else if (r == 0) {
	    _csocket_watch = g_io_add_watch(_csocket, (GIOCondition) (G_IO_OUT | G_IO_ERR | G_IO_HUP), csocket_watch, this);
	    return FALSE;
	} else
	    _csocket_state = csocket_initial;
    }

    if (_csocket_state == csocket_initial) {
	gchar *str;
	gsize len;
	GError *err = NULL;
	GIOStatus s = g_io_channel_read_line(_csocket, &str, &len, 0, &err);
	if (s == G_IO_STATUS_AGAIN) {
	    _csocket_watch = g_io_add_watch(_csocket, G_IO_IN, csocket_watch, this);
	    return FALSE;
	} else if (s == G_IO_STATUS_ERROR) {
	    kill_with_dialog(gerrh, gerrh_pos, "Connection error: %s", err->message);
	    g_error_free(err);
	    return FALSE;
	} else {
	    if (len < 24 || memcmp(str, "Click::ControlSocket/1.", 23) != 0
		|| cp_integer(str + 23, str + len, 10, &_csocket_minor_version) == str + 23) {
		gerrh->error("This connection is not a Click ControlSocket.");
		gerrh->message("(Expected a greeting of 'Click::ControlSocket/1.x'; got '%s'.)", str);
		return kill_with_dialog(gerrh, gerrh_pos, 0);
	    }

	    _csocket_state = csocket_connected;
	    g_io_channel_set_buffered(_csocket, FALSE);
	    do_read("config", String(), 0);
	    do_check_write("hotconfig", 0);
	    do_read("list", String(), 0);
	    _cr->on_driver_connected();
	}
    }

    if (_csocket_state == csocket_connected) {
	// write phase
	for (size_t i = 0; i < _csocket_msgq.size() && i < 10; i++) {
	    msg *m = _csocket_msgq[i];
	    if (m->wpos == m->command.length())
		continue;
	    gsize len;
	    GError *err = NULL;
	    GIOStatus s = g_io_channel_write_chars(_csocket, m->command.data() + m->wpos, m->command.length() - m->wpos, &len, &err);
	    m->wpos += len;
	    if (s == G_IO_STATUS_AGAIN)
		break;
	    else if (s == G_IO_STATUS_ERROR) {
		kill_with_dialog(gerrh, gerrh_pos, "Connection error: %s", err->message);
		g_error_free(err);
		return FALSE;
	    } else
		assert(m->wpos == m->command.length());
	}

	// read phase
	while (_csocket_msgq.size() && _csocket_state == csocket_connected) {
	    msg *m = _csocket_msgq.front();
	    if (m->wpos < m->command.length())
		break;
	    if (msg_parse(m, gerrh, gerrh_pos))
		continue;
	    gsize len;
	    GError *err = NULL;
	    GIOStatus s = g_io_channel_read_chars(_csocket, m->sa.reserve(4096), 4096, &len, &err);
	    m->sa.adjust_length(len);
	    if (s == G_IO_STATUS_AGAIN && len > 0)
		continue;	// parse message again
	    else if (s == G_IO_STATUS_AGAIN)
		break;
	    else if (s == G_IO_STATUS_ERROR) {
		kill_with_dialog(gerrh, gerrh_pos, "Connection error: %s", err->message);
		return FALSE;
	    } else if (s == G_IO_STATUS_EOF) {
		kill_with_dialog(gerrh, gerrh_pos, "Connection error: %s", "end of file");
		return FALSE;
	    }
	}

	// register callback if we get this far
	assert(_csocket_watch = (guint) -1);
	int condition = 0;
	for (size_t i = 0; i < _csocket_msgq.size() && i < 10; i++)
	    if (_csocket_msgq[i]->wpos != _csocket_msgq[i]->command.length())
		condition |= G_IO_OUT;
	if (_csocket_msgq.size() && _csocket_msgq.front()->wpos == _csocket_msgq.front()->command.length())
	    condition |= G_IO_IN;
	if (condition && _csocket_state == csocket_connected)
	    _csocket_watch = g_io_add_watch(_csocket, (GIOCondition) condition, csocket_watch, this);
	else
	    _csocket_watch = 0;
    }

    return FALSE;
}

bool csocket_cdriver::msg_parse(msg *m, GatherErrorHandler *gerrh, int gerrh_pos)
{
    const char *ends = m->sa.data() + m->sa.length(); // beware!

    // read lines
    while (m->rendmsgpos == (size_t) -1 || m->rdatapos == (size_t) -1) {
	// read a line
	const char *line = m->sa.data() + m->rlinepos;
	const char *s = line;
	while (s != ends && *s != '\r' && *s != '\n')
	    s++;
	if (s == ends)
	    return false;
	else if (line == s && *s == '\n' && m->ignore_newline) {
	    m->rlinepos++;
	    m->ignore_newline = false;
	    continue;
	}
	m->ignore_newline = (*s == '\r');
	const char *before_eol = s;
	s++;

	// check DATA syntax
	if (m->rendmsgpos != (size_t) -1) {
	    if (line + 6 > ends
		|| line[0] != 'D' || line[1] != 'A' || line[2] != 'T'
		|| line[3] != 'A' || line[4] != ' '
		|| !cp_integer(line + 5, before_eol, 0, &m->rdatalen)) {
		kill_with_dialog(gerrh, gerrh_pos, "Connection error: missing data");
		return true;
	    }
	    m->rdatapos = s - m->sa.data();
	    break;
	}

	// check message syntax
	if (line + 4 > ends
	    || !isdigit((unsigned char) line[0])
	    || !isdigit((unsigned char) line[1])
	    || !isdigit((unsigned char) line[2])
	    || (line[3] != '-' && line[3] != ' ')) {
	    kill_with_dialog(gerrh, gerrh_pos, "Connection error: garbled message");
	    return true;
	}

	// maybe this line actually ends the message
	m->rlinepos = s - m->sa.data();
	if (line[3] == ' ') {
	    m->rendmsgpos = m->rlinepos;
	    if ((m->command[0] != 'R' && m->command[0] != 'L') // READ, LLRPC
		|| line[0] != '2') {
		m->rdatapos = m->rlinepos;
		m->rdatalen = 0;
	    }
	}
    }

    // skip ignorable newline
    if (m->ignore_newline && m->rdatapos < (size_t) m->sa.length()) {
	if (m->sa[m->rdatapos] == '\n')
	    m->rdatapos++;
	m->ignore_newline = false;
    }

    // read data
    if (m->rdatapos + m->rdatalen > (size_t) m->sa.length())
	return false;

    // append leftover data to next message
    if (m->rdatapos + m->rdatalen < (size_t) m->sa.length()) {
	if (_csocket_msgq.size() == 1
	    || _csocket_msgq[1]->wpos != _csocket_msgq[1]->command.length()) {
	    kill_with_dialog(gerrh, gerrh_pos, "Connection error: too much data");
	    // ... but continue.
	} else {
	    size_t delta = m->sa.length() - (m->rdatapos + m->rdatalen);
	    _csocket_msgq[1]->sa.append(m->sa.data() + m->rdatapos + m->rdatalen, delta);
	    m->sa.adjust_length(-delta);
	}
    }

    // otherwise, actually have a message; parse it
    String response = m->sa.take_string();
    int status = (response[0] - '0') * 100 + (response[1] - '0') * 10 + (response[2] - '0');

    messagevector messages;
    const char *end = response.begin() + m->rdatapos;
    for (const char *pos = response.begin(); pos != end; ) {
	// grab current line
	const char *sol = pos;
	const char *space = 0;
	while (pos != end && *pos != '\n' && *pos != '\r') {
	    if (*pos == ' ' && sol + 4 < pos && space == 0)
		space = pos;
	    ++pos;
	}
	// extract landmark and send message
	if (sol + 4 >= pos) {
	    gerrh->error("Garbled response!");
	    break;
	} else if (space && space[-1] == ':')
	    messages.push_back(make_pair(response.substring(sol + 4, space - 1), response.substring(space + 1, pos)));
	else
	    messages.push_back(make_pair(String(), response.substring(sol + 4, pos)));
	if (pos + 2 <= end && pos[0] == '\r' && pos[1] == '\n')
	    pos += 2;
	else if (pos < end)
	    pos++;
    }

    // act on results
    String hparam;
    if (m->command_datalen >= 0)
	hparam = m->command.substring(m->command.length() - m->command_datalen);
    else			// leave off "\r\n"
	hparam = m->command.substring(m->command.length() + m->command_datalen - 2, m->command_datalen - 2);
    String hvalue = response.substring(m->rdatapos);
    if (m->type == dtype_read)
	_cr->on_handler_read(m->hname, hparam, hvalue, status, messages);
    else if (m->type == dtype_write)
	_cr->on_handler_write(m->hname, hparam, status, messages);
    else if (m->type == dtype_check_write)
	_cr->on_handler_check_write(m->hname, status, messages);

    transfer_messages(_cr, status, messages);

    delete m;
    _csocket_msgq.pop_front();
    return true;
}

void csocket_cdriver::add_msg(const String &hname, const String &command, int command_datalen, int type, int flags)
{
    if (_csocket && _csocket_state == csocket_connected) {
	//fprintf(stderr, "%s", command.c_str());
	msg *csm = new msg(_cr, hname, command, command_datalen, type, flags);

	if ((flags & (dflag_background | dflag_clear)) == dflag_background)
	    _csocket_msgq.push_back(csm);
	else {
	    std::deque<msg *>::iterator qi = _csocket_msgq.end();
	    while (qi != _csocket_msgq.begin()
		   && qi[-1]->wpos == 0
		   && (qi[-1]->flags & dflag_background))
		--qi;
	    if (flags & dflag_clear)
		qi = _csocket_msgq.erase(qi, _csocket_msgq.end());
	    _csocket_msgq.insert(qi, csm);
	}
	if (_csocket_watch == 0)
	    _csocket_watch = g_io_add_watch(_csocket, G_IO_OUT, csocket_watch, this);
    }
}


void csocket_cdriver::do_read(const String &hname, const String &hparam, int flags)
{
    StringAccum sa;
    int hparam_len = hparam.length();
    if (!hparam)
	sa << "READ " << hname << "\r\n";
    else if (_csocket_minor_version >= 2)
	sa << "READDATA " << hname << " " << hparam.length() << "\r\n" << hparam;
    else if (find(hparam, '\r') == hparam.end()
	     && find(hparam, '\n') == hparam.end()) {
	sa << "READ " << hname << " " << hparam << "\r\n";
	hparam_len = -hparam_len;
    } else {
	messagevector messages;
	messages.push_back(make_pair(String(), String("Read handler '") + hname + "' error:"));
	messages.push_back(make_pair(String(), String("  Connection does not support read handlers with parameters")));
	_cr->on_handler_read(hname, hparam, String(), 500, messages);
	transfer_messages(_cr, 500, messages);
	return;
    }
    add_msg(hname, sa.take_string(), hparam_len, dtype_read, flags);
}

void csocket_cdriver::do_write(const String &hname, const String &hvalue, int flags)
{
    StringAccum sa;
    sa << "WRITEDATA " << hname << " " << hvalue.length() << "\r\n" << hvalue;
    add_msg(hname, sa.take_string(), hvalue.length(), dtype_write, flags);
}

void csocket_cdriver::do_check_write(const String &hname, int flags)
{
    add_msg(hname, "CHECKWRITE " + hname + "\r\n", 0, dtype_check_write, flags);
}


/*****
 *
 * Kernel driver
 *
 */

clickfs_cdriver::clickfs_cdriver(crouter *cr, const String &prefix)
    : _cr(cr), _prefix(prefix), _active(true)
{
    _cr->set_driver(this, true);
    assert(_prefix.length());
    if (_prefix.back() != '/')
	_prefix += '/';
    String prefix_h = _prefix + ".h";
    _dot_h = (access(prefix_h.c_str(), F_OK) >= 0);
    do_read("config", String(), 0);
    do_check_write("hotconfig", 0);
    do_read("list", String(), 0);
    _cr->on_driver_connected();
}

bool clickfs_cdriver::active() const
{
    return _active;
}

int clickfs_cdriver::driver_mask() const
{
    return 1 << Driver::LINUXMODULE;
}

void clickfs_cdriver::complain(const String &fullname, const String &ename, const String &, int errno_val, messagevector &messages)
{
    if (errno_val == ENOENT) {
	if (access(_prefix.c_str(), F_OK) < 0) {
	    messages.push_back(make_pair(String(), String("No router installed")));
	    _cr->set_driver(this, false);
	    _active = false;
	} else if (ename && access((_prefix + ename).c_str(), F_OK) < 0)
	    messages.push_back(make_pair(String(), "No element named '" + ename + "'"));
	else
	    messages.push_back(make_pair(String(), "No handler named '" + fullname + "'"));
    } else if (errno_val == EACCES)
	messages.push_back(make_pair(String(), String("Permission denied")));
    else
	messages.push_back(make_pair(String(), "Handler error: " + String(strerror(errno_val))));
}

String clickfs_cdriver::filename(const String &ename, const String &hname) const
{
    StringAccum sa;
    sa << _prefix << ename;
    if (ename)
	sa << '/';
    if (_dot_h)
	sa << ".h/";
    sa << hname;
    return sa.take_string();
}

void clickfs_cdriver::do_read(const String &fullname, const String &hparam, int flags)
{
    String ename, hname;
    if (!_active
	|| check_handler_name(fullname, ename, hname, _cr->error_handler()) < 0)
	return;

    messagevector messages;
    int status = 500;

    if (hparam) {
	messages.push_back(make_pair(String(), "Read handler '" + fullname + "' error:"));
	messages.push_back(make_pair(String(), "  Kernel configurations do not support read handler parameters"));
	_cr->on_handler_read(fullname, hparam, String(), 500, messages);
	transfer_messages(_cr, 500, messages);
	return;
    }

    String fn = filename(ename, hname);
    int fd = open(fn.c_str(), O_RDONLY);
    StringAccum results;
    if (fd >= 0) {
#if CLICK_LLRPC_RAW_HANDLER
	ioctl(fd, CLICK_LLRPC_RAW_HANDLER);
#endif
	while (1) {
	    ssize_t amt = read(fd, results.reserve(4096), 4096);
	    if (amt == -1 && errno != EINTR) {
		complain(fullname, ename, hname, errno, messages);
		close(fd);
		goto done;
	    } else if (amt == 0)
		break;
	    else if (amt != -1)
		results.adjust_length(amt);
	}

	if (close(fd) >= 0) {
	    messages.push_back(make_pair(String(), "Read handler '" + fullname + "' OK"));
	    status = 200;
	} else {
	    int errno_val = errno;
	    messages.push_back(make_pair(String(), "Read handler '" + fullname + "' error:"));
	    messages.push_back(make_pair(String(), String(strerror(errno_val))));
	    messages.push_back(make_pair(String(), "(Check " + _prefix + "errors for details.)"));
	}
    } else
	complain(fullname, ename, hname, errno, messages);

  done:
    if ((flags & dflag_nonraw) && results.length() && results.back() == '\n'
	&& find(results.begin(), results.end() - 1, '\n') == results.end() - 1)
	results.pop_back();
    _cr->on_handler_read(fullname, hparam, results.take_string(), status, messages);
    transfer_messages(_cr, status, messages);
}

void clickfs_cdriver::do_write(const String &fullname, const String &hvalue, int)
{
    String ename, hname;
    if (!_active
	|| check_handler_name(fullname, ename, hname, _cr->error_handler()) < 0)
	return;

    messagevector messages;
    int status = 500;

    String fn = filename(ename, hname);
    int fd = open(fn.c_str(), O_WRONLY | O_TRUNC);
    if (fd >= 0) {
	const char *s = hvalue.begin();
	const char *end = hvalue.end();
	while (s < end) {
	    ssize_t amt = write(fd, s, end - s);
	    if (amt == -1 && errno != EINTR) {
		complain(fullname, ename, hname, errno, messages);
		close(fd);
		goto done;
	    } else if (amt >= 0)
		s += amt;
	}

#if CLICK_LLRPC_CALL_HANDLER
	char buf[2048];
	click_llrpc_call_handler_st chs;
# if CLICK_LLRPC_CALL_HANDLER_FLAG_RAW
	chs.flags = CLICK_LLRPC_CALL_HANDLER_FLAG_RAW;
# endif
	chs.errorbuf = buf;
	chs.errorlen = sizeof(buf);
	if (ioctl(fd, CLICK_LLRPC_CALL_HANDLER, &chs) >= 0)
	    status = 200;
	messages.push_back(make_pair(String(), String(buf, std::min(chs.errorlen, sizeof(buf)))));
#else
	if (close(fd) >= 0) {
	    messages.push_back(make_pair(String(), "Write handler '" + fullname + "' OK"));
	    status = 200;
	} else {
	    int errno_val = errno;
	    messages.push_back(make_pair(String(), "Write handler '" + fullname + "' error:"));
	    messages.push_back(make_pair(String(), String(strerror(errno_val))));
	    messages.push_back(make_pair(String(), "(Check " + _prefix + "errors for details.)"));
	}
#endif
    } else
	complain(fullname, ename, hname, errno, messages);

  done:
    _cr->on_handler_write(fullname, hvalue, status, messages);
    transfer_messages(_cr, status, messages);
}

void clickfs_cdriver::do_check_write(const String &fullname, int)
{
    String ename, hname;
    if (!_active
	|| check_handler_name(fullname, ename, hname, _cr->error_handler()) < 0)
	return;

    messagevector messages;
    int status = 500;

    String fn = filename(ename, hname);
    if (access(fn.c_str(), W_OK) >= 0) {
	// If accessible, it still might be a directory rather than a handler.
	struct stat buf;
	stat(fn.c_str(), &buf);
	if (!S_ISDIR(buf.st_mode)) {
	    messages.push_back(make_pair(String(), "Write handler '" + fullname + "' OK"));
	    status = 200;
	} else
	    messages.push_back(make_pair(String(), "No handler named '" + fullname + "'"));
    } else
	complain(fullname, ename, hname, errno, messages);

    _cr->on_handler_check_write(fullname, status, messages);
    transfer_messages(_cr, status, messages);
}

}
