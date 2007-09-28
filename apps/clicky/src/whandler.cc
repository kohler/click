#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "whandler.hh"
#include "wdriver.hh"
#include <gdk/gdkkeysyms.h>
#include <click/confparse.hh>
extern "C" {
#include "interface.h"
#include "support.h"
}

extern "C" {
static gboolean on_handler_event(GtkWidget *, GdkEvent *, gpointer);
static void on_handler_entry_changed(GObject *, GParamSpec *, gpointer);
static void on_handler_text_buffer_changed(GtkTextBuffer *, gpointer);
static void on_handler_check_button_toggled(GtkToggleButton *, gpointer);
static void on_handler_action_apply_clicked(GtkButton *, gpointer);
static void on_handler_action_cancel_clicked(GtkButton *, gpointer);
static void destroy_callback(GtkWidget *w, gpointer) {
    gtk_widget_destroy(w);
}
}

RouterWindow::whandler::whandler(RouterWindow *rw)
    : _rw(rw), _actions_changed(false), _updating(0)
{
    _handlerbox = GTK_BOX(lookup_widget(_rw->_window, "eview_handlerbox"));

    // config handler entry is always there, thus special
    _eview_config = lookup_widget(_rw->_window, "eview_config");
    g_object_set_data_full(G_OBJECT(_eview_config), "clicky_hname", g_strdup(""), g_free);
    g_signal_connect(_eview_config, "event", G_CALLBACK(on_handler_event), this);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_eview_config));
    g_signal_connect(buffer, "changed", G_CALLBACK(on_handler_text_buffer_changed), this);
    g_object_set_data(G_OBJECT(buffer), "clicky_view", _eview_config);

    _actions[0] = _actions[1] = 0;
    _actions_apply[0] = _actions_apply[1] = 0;
}

RouterWindow::whandler::~whandler()
{
}

void RouterWindow::whandler::clear()
{
    _ehandlers.clear();
    _hvalues.clear();
    _hinfo.clear();
    _display_ename = String();
}


/*****
 *
 * Per-hander widget settings
 *
 */

