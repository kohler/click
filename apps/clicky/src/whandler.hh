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
    void notify_write(const String &hname, const String &data, int status);

    void refresh_all();
    void show_actions(GtkWidget *near, const String &hname, bool changed);
    void hide_actions(const String &hname = String(), bool restore = true);
    void apply_action(const String &hname, bool activate);
    const String &active_action() const;
    
  private:

    enum { hflag_r = 1 << 0, hflag_w = 1 << 1, hflag_rparam = 1 << 2,
	   hflag_ready = 1 << 3, hflag_raw = 1 << 4, hflag_calm = 1 << 5,
	   hflag_expensive = 1 << 6, hflag_boring = 1 << 7,
	   hflag_multiline = 1 << 8, hflag_collapse = 1 << 9,
	   hflag_button = 1 << 10, hflag_checkbox = 1 << 11 };
    struct hinfo {
	String fullname;
	String name;
	int flags;
	GtkWidget *wcontainer;
	GtkWidget *wlabel;
	GtkWidget *wdata;
	int wposition;
	
	hinfo(const String &e, const String &n, int f)
	    : fullname(e ? e + "." + n : n), name(n), flags(f),
	      wcontainer(0), wlabel(0), wdata(0), wposition(-1) {
	}

	bool readable() const {
	    return (flags & hflag_r) != 0;
	}
	bool read_param() const {
	    return (flags & hflag_rparam) != 0;
	}
	bool writable() const {
	    return (flags & hflag_w) != 0;
	}
	bool editable() const {
	    return (flags & (hflag_w | hflag_rparam)) != 0;
	}
	bool write_only() const {
	    return (flags & (hflag_r | hflag_w)) == hflag_w;
	}
	bool refreshable() const {
	    return (flags & (hflag_r | hflag_rparam | hflag_boring | hflag_expensive)) == hflag_r;
	}

	void unhighlight(const RouterWindow *rw) const {
	    if (wlabel)
		gtk_label_set_attributes(GTK_LABEL(wlabel), rw->small_attr());
	}
	
	void widget_create(RouterWindow::whandler *wh, int new_flags);
	void widget_set_data(RouterWindow::whandler *wh, const String &data, bool change_form);
    };
    
    RouterWindow *_rw;
    HashMap<String, String> _ehandlers;
    HashMap<String, String> _hvalues;
    std::deque<hinfo> _hinfo;
    GtkBox *_handlerbox;
    String _display_ename;
    
    GtkWidget *_actions[2];
    GtkWidget *_actions_apply[2];
    String _actions_hname;
    bool _actions_changed;
    int _updating;
    
    GtkWidget *_eview_config;

    void make_actions(int which);
    
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
