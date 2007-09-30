#ifndef CLICKY_WHANDLER_HH
#define CLICKY_WHANDLER_HH 1
#include <deque>
#include <click/string.hh>
#include <gtk/gtk.h>
#include "hashmap1.hh"
#include "wrouter.hh"
namespace clicky {
class handler_value;
class handler_values;
class handler_value_iterator;

enum {
    hflag_r = 1 << 0,
    hflag_w = 1 << 1,
    hflag_rparam = 1 << 2,
    hflag_raw = 1 << 3,
    hflag_special = 1 << 4,
    hflag_dead = 1 << 5,
    
    hflag_calm = 1 << 6,
    hflag_expensive = 1 << 7,
    hflag_multiline = 1 << 8,
    hflag_button = 1 << 9,
    hflag_checkbox = 1 << 10,
    hflag_collapse = 1 << 11,
    hflag_visible = 1 << 12,
    hflag_refresh = 1 << 13,
    
    hflag_autorefresh = 1 << 14,
    hflag_autorefresh_outstanding = 1 << 15,
    hflag_have_hvalue = 1 << 16,

    hflag_preferences = 1 << 17,

    hflag_mandatory_driver_mask = hflag_r | hflag_w | hflag_rparam | hflag_raw
    | hflag_special | hflag_dead,
    hflag_default_driver_mask = hflag_mandatory_driver_mask
    | hflag_calm | hflag_expensive | hflag_multiline
    | hflag_button | hflag_checkbox | hflag_collapse
    | hflag_visible | hflag_refresh
};

class handler_value { public:

    typedef String key_type;

    handler_value(const String &hname)
	: _hname(hname), _flags(0), _driver_flags(0),
	  _driver_mask(hflag_default_driver_mask), _autorefresh_period(1000),
	  _autorefresh_source(0), _next(0) {
	_name_offset = hname.find_right('.');
	_name_offset = (_name_offset < 0 ? 0 : _name_offset + 1);
    }
    ~handler_value() {
	if (_autorefresh_source)
	    g_source_remove(_autorefresh_source);
    }

    const String &hashkey() const {
	return _hname;
    }

    const String &hname() const {
	return _hname;
    }
    String leaf_name() const {
	return _hname.substring(_name_offset);
    }
    
    const String &hparam() const {
	return _hparam;
    }
    bool have_required_hparam() const {
	return (_flags & (hflag_rparam | hflag_have_hvalue)) != hflag_rparam;
    }
    
    const String &hvalue() const {
	return _hvalue;
    }
    bool have_hvalue() const {
	return (_flags & hflag_have_hvalue) != 0;
    }
    void clear_hvalue() {
	_hvalue = String();
	_flags &= ~hflag_have_hvalue;
    }
    void set_hvalue(const String &hvalue) {
	_hvalue = hvalue;
	_flags |= hflag_have_hvalue;
    }
    
    bool readable() const {
	return (_flags & hflag_r) != 0;
    }
    bool read_param() const {
	return (_flags & hflag_rparam) != 0;
    }
    bool writable() const {
	return (_flags & hflag_w) != 0;
    }
    bool editable() const {
	return (_flags & (hflag_w | hflag_rparam)) != 0;
    }
    bool write_only() const {
	return (_flags & (hflag_r | hflag_w)) == hflag_w;
    }
    bool refreshable() const {
	return (_flags & hflag_refresh) != 0;
    }
    static bool default_refreshable(int flags) {
	return (flags & (hflag_r | hflag_special | hflag_expensive)) == hflag_r;
    }

    int flags() const {
	return _flags;
    }
    int autorefresh_period() const {
	return _autorefresh_period;
    }

    void set_autorefresh_period(int p) {
	_autorefresh_period = p;
    }

    void refresh(wmain *w);
    
    gboolean on_autorefresh(wmain *w);

    void set_driver_flags(wmain *w, int new_flags);

    void set_flags(wmain *w, int new_flags);

    static const String no_hvalue_string;
    
  private:
    
    String _hname;
    int _name_offset;
    String _hparam;
    String _hvalue;
    int _flags;
    int _driver_flags;
    int _driver_mask;
    int _autorefresh_period;
    guint _autorefresh_source;
    handler_value *_next;

    friend class handler_values;
    friend class handler_value_iterator;

    void change_flags(wmain *w);
    
};

struct handler_value_iterator {

    inline void operator++(int) {
	_v = _v->_next;
    }
    inline void operator++() {
	_v = _v->_next;
    }

    inline handler_value *operator->() const {
	return _v;
    }
    inline handler_value &operator*() const {
	return *_v;
    }

  private:

    inline handler_value_iterator(handler_value *v)
	: _v(v) {
    }

    handler_value *_v;

    friend class handler_values;
    friend bool operator==(handler_value_iterator, handler_value_iterator);
    friend bool operator!=(handler_value_iterator, handler_value_iterator);
    
};

inline bool operator==(handler_value_iterator a, handler_value_iterator b) {
    return a._v == b._v;
}

inline bool operator!=(handler_value_iterator a, handler_value_iterator b) {
    return a._v != b._v;
}


struct handler_values {
    
    handler_values(wmain *w);

    typedef handler_value_iterator iterator;
    
    iterator begin(const String &ename);
    
    inline iterator end() {
	return iterator(0);
    }

    handler_value *find(const String &hname) {
	return _hv.find(hname).operator->();
    }

    inline bool empty() const {
	return _hv.empty();
    }

    void clear() {
	_hv.clear();
    }
    
    handler_value *set(const String &hname, const String &hparam, const String &hvalue);
    
  private:
    
    wmain *_w;
    HashMap<handler_value> _hv;
    HashMap<String, int> _class_uflags;

    void set_handlers(const String &hname, const String &hparam, const String &hvalue);
    
};


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

    void notify_element(const String &ename);
    void notify_read(const String &hname, const String &hparam, const String &hvalue);
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
    handler_values _hv;
    
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
    
};

inline void whandler::notify_element(const String &)
{
}

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
	    gtk_label_set_text(GTK_LABEL(wlabel), hv->leaf_name().c_str());
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

inline void handler_value::set_driver_flags(wmain *w, int new_driver_flags)
{
    _driver_flags = new_driver_flags;
    int new_flags = (_flags & ~_driver_mask) | (_driver_flags & _driver_mask);
    if (new_flags != _flags)
	set_flags(w, new_flags);
}

}
#endif