void RouterWindow::whandler::hinfo::widget_create(RouterWindow::whandler *wh, int new_flags)
{
    assert(wposition >= 0);
    
    // don't flash the expander
    if (wcontainer && (flags & hflag_collapse)
	&& (new_flags & hflag_collapse)) {
	gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(wcontainer)));
	wdata = 0;
    } else if (wcontainer) {
	gtk_widget_destroy(wcontainer);
	wcontainer = wlabel = wdata = 0;
    }

    flags = new_flags;

    // create container
    if (wcontainer)
	/* do not recreate */;
    else if (flags & hflag_collapse)
	wcontainer = gtk_expander_new(NULL);
    else if (flags & (hflag_button | hflag_checkbox))
	wcontainer = gtk_hbox_new(FALSE, 0);
    else
	wcontainer = gtk_vbox_new(FALSE, 0);

    // create label
    if ((flags & hflag_collapse) || !(flags & (hflag_button | hflag_checkbox))) {
	wlabel = gtk_label_new(name.c_str());
	gtk_label_set_attributes(GTK_LABEL(wlabel), wh->router_window()->small_attr());
	gtk_misc_set_alignment(GTK_MISC(wlabel), 0, 0.5);
	gtk_widget_show(wlabel);
	if (flags & hflag_collapse)
	    gtk_expander_set_label_widget(GTK_EXPANDER(wcontainer), wlabel);
	else
	    gtk_box_pack_start(GTK_BOX(wcontainer), wlabel, FALSE, FALSE, 0);
    }

    // create data widget
    GtkWidget *wadd;
    int padding = 0;
    gboolean expand = FALSE;
    if (flags & hflag_button) {
	wadd = wdata = gtk_button_new_with_label(name.c_str());
	if (!editable())
	    gtk_widget_set_sensitive(wdata, FALSE);
	else if (wh->active())
	    g_signal_connect(wdata, "clicked", G_CALLBACK(on_handler_action_apply_clicked), wh);
	padding = 2;
	
    } else if (flags & hflag_checkbox) {
	wadd = gtk_event_box_new();
	wdata = gtk_check_button_new_with_label(name.c_str());
	gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(wdata), TRUE);
	if (!editable())
	    gtk_widget_set_sensitive(wdata, FALSE);
	else if (wh->active()) {
	    g_signal_connect(wdata, "toggled", G_CALLBACK(on_handler_check_button_toggled), wh);
	    g_object_set_data_full(G_OBJECT(wadd), "clicky_hname", g_strdup(fullname.c_str()), g_free);
	    gtk_widget_add_events(wdata, GDK_FOCUS_CHANGE_MASK);
	    g_signal_connect(wdata, "event", G_CALLBACK(on_handler_event), wh);
	}
	expand = TRUE;
	gtk_container_add(GTK_CONTAINER(wadd), wdata);
	gtk_widget_show(wdata);
	// does nothing yet
	
    } else if (flags & hflag_multiline) {
	wadd = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wadd), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(wadd), GTK_SHADOW_IN);

	GtkTextBuffer *buffer = gtk_text_buffer_new(wh->router_window()->binary_tag_table());
	if (refreshable())
	    gtk_text_buffer_set_text(buffer, "???", 3);
	wdata = gtk_text_view_new_with_buffer(buffer);
	g_object_set_data(G_OBJECT(buffer), "clicky_view", wdata);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(wdata), GTK_WRAP_WORD);
	gtk_widget_show(wdata);
	gtk_container_add(GTK_CONTAINER(wadd), wdata);

	if (!editable()) {
	    gtk_text_view_set_editable(GTK_TEXT_VIEW(wdata), FALSE);
	    g_object_set(wdata, "can-focus", FALSE, (const char *) NULL);
	} else if (wh->active()) {
	    g_signal_connect(wdata, "event", G_CALLBACK(on_handler_event), wh);
	    g_signal_connect(buffer, "changed", G_CALLBACK(on_handler_text_buffer_changed), wh);
	}
	
    } else {
	wadd = wdata = gtk_entry_new();
	if (refreshable())
	    gtk_entry_set_text(GTK_ENTRY(wdata), "???");

	if (!editable()) {
	    gtk_entry_set_editable(GTK_ENTRY(wdata), FALSE);
	    g_object_set(wdata, "can-focus", FALSE, (const char *) NULL);
	} else if (wh->active()) {
	    g_signal_connect(wdata, "event", G_CALLBACK(on_handler_event), wh);
	    g_signal_connect(wdata, "notify::text", G_CALLBACK(on_handler_entry_changed), wh);
	}
    }

    g_object_set_data_full(G_OBJECT(wdata), "clicky_hname", g_strdup(fullname.c_str()), g_free);
    
    gtk_widget_show(wadd);
    if (flags & hflag_collapse)
	gtk_container_add(GTK_CONTAINER(wcontainer), wadd);
    else
	gtk_box_pack_start(GTK_BOX(wcontainer), wadd, expand, expand, 0);

    if (!wcontainer->parent) {
	gtk_box_pack_start(wh->handler_box(), wcontainer, FALSE, FALSE, padding);
	gtk_box_reorder_child(wh->handler_box(), wcontainer, wposition);
	gtk_widget_show(wcontainer);
    }

    assert(wcontainer && wdata);
}

bool binary_to_utf8(const String &data, StringAccum &text, Vector<int> &positions)
{
    static const char hexdigits[] = "0123456789ABCDEF";
    const char *last = data.begin();
    bool multiline = false;
    for (const char *s = data.begin(); s != data.end(); s++)
	if ((unsigned char) *s > 126
	    || ((unsigned char) *s < 32 && !isspace((unsigned char) *s))) {
	    if (last != s)
		text.append(last, s - last);
	    if (positions.size() && positions.back() == text.length())
		positions.pop_back();
	    else
		positions.push_back(text.length());
	    text.append(hexdigits[((unsigned char) *s) >> 4]);
	    text.append(hexdigits[((unsigned char) *s) & 15]);
	    positions.push_back(text.length());
	    last = s + 1;
	    multiline = true;
	} else if (*s == '\n' || *s == '\f')
	    multiline = true;
    if (text.length())
	text.append(last, data.end() - last);
    return multiline;
}

