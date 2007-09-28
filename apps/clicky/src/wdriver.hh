#ifndef CLICKY_WDRIVER_HH
#define CLICKY_WDRIVER_HH 1
#include "wrouter.hh"

class RouterWindow::wdriver { public:

    virtual ~wdriver() { }
    
    enum { dflag_background = 1, dflag_clear = 2, dflag_nonraw = 4 };

    virtual int driver_mask() const = 0;

    virtual void do_read(const String &hname, const String &hparam, int flags) = 0;
    virtual void do_write(const String &hname, const String &hvalue, int flags) = 0;
    virtual void do_check_write(const String &hname, int flags) = 0;

    static int check_handler_name(const String &inname, String &ename, String &hname, ErrorHandler *errh);
    static void transfer_messages(RouterWindow *rw, int status, const messagevector &messages);
    
};


class RouterWindow::wdriver_csocket : public RouterWindow::wdriver { public:

    wdriver_csocket(RouterWindow *rw, GIOChannel *socket, bool ready);
    ~wdriver_csocket();

    static GIOChannel *start_connect(IPAddress addr, uint16_t port, bool *ready, ErrorHandler *errh);

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
	throb_after tnotify;
	int type;
	int flags;
	String hname;
	String command;
	int wpos;
	StringAccum sa;
	size_t rlinepos;
	size_t rendmsgpos;
	size_t rdatapos;
	size_t rdatalen;
	bool ignore_newline;
	
	msg(RouterWindow *rw_, const String &hname_, const String &command_, int type_, int flags_)
	    : tnotify(rw_, 400), type(type_), flags(flags_), hname(hname_),
	      command(command_), wpos(0), rlinepos(0), rendmsgpos((size_t) -1),
	      rdatapos((size_t) -1), rdatalen(0), ignore_newline(false) {
	}
    };

    RouterWindow *_rw;
    GIOChannel *_csocket;
    guint _csocket_watch;
    int _csocket_state;
    int _csocket_minor_version;
    std::deque<msg *> _csocket_msgq;

    gboolean kill_with_dialog(GatherErrorHandler *gerrh, int gerrh_pos, const char *format, ...);
    bool msg_parse(msg *m, GatherErrorHandler *gerrh, int gerrh_pos);
    void add_msg(const String &hname, const String &contents, int type, int flags);
    
};


class RouterWindow::wdriver_kernel : public RouterWindow::wdriver { public:

    wdriver_kernel(RouterWindow *rw, const String &prefix);

    int driver_mask() const;
    
    void do_read(const String &hname, const String &hparam, int flags);
    void do_write(const String &hname, const String &hvalue, int flags);
    void do_check_write(const String &hname, int flags);

  private:
    
    RouterWindow *_rw;
    String _prefix;
    bool _active;

    String filename(const String &ename, const String &hname) const;
    void complain(const String &fullname, const String &ename, const String &hname, int errno_val, messagevector &messages);

};

#endif
