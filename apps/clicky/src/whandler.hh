#ifndef CLICKY_WHANDLER_HH
#define CLICKY_WHANDLER_HH 1
#include <deque>
#include <click/string.hh>
#include <gtk/gtk.h>
#include <click/hashtable.hh>
#include "crouter.hh"
#include "wmain.hh"
#include "hvalues.hh"
namespace clicky {

struct whandler {

    whandler(wmain *rw);
    ~whandler();

    void clear();

    wmain *main() const {
	return _rw;
    }
    bool active() const {
	return _rw->driver() != 0;
    }
    GtkBox *handler_box() const {
	return _handlerbox;
    }

    void display(const String &ename, bool incremental);
    void redisplay();

    void notify_read(handler_value *hv);
    void notify_write(const String &hname, const String &hvalue, int status);

    void refresh(const String &hname, bool always);
    void refresh_all(bool always);
    bool on_autorefresh(const String &hname);
    void show_actions(GtkWidget *near, const String &hname, bool changed);
    void hide_actions(const String &hname = String(), bool restore = true);
    void apply_action(const String &hname, bool activate);
    const String &active_action() const;

    void recalculate_positions();

    void set_hinfo_flags(const String &hname, int flags, int flag_value);
    void set_hinfo_autorefresh_period(const String &hname, int period);

    enum { onpref_initial, onpref_showpref, onpref_prefok, onpref_prefcancel };
    void on_preferences(int action);

    static const char *widget_hname(GtkWidget *w);

  private:

    struct hinfo {
	handler_value *hv;
	GtkWidget *wcontainer;
	GtkWidget *wlabel;
	GtkWidget *wdata;
	int wposition;

	int _old_flags;
	int _old_autorefresh_period;

	hinfo(handler_value *hv_)
	    : hv(hv_), wcontainer(0), wlabel(0), wdata(0), wposition(0) {
	}

	bool readable() const {
	    return hv->readable();
	}
	bool writable() const {
	    return hv->writable();
	}
	bool editable() const {
	    return hv->editable();
	}

	void create(whandler *wh, int new_flags, bool always_position);

	void set_edit_active(wmain *rw, bool active);
	void display(whandler *wh, bool change_form);

      private:
	int create_preferences(whandler *wh);
	int create_display(whandler *wh);
    };

    wmain *_rw;

    std::deque<hinfo> _hinfo;
    String _display_ename;

    GtkWidget *_eviewbox;
    GtkBox *_handlerbox;
    GtkButtonBox *_hpref_actions;

    GtkWidget *_actions[2];
    GtkWidget *_actions_apply[2];
    String _actions_hname;
    bool _actions_changed;
    int _updating;

    GtkWidget *_eview_config;

    void make_actions(int which);
    inline bool hname_element_displayed(const String &hname) const;
    inline const hinfo *find_hinfo(const String &hname) const;
    inline hinfo *find_hinfo(const String &hname);
    inline hinfo *find_hinfo(handler_value *hv);

};

inline const String &whandler::active_action() const
{
    return _actions_hname;
}

inline void whandler::hinfo::set_edit_active(wmain *main, bool active)
{
    if (wlabel && !active)
	gtk_label_set_attributes(GTK_LABEL(wlabel), main->small_attr());
    else if (wlabel && active) {
	if (hv->read_param()) {
	    gtk_label_set_text(GTK_LABEL(wlabel), hv->handler_name().c_str());
	    hv->clear_hvalue();
	}
	gtk_label_set_attributes(GTK_LABEL(wlabel), main->small_bold_attr());
    }
}

inline bool whandler::hname_element_displayed(const String &hname) const
{
    return (_display_ename
	    && _display_ename.length() + 1 < hname.length()
	    && memcmp(_display_ename.begin(), hname.begin(), _display_ename.length()) == 0
	    && hname[_display_ename.length()] == '.');
}

inline const whandler::hinfo *whandler::find_hinfo(const String &hname) const
{
    if (hname_element_displayed(hname))
	for (std::deque<hinfo>::const_iterator iter = _hinfo.begin();
	     iter != _hinfo.end(); ++iter)
	    if (iter->hv->hname() == hname)
		return iter.operator->();
    return 0;
}

inline whandler::hinfo *whandler::find_hinfo(const String &hname)
{
    if (hname_element_displayed(hname))
	for (std::deque<hinfo>::iterator iter = _hinfo.begin();
	     iter != _hinfo.end(); ++iter)
	    if (iter->hv->hname() == hname)
		return iter.operator->();
    return 0;
}

inline whandler::hinfo *whandler::find_hinfo(handler_value *hv)
{
    for (std::deque<hinfo>::iterator iter = _hinfo.begin();
	 iter != _hinfo.end(); ++iter)
	if (iter->hv == hv)
	    return iter.operator->();
    return 0;
}

}
#endif