void RouterWindow::whandler::hinfo::widget_set_data(RouterWindow::whandler *wh, const String &data, bool change_form)
{
    if (flags & hflag_button)
	return;

    // Multiline data requires special handling
    StringAccum binary_data;
    Vector<int> positions;
    bool multiline = binary_to_utf8(data, binary_data, positions);

    // Valid checkbox data?
    if (flags & hflag_checkbox) {
	bool value;
	if (cp_bool(cp_uncomment(data), &value)) {
	    gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(wdata), FALSE);
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wdata), value);
	    return;
	} else if (!change_form) {
	    gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(wdata), TRUE);
	    return;
	} else
	    widget_create(wh, (flags & ~hflag_checkbox) | (multiline ? hflag_multiline : 0));
    }

    // Valid multiline data?
    if (!(flags & hflag_multiline) && (multiline || positions.size())) {
	if (!change_form) {
	    widget_set_data(wh, "???", false);
	    return;
	} else
	    widget_create(wh, flags | hflag_multiline);
    }
    
    // Set data
    if (positions.size()) {
	assert(flags & hflag_multiline);
	GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(wdata));
	gtk_text_buffer_set_text(b, binary_data.data(), binary_data.length());
	GtkTextIter i1, i2;
	for (int *i = positions.begin(); i != positions.end(); i += 2) {
	    gtk_text_buffer_get_iter_at_offset(b, &i1, i[0]);
	    gtk_text_buffer_get_iter_at_offset(b, &i2, i[1]);
	    gtk_text_buffer_apply_tag(b, wh->router_window()->binary_tag(), &i1, &i2);
	}
	gtk_text_buffer_get_end_iter(b, &i1);
	gtk_text_buffer_place_cursor(b, &i1);	
    } else if (flags & hflag_multiline) {
	GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(wdata));
	gtk_text_buffer_set_text(b, data.data(), data.length());
	GtkTextIter i1;
	gtk_text_buffer_get_end_iter(b, &i1);
	gtk_text_buffer_place_cursor(b, &i1);	
    } else {
	gtk_entry_set_text(GTK_ENTRY(wdata), data.c_str());
	gtk_entry_set_position(GTK_ENTRY(wdata), -1);
    }
}



/*****
 *
 * Life is elsewhere
 *
 */

