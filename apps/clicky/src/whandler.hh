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
    void notify_read(const String &hname, const String &hparam, const String &hvalue, bool real = true);
    void notify_write(const String &hname, const String &hvalue, int status);

    void refresh_all();
    bool on_autorefresh(const String &hname); 
    void show_actions(GtkWidget *near, const String &hname, bool changed);
    void hide_actions(const String &hname = String(), bool restore = true);
    void apply_action(const String &hname, bool activate);
    const String &active_action() const;

    void recalculate_positions();

    static const gchar *hpref_handler_name(GtkWidget *w);
    void set_hinfo_flags(const String &hname, int flags, int flag_value);
    void set_hinfo_autorefresh_period(const String &hname, int period);

    enum { onpref_initial, onpref_showpref, onpref_prefok, onpref_prefcancel };
    void on_preferences(int action);
    
    enum {
	hflag_r = 1 << 0,
	hflag_w = 1 << 1,
	hflag_rparam = 1 << 2,
	hflag_raw = 1 << 3,
	hflag_calm = 1 << 4,
	hflag_expensive = 1 << 5,
	hflag_boring = 1 << 6,
	hflag_multiline = 1 << 7,
	hflag_collapse = 1 << 8,
	hflag_button = 1 << 9,
	hflag_checkbox = 1 << 10,
	hflag_visible = 1 << 11,
	hflag_refresh = 1 << 12,
	hflag_autorefresh = 1 << 13,
	hflag_special = 1 << 14,
	
	hflag_hparam_displayed = 1 << 15,
	hflag_preferences = 1 << 16,
	hflag_autorefresh_outstanding = 1 << 17
    };
    
  private:
    
    struct hinfo {
	String fullname;
	String name;
	int flags;
	GtkWidget *wcontainer;
	GtkWidget *wlabel;
	GtkWidget *wdata;
	int wposition;
	int autorefresh;
	guint autorefresh_source;
	
	hinfo(const String &e, const String &n, int f)
	    : fullname(e ? e + "." + n : n), name(n), flags(f),
	      wcontainer(0), wlabel(0), wdata(0), wposition(0),
	      autorefresh(1000), autorefresh_source(0) {
	}
	~hinfo() {
	    if (autorefresh_source)
		g_source_remove(autorefresh_source);
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
	    return (flags & hflag_refresh) != 0;
	}
	static bool default_refreshable(int flags) {
	    return (flags & (hflag_r | hflag_rparam | hflag_boring | hflag_expensive)) == hflag_r;
	}
	
	void create(RouterWindow::whandler *wh, int new_flags);
	void start_autorefresh(whandler *wh);
	
	void set_edit_active(const RouterWindow *rw, bool active);
	void display(RouterWindow::whandler *wh, const String &hparam, const String &hvalue, bool change_form);
	
      private:
	int create_preferences(RouterWindow::whandler *wh);
	int create_display(RouterWindow::whandler *wh);
    };
    
    RouterWindow *_rw;
    HashMap<String, String> _ehandlers;
    HashMap<String, String> _hvalues;
    std::deque<hinfo> _hinfo;
    GtkBox *_handlerbox;
    GtkButtonBox *_hpref_actions;
    String _display_ename;
    
    GtkWidget *_actions[2];
    GtkWidget *_actions_apply[2];
    String _actions_hname;
    bool _actions_changed;
    int _updating;
    
    GtkWidget *_eview_config;

    void make_actions(int which);
    inline const hinfo *find_hinfo(const String &hname) const;
    inline hinfo *find_hinfo(const String &hname);
    
};

inline void RouterWindow::whandler::notify_element(const String &ename)
{
    _ehandlers.insert(ename, String());
}

inline const String &RouterWindow::whandler::active_action() const
{
    return _actions_hname;
}

inline void RouterWindow::whandler::hinfo::set_edit_active(const RouterWindow *rw, bool active)
{
    if (wlabel && !active)
	gtk_label_set_attributes(GTK_LABEL(wlabel), rw->small_attr());
    else if (wlabel && active) {
	if (flags & hflag_hparam_displayed) {
	    gtk_label_set_text(GTK_LABEL(wlabel), name.c_str());
	    flags &= ~hflag_hparam_displayed;
	}
	gtk_label_set_attributes(GTK_LABEL(wlabel), rw->small_bold_attr());
    }
}

inline const RouterWindow::whandler::hinfo *RouterWindow::whandler::find_hinfo(const String &hname) const
{
    for (std::deque<hinfo>::const_iterator iter = _hinfo.begin();
	 iter != _hinfo.end(); ++iter)
	if (iter->fullname == hname)
	    return iter.operator->();
    return 0;
}

inline RouterWindow::whandler::hinfo *RouterWindow::whandler::find_hinfo(const String &hname)
{
    for (std::deque<hinfo>::iterator iter = _hinfo.begin();
	 iter != _hinfo.end(); ++iter)
	if (iter->fullname == hname)
	    return iter.operator->();
    return 0;
}

#endif
