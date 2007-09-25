#ifndef CLICKY_WHANDLER_HH
#define CLICKY_WHANDLER_HH 1
#include <deque>
#include <click/string.hh>
#include <gtk/gtk.h>
#include <click/hashmap.hh>
#include "wrouter.hh"
class RouterWindow;

struct RouterWindow::whandler {

    whandler(RouterWindow *rw);
    ~whandler();

    void clear();

    RouterWindow *router_window() const {
	return _rw;
    }
    bool active() const {
	return _rw->driver() != 0;
    }
    GtkBox *handler_box() const {
	return _handlerbox;
    }
    
    void display(const String &ename, bool incremental);

    void notify_element(const String &ename);
    void notify_handlers(const String &ename, const String &data);
    void notify_read(const String &hname, const String &data, bool real = true);

    void refresh_all();
    void show_actions(GtkWidget *near, const String &hname);
    void hide_actions(const String &hname = String(), bool restore = true);
    void apply_action(const String &hname, bool activate);
    const String &active_action() const;
    
  private:

    enum { hflag_r = 1 << 0, hflag_w = 1 << 1, hflag_ready = 1 << 2,
	   hflag_boring = 1 << 3, hflag_multiline = 1 << 4,
	   hflag_collapse = 1 << 5, hflag_collapse_expanded = 1 << 6,
	   hflag_expensive = 1 << 7, hflag_shown = 1 << 8,
	   hflag_button = 1 << 9, hflag_calm = 1 << 10,
	   hflag_checkbox = 1 << 11, hflag_raw = 1 << 12 };
    struct hinfo {
	String fullname;
	String name;
	int flags;
	GtkWidget *wcontainer;
	GtkWidget *wdata;
	hinfo(const String &e, const String &n, int f)
	    : fullname(e ? e + "." + n : n), name(n), flags(f),
	      wcontainer(0), wdata(0) {
	}
	void widget_create(RouterWindow::whandler *wh, int position, int new_flags);
	void widget_set_data(RouterWindow::whandler *wh, const String &data, bool change_form, int position);
    };
    
    RouterWindow *_rw;
    HashMap<String, String> _ehandlers;
    HashMap<String, String> _hvalues;
    std::deque<hinfo> _hinfo;
    GtkBox *_handlerbox;
    String _display_ename;
    
    GtkWidget *_actions;
    GtkWidget *_actions_apply;
    String _actions_hname;
    int _updating;

    GtkWidget *_eview_config;

    void make_actions();
    
};

inline void RouterWindow::whandler::notify_element(const String &ename)
{
    _ehandlers.insert(ename, String());
}

inline const String &RouterWindow::whandler::active_action() const
{
    return _actions_hname;
}

#endif