void RouterWindow::whandler::display(const String &ename, bool incremental)
{
    if (_display_ename == ename && incremental)
	return;
    _display_ename = ename;
    gtk_container_foreach(GTK_CONTAINER(_handlerbox), destroy_callback, NULL);
    _hinfo.clear();
    hide_actions();
    
    String *hdata;
    if (ename)
	hdata = _ehandlers.findp(ename);
    else
	hdata = 0;

    if (!hdata || !*hdata) {
	GtkWidget *w = lookup_widget(_rw->_window, "eview_config");
	gtk_text_view_set_editable(GTK_TEXT_VIEW(w), TRUE);
	g_object_set(G_OBJECT(w), "can-focus", TRUE, (const char *) NULL);
    }
    if (!hdata && _ehandlers.empty())
	// special case: there are no elements
	return;
    else if (!hdata) {
	GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), _("Unknown element"));
	gtk_widget_show(l);
	gtk_box_pack_start(_handlerbox, l, FALSE, FALSE, 0);
	return;
    } else if (!*hdata) {
	assert(_rw->driver());
	GtkWidget *l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), _("<i>Loading...</i>"));
	gtk_widget_show(l);
	gtk_box_pack_start(_handlerbox, l, FALSE, FALSE, 0);
	_rw->driver()->do_read(ename + ".handlers", String(), 0);
	return;
    }
    
    // parse handler data into _hinfo
    const char *s = hdata->begin();
    bool syntax_error = false;
    while (s != hdata->end()) {
	const char *hname = s;
	while (s != hdata->end() && !isspace((unsigned char) *s))
	    ++s;
	if (s == hname || s == hdata->end() || *s != '\t') {
	    syntax_error = true;
	    break;
	}
	String n = hdata->substring(hname, s);

	while (s != hdata->end() && isspace((unsigned char) *s))
	    ++s;

	int hflags = 0;
	for (; s != hdata->end() && !isspace((unsigned char) *s); ++s)
	    switch (*s) {
	      case 'r':
		hflags |= hflag_r;
		break;
	      case 'w':
		hflags |= hflag_w;
		break;
	      case '+':
		hflags |= hflag_rparam;
		break;
	      case '%':
		hflags |= hflag_raw;
		break;
	      case '.':
		hflags |= hflag_calm;
		break;
	      case '$':
		hflags |= hflag_expensive;
		break;
	      case 'b':
		hflags |= hflag_button;
		break;
	      case 'c':
		hflags |= hflag_checkbox;
		break;
	    }
	if (!(hflags & hflag_r))
	    hflags &= ~hflag_rparam;
	if (hflags & hflag_r)
	    hflags &= ~hflag_button;
	if (hflags & hflag_rparam)
	    hflags &= ~hflag_checkbox;
	if (n == "class" || n == "name")
	    hflags |= hflag_boring;
	else if (n == "config")
	    hflags |= hflag_multiline;
	else if (n == "ports" || n == "handlers")
	    hflags |= hflag_collapse;

	_hinfo.push_front(hinfo(_display_ename, n, hflags));

	while (s != hdata->end() && *s != '\r' && *s != '\n')
	    ++s;
	if (s + 1 < hdata->end() && *s == '\r' && s[1] == '\n')
	    s += 2;
	else if (s != hdata->end())
	    ++s;
    }

    // parse _hinfo into widgets
    int pos = 0;
    for (size_t i = 0; i < _hinfo.size(); i++) {
	hinfo &hi = _hinfo[i];

	if (hi.flags & hflag_boring)
	    continue;

	if (hi.name == "config") {
	    hi.wdata = _eview_config;
	    gboolean edit = (active() && hi.editable() ? TRUE : FALSE);
	    gtk_text_view_set_editable(GTK_TEXT_VIEW(hi.wdata), edit);
	    g_object_set(hi.wdata, "can-focus", edit, (const char *) NULL);
	    g_object_set_data_full(G_OBJECT(hi.wdata), "clicky_hname", g_strdup(hi.fullname.c_str()), g_free);
	    continue;
	}

	hi.wposition = pos++;
	hi.widget_create(this, hi.flags);

	if (String *x = _hvalues.findp(hi.fullname))
	    if ((hi.flags & hflag_r) && (hi.flags & hflag_calm)) {
		_updating++;
		hi.widget_set_data(this, *x, true);
		_updating--;
	    } else
		_hvalues.remove(hi.fullname);
    }
}

void RouterWindow::whandler::notify_handlers(const String &ename, const String &data)
{
    if (_ehandlers[ename] != data) {
	_ehandlers.insert(ename, data);
	_hvalues.insert(ename + ".handlers", data);
	if (ename == _display_ename)
	    display(ename, false);
    }
}

