#ifndef CLICKY_CDRIVER_HH
#define CLICKY_CDRIVER_HH 1
#include "crouter.hh"
namespace clicky {

class cdriver { public:

    virtual ~cdriver() { }

    enum { dflag_background = 1, dflag_clear = 2, dflag_nonraw = 4 };

    virtual bool active() const = 0;
    virtual int driver_mask() const = 0;

    virtual void do_read(const String &hname, const String &hparam, int flags) = 0;
    virtual void do_write(const String &hname, const String &hvalue, int flags) = 0;
    virtual void do_check_write(const String &hname, int flags) = 0;

    static int check_handler_name(const String &inname, String &ename, String &hname, ErrorHandler *errh);
    static void transfer_messages(crouter *rw, int status, const messagevector &messages);

};


class csocket_cdriver : public cdriver { public:

    csocket_cdriver(crouter *rw, GIOChannel *channel, bool ready);
    ~csocket_cdriver();

    static int make_nonblocking(int fd, ErrorHandler *errh);
    static GIOChannel *start_connect(IPAddress addr, uint16_t port, bool *ready, ErrorHandler *errh);

    bool active() const;
    int driver_mask() const;

    void do_read(const String &hname, const String &hparam, int flags);
    void do_write(const String &hname, const String &hvalue, int flags);
    void do_check_write(const String &hname, int flags);

    // actually private
    gboolean csocket_event(GIOCondition);

  private:

    enum { csocket_failed, csocket_connecting, csocket_initial,
	   csocket_connected };
    enum { dtype_read = 1, dtype_write = 2, dtype_check_write = 3 };

    struct msg {
	crouter::throb_after tnotify;
	int type;
	int flags;
	String hname;
	String command;
	int command_datalen;
	int wpos;
	StringAccum sa;
	size_t rlinepos;
	size_t rendmsgpos;
	size_t rdatapos;
	size_t rdatalen;
	bool ignore_newline;

	msg(crouter *cr_, const String &hname_, const String &command_, int command_datalen_, int type_, int flags_)
	    : tnotify(cr_, 400), type(type_), flags(flags_), hname(hname_),
	      command(command_), command_datalen(command_datalen_),
	      wpos(0), rlinepos(0), rendmsgpos((size_t) -1),
	      rdatapos((size_t) -1), rdatalen(0), ignore_newline(false) {
	}
    };

    crouter *_cr;
    GIOChannel *_csocket;
    guint _csocket_watch;
    int _csocket_state;
    int _csocket_minor_version;
    std::deque<msg *> _csocket_msgq;

    gboolean kill_with_dialog(GatherErrorHandler *gerrh, int gerrh_pos, const char *format, ...);
    bool msg_parse(msg *m, GatherErrorHandler *gerrh, int gerrh_pos);
    void add_msg(const String &hname, const String &command, int command_datalen, int type, int flags);

};


class clickfs_cdriver : public cdriver { public:

    clickfs_cdriver(crouter *cr, const String &prefix);

    bool active() const;
    int driver_mask() const;

    void do_read(const String &hname, const String &hparam, int flags);
    void do_write(const String &hname, const String &hvalue, int flags);
    void do_check_write(const String &hname, int flags);

  private:

    crouter *_cr;
    String _prefix;
    bool _active;
    bool _dot_h;

    String filename(const String &ename, const String &hname) const;
    void complain(const String &fullname, const String &ename, const String &hname, int errno_val, messagevector &messages);

};

}
#endif