void RouterWindow::whandler::make_actions(int which)
{
    assert(which == 0 || which == 1);
    if (_actions[which])
	return;
    // modified from GtkTreeView's search window

    // create window
    _actions[which] = gtk_window_new(GTK_WINDOW_POPUP);
    if (GTK_WINDOW(_rw->_window)->group)
	gtk_window_group_add_window(GTK_WINDOW(_rw->_window)->group, GTK_WINDOW(_actions[which]));
    gtk_window_set_transient_for(GTK_WINDOW(_actions[which]), GTK_WINDOW(_rw->_window));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(_actions[which]), TRUE);
    // gtk_window_set_modal(GTK_WINDOW(tree_view->priv->search_window), TRUE);

    // add contents
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_widget_show(frame);
    gtk_container_add(GTK_CONTAINER(_actions[which]), frame);

    GtkWidget *bbox = gtk_hbutton_box_new();
    gtk_box_set_spacing(GTK_BOX(bbox), 5);
    gtk_widget_show(bbox);
    gtk_container_set_border_width(GTK_CONTAINER(bbox), 3);
    gtk_container_add(GTK_CONTAINER(frame), bbox);
    GtkWidget *cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_widget_show(cancel);
    gtk_container_add(GTK_CONTAINER(bbox), cancel);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_handler_action_cancel_clicked), this);
    if (which == 0)
	_actions_apply[which] = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    else {
	_actions_apply[which] = gtk_button_new_with_mnemonic(_("_Query"));
	gtk_button_set_image(GTK_BUTTON(_actions_apply[which]), gtk_image_new_from_stock(GTK_STOCK_OK, GTK_ICON_SIZE_BUTTON));
    }
    gtk_widget_show(_actions_apply[which]);
    gtk_container_add(GTK_CONTAINER(bbox), _actions_apply[which]);
    g_signal_connect(_actions_apply[which], "clicked", G_CALLBACK(on_handler_action_apply_clicked), this);

    gtk_widget_realize(_actions[which]);
}

void RouterWindow::whandler::show_actions(GtkWidget *near, const String &hname, bool changed)
{
    if ((hname == _actions_hname && (!changed || _actions_changed))
	|| _updating)
	return;
    
    // find handler
    std::deque<hinfo>::iterator hi = _hinfo.begin();
    while (hi != _hinfo.end() && hi->fullname != hname)
	++hi;
    if (hi == _hinfo.end() || !hi->editable() || !active())
	return;

    // mark change
    if (changed) {
	if (hi->wlabel)
	    gtk_label_set_attributes(GTK_LABEL(hi->wlabel), router_window()->small_bold_attr());
	if (hname == _actions_hname) {
	    _actions_changed = changed;
	    return;
	}
    }

    // hide old actions, create new ones
    if (_actions_hname)
	hide_actions(_actions_hname);
    _actions_hname = hname;
    _actions_changed = changed;

    // remember checkbox state
    if (hi->flags & hflag_checkbox) {
	GtkToggleButton *b = GTK_TOGGLE_BUTTON(hi->wdata);
	if (gtk_toggle_button_get_inconsistent(b)) {
	    _hvalues.insert(hi->fullname, String());
	    gtk_toggle_button_set_active(b, FALSE);
	} else
	    _hvalues.insert(hi->fullname, cp_unparse_bool(gtk_toggle_button_get_active(b)));
	gtk_toggle_button_set_inconsistent(b, FALSE);
    }
    
    // get monitor and widget coordinates
    gtk_widget_realize(near);    
    GdkScreen *screen = gdk_drawable_get_screen(near->window);
    gint monitor_num = gdk_screen_get_monitor_at_window(screen, near->window);
    GdkRectangle monitor;
    gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor);
    
    while (GTK_WIDGET_NO_WINDOW(near))
	near = near->parent;
    gint near_x1, near_y1, near_x2, near_y2;
    gdk_window_get_origin(near->window, &near_x1, &near_y1);
    gdk_drawable_get_size(near->window, &near_x2, &near_y2);
    near_x2 += near_x1, near_y2 += near_y1;

    // get action area requisition
    int which = (hi->writable() ? 0 : 1);
    make_actions(which);
    GtkRequisition requisition;
    gtk_widget_size_request(_actions[which], &requisition);

    // adjust position based on screen
    gint x, y;
    if (near_x2 > gdk_screen_get_width(screen))
	x = gdk_screen_get_width(screen) - requisition.width;
    else if (near_x2 - requisition.width < 0)
	x = 0;
    else
	x = near_x2 - requisition.width;
    
    if (near_y2 + requisition.height > gdk_screen_get_height(screen)) {
	if (near_y1 - requisition.height < 0)
	    y = 0;
	else
	    y = near_y1 - requisition.height;
    } else
	y = near_y2;

    gtk_window_move(GTK_WINDOW(_actions[which]), x, y);
    gtk_widget_show(_actions[which]);
}

void RouterWindow::whandler::hide_actions(const String &hname, bool restore)
{
    if (!hname || hname == _actions_hname) {
	if (_actions[0])
	    gtk_widget_hide(_actions[0]);
	if (_actions[1])
	    gtk_widget_hide(_actions[1]);

	std::deque<hinfo>::iterator hi = _hinfo.begin();
	while (hi != _hinfo.end() && hi->fullname != _actions_hname)
	    ++hi;
	if (hi == _hinfo.end() || !hi->editable() || !active())
	    return;
	
	// remember checkbox state
	if ((hi->flags & hflag_checkbox) && restore) {
	    GtkToggleButton *b = GTK_TOGGLE_BUTTON(hi->wdata);
	    String v = _hvalues[hi->fullname];
	    bool value;
	    if (cp_bool(v, &value)) {
		_updating++;
		gtk_toggle_button_set_active(b, value);
		_updating--;
	    } else
		gtk_toggle_button_set_inconsistent(b, TRUE);
	}

	// unbold label on empty handlers
	if (hi->write_only() || hi->read_param()) {
	    if (GTK_IS_ENTRY(hi->wdata)
		&& strlen(gtk_entry_get_text(GTK_ENTRY(hi->wdata))) == 0)
		hi->unhighlight(_rw);
	    else if (GTK_IS_TEXT_VIEW(hi->wdata)) {
		GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hi->wdata));
		if (gtk_text_buffer_get_char_count(b) == 0)
		    hi->unhighlight(_rw);
	    }
	}
	
	_actions_hname = String();
	_actions_changed = false;
    }
}

void RouterWindow::whandler::apply_action(const String &action_for, bool activate)
{
    if (active()) {
	std::deque<hinfo>::iterator hi = _hinfo.begin();
	while (hi != _hinfo.end() && hi->fullname != action_for)
	    ++hi;
	if (hi == _hinfo.end() || !hi->editable())
	    return;
	
	int which = (hi->writable() ? 0 : 1);
	if (activate)
	    g_signal_emit_by_name(G_OBJECT(_actions_apply[which]), "activate");

	const gchar *data;
	gchar *data_free = 0;
	if (hi->flags & hflag_button)
	    data = "";
	else if (hi->flags & hflag_checkbox) {
	    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hi->wdata)))
		data = "true";
	    else
		data = "false";
	} else if (GTK_IS_TEXT_VIEW(hi->wdata)) {
	    GtkTextIter i1, i2;
	    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hi->wdata));
	    gtk_text_buffer_get_start_iter(buffer, &i1);
	    gtk_text_buffer_get_end_iter(buffer, &i2);
	    data_free = gtk_text_buffer_get_text(buffer, &i1, &i2, FALSE);
	    data = data_free;
	} else if (GTK_IS_ENTRY(hi->wdata))
	    data = gtk_entry_get_text(GTK_ENTRY(hi->wdata));
	else
	    data = "";

	assert(_rw->driver());
	if (hi->writable())
	    _rw->driver()->do_write(action_for, data, 0);
	else
	    _rw->driver()->do_read(action_for, data, 0);
	_hvalues.remove(hi->fullname);
	
	hide_actions(action_for, false);
	if (data_free)
	    g_free(data_free);
    }
}

extern "C" {
static gboolean on_handler_event(GtkWidget *w, GdkEvent *event, gpointer user_data)
{
    RouterWindow::whandler *wh = reinterpret_cast<RouterWindow::whandler *>(user_data);
    const gchar *hname = (const gchar *) g_object_get_data(G_OBJECT(w), "clicky_hname");
    
    if ((event->type == GDK_FOCUS_CHANGE && !event->focus_change.in)
	|| (event->type == GDK_KEY_PRESS && event->key.keyval == GDK_Escape))
	wh->hide_actions(hname);
    else if ((event->type == GDK_KEY_PRESS && event->key.keyval == GDK_Return)
	     || (event->type == GDK_KEY_PRESS && event->key.keyval == GDK_ISO_Enter)) {
	if (GTK_IS_ENTRY(w))
	    wh->apply_action(hname, true);
    } else if (event->type == GDK_BUTTON_PRESS
	     || event->type == GDK_2BUTTON_PRESS
	     || event->type == GDK_3BUTTON_PRESS)
	wh->show_actions(w, hname, false);

    return FALSE;
}

static void on_handler_check_button_toggled(GtkToggleButton *tb, gpointer user_data)
{
    const gchar *hname = (const gchar *) g_object_get_data(G_OBJECT(tb), "clicky_hname");
    reinterpret_cast<RouterWindow::whandler *>(user_data)->show_actions(GTK_WIDGET(tb), hname, true);
}

static void on_handler_entry_changed(GObject *obj, GParamSpec *, gpointer user_data)
{
    const gchar *hname = (const gchar *) g_object_get_data(obj, "clicky_hname");
    RouterWindow::whandler *wh = reinterpret_cast<RouterWindow::whandler *>(user_data);
    wh->show_actions(GTK_WIDGET(obj), hname, true);
}

static void on_handler_text_buffer_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    GtkWidget *view = (GtkWidget *) g_object_get_data(G_OBJECT(buffer), "clicky_view");
    const gchar *hname = (const gchar *) g_object_get_data(G_OBJECT(view), "clicky_hname");
    RouterWindow::whandler *wh = reinterpret_cast<RouterWindow::whandler *>(user_data);
    wh->show_actions(view, hname, true);
}

static void on_handler_action_cancel_clicked(GtkButton *, gpointer user_data)
{
    reinterpret_cast<RouterWindow::whandler *>(user_data)->hide_actions();
}

static void on_handler_action_apply_clicked(GtkButton *button, gpointer user_data)
{
    RouterWindow::whandler *wh = reinterpret_cast<RouterWindow::whandler *>(user_data);
    const gchar *hname = (const gchar *) g_object_get_data(G_OBJECT(button), "clicky_hname");
    String hstr = (hname ? String(hname) : wh->active_action());
    wh->apply_action(hstr, false);
}
}

/** @brief Read all read handlers and reset all write-only handlers. */
void RouterWindow::whandler::refresh_all()
{
    for (std::deque<hinfo>::iterator hi = _hinfo.begin(); hi != _hinfo.end(); ++hi)
	if (hi->refreshable()) {
	    int flags = (hi->flags & hflag_raw ? 0 : wdriver::dflag_nonraw);
	    _rw->driver()->do_read(hi->fullname, String(), flags);
	} else if (hi->write_only() && _actions_hname != hi->fullname) {
	    hi->widget_set_data(this, "", false);
	    hi->unhighlight(_rw);
	}
}

void RouterWindow::whandler::notify_read(const String &hname, const String &data, bool real)
{
    if (_display_ename.length() >= hname.length()
	|| memcmp(hname.data(), _display_ename.data(), _display_ename.length()) != 0)
	return;
    for (std::deque<hinfo>::iterator hi = _hinfo.begin(); hi != _hinfo.end(); ++hi)
	if (hi->readable() && hi->fullname == hname) {
	    _updating++;
	    hi->widget_set_data(this, data, true);
	    hi->unhighlight(_rw);
	    _updating--;

	    // set sticky value
	    if (real)
		_hvalues.insert(hi->fullname, data);
	    break;
	}
}

void RouterWindow::whandler::notify_write(const String &hname, const String &, int status)
{
    if (_display_ename.length() >= hname.length()
	|| memcmp(hname.data(), _display_ename.data(), _display_ename.length()) != 0)
	return;
    for (std::deque<hinfo>::iterator hi = _hinfo.begin(); hi != _hinfo.end(); ++hi)
	if (hi->writable() && hi->fullname == hname) {
	    _updating++;
	    if (!hi->readable() && status < 300) {
		hi->widget_set_data(this, "", false);
		hi->unhighlight(_rw);
	    }
	    _updating--;
	    break;
	}
}
