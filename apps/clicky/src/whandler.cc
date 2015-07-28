#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "whandler.hh"
#include "cdriver.hh"
#include <gdk/gdkkeysyms.h>
#include <click/confparse.hh>
extern "C" {
#include "interface.h"
#include "support.h"
}
namespace clicky {

/*****
 *
 * handler window
 *
 */

extern "C" {
static gboolean on_handler_event(GtkWidget *, GdkEvent *, gpointer);
static void on_handler_read_notify(GObject *, GParamSpec *, gpointer);
static void on_handler_entry_changed(GObject *, GParamSpec *, gpointer);
static void on_handler_text_buffer_changed(GtkTextBuffer *, gpointer);
static void on_handler_check_button_toggled(GtkToggleButton *, gpointer);
static void on_handler_action_apply_clicked(GtkButton *, gpointer);
static void on_handler_action_cancel_clicked(GtkButton *, gpointer);

static void on_hpref_visible_toggled(GtkToggleButton *, gpointer);
static void on_hpref_refreshable_toggled(GtkToggleButton *, gpointer);
static void on_hpref_autorefresh_toggled(GtkToggleButton *, gpointer);
static void on_hpref_autorefresh_value_changed(GtkSpinButton *, gpointer);
static void on_hpref_preferences_clicked(GtkButton *, gpointer);
static void on_hpref_ok_clicked(GtkButton *, gpointer);
static void on_hpref_cancel_clicked(GtkButton *, gpointer);

static void destroy_callback(GtkWidget *w, gpointer) {
    gtk_widget_destroy(w);
}
}

const char *whandler::widget_hname(GtkWidget *w)
{
    for (; w; w = w->parent)
	if (gpointer x = g_object_get_data(G_OBJECT(w), "clicky_hname"))
	    return reinterpret_cast<gchar *>(x);
    return 0;
}

whandler::whandler(wmain *rw)
    : _rw(rw), _hpref_actions(0), _actions_changed(false), _updating(0)
{
    _eviewbox = lookup_widget(_rw->_window, "eviewbox");
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

whandler::~whandler()
{
}

void whandler::clear()
{
    _hinfo.clear();
    _display_ename = String();
}


/*****
 *
 * Per-hander widget settings
 *
 */

void whandler::recalculate_positions()
{
    int pos = 0;
    for (std::deque<hinfo>::iterator iter = _hinfo.begin();
	 iter != _hinfo.end(); ++iter) {
	iter->wposition = pos;
	if (iter->hv->flags() & (hflag_visible | hflag_preferences))
	    ++pos;
    }
}

int whandler::hinfo::create_preferences(whandler *wh)
{
    int flags = _old_flags = hv->flags();
    _old_autorefresh_period = hv->autorefresh_period();
    assert((flags & hflag_preferences) && !wcontainer && !wlabel && !wdata);

    // set up the frame
    wcontainer = gtk_frame_new(NULL);
    wlabel = gtk_label_new(hv->handler_name().c_str());
    gtk_label_set_attributes(GTK_LABEL(wlabel), wh->main()->small_attr());
    gtk_misc_set_alignment(GTK_MISC(wlabel), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(wlabel), 2, 0);
    gtk_frame_set_label_widget(GTK_FRAME(wcontainer), wlabel);
    GtkWidget *aligner = gtk_alignment_new(0, 0, 1, 1);
    gtk_alignment_set_padding(GTK_ALIGNMENT(aligner), 0, 4, 2, 2);
    gtk_container_add(GTK_CONTAINER(wcontainer), aligner);
    g_object_set_data_full(G_OBJECT(aligner), "clicky_hname", g_strdup(hv->hname().c_str()), g_free);

    // fill the dialog
    GtkWidget *mainbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(aligner), mainbox);

    GtkWidget *w = gtk_check_button_new_with_label(_("Visible"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (flags & hflag_visible));
    gtk_box_pack_start(GTK_BOX(mainbox), w, FALSE, FALSE, 0);
    g_signal_connect(w, "toggled", G_CALLBACK(on_hpref_visible_toggled), wh);

    GtkWidget *visiblebox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainbox), visiblebox, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(w), "clicky_hider", visiblebox);

    GtkWidget *autorefresh_period = 0;

    if (flags & hflag_r) {
	w = gtk_check_button_new_with_label(_("Refreshable"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (flags & hflag_refresh));
	gtk_box_pack_start(GTK_BOX(visiblebox), w, FALSE, FALSE, 0);
	g_signal_connect(w, "toggled", G_CALLBACK(on_hpref_refreshable_toggled), wh);
	if (flags & hflag_rparam) {
	    // XXX add a text widget for refresh data
	}

	// autorefresh
	w = gtk_check_button_new_with_label(_("Autorefresh"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), (flags & hflag_autorefresh));
	gtk_box_pack_start(GTK_BOX(visiblebox), w, FALSE, FALSE, 0);
	g_signal_connect(w, "toggled", G_CALLBACK(on_hpref_autorefresh_toggled), wh);

	autorefresh_period = gtk_hbox_new(FALSE, 0);
	g_object_set_data(G_OBJECT(w), "clicky_hider", autorefresh_period);
	GtkAdjustment *adj = (GtkAdjustment *) gtk_adjustment_new(hv->autorefresh_period() / 1000., 0.01, 60, 0.01, 0.5, 0);
	GtkWidget *spin = gtk_spin_button_new(adj, 0.01, 3);
	gtk_box_pack_start(GTK_BOX(autorefresh_period), spin, FALSE, FALSE, 0);
	g_signal_connect(spin, "value-changed", G_CALLBACK(on_hpref_autorefresh_value_changed), wh);
	GtkWidget *label = gtk_label_new(_(" sec period"));
	gtk_box_pack_start(GTK_BOX(autorefresh_period), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(visiblebox), autorefresh_period, FALSE, FALSE, 0);
    }

    // diagram may be interested
    // wh->main()->diagram()->hpref_widgets(hv, mainbox);

    // return
    gtk_widget_show_all(wcontainer);
    if ((flags & (hflag_r | hflag_autorefresh)) != (hflag_r | hflag_autorefresh)
	&& autorefresh_period)
	gtk_widget_hide(autorefresh_period);
    if (!(flags & hflag_visible))
	gtk_widget_hide(visiblebox);
    return 0;
}

int whandler::hinfo::create_display(whandler *wh)
{
    int flags = hv->flags();
    assert(!(flags & hflag_preferences) && !wdata);

    // create container
    if (wcontainer)
	/* do not recreate */;
    else if (flags & hflag_collapse) {
	wcontainer = gtk_expander_new(NULL);
	if ((flags & hflag_r) != 0 && (flags & hflag_have_hvalue) == 0)
	    g_signal_connect(wcontainer, "notify::expanded", G_CALLBACK(on_handler_read_notify), wh);
    } else if (flags & (hflag_button | hflag_checkbox))
	wcontainer = gtk_hbox_new(FALSE, 0);
    else
	wcontainer = gtk_vbox_new(FALSE, 0);

    // create label
    if ((flags & hflag_collapse) || !(flags & (hflag_button | hflag_checkbox))) {
	wlabel = gtk_label_new(hv->handler_name().c_str());
	gtk_label_set_attributes(GTK_LABEL(wlabel), wh->main()->small_attr());
	if (!(flags & hflag_collapse))
	    gtk_label_set_ellipsize(GTK_LABEL(wlabel), PANGO_ELLIPSIZE_END);
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
	wadd = wdata = gtk_button_new_with_label(hv->handler_name().c_str());
	if (!hv->editable())
	    gtk_widget_set_sensitive(wdata, FALSE);
	else if (wh->active())
	    g_signal_connect(wdata, "clicked", G_CALLBACK(on_handler_action_apply_clicked), wh);
	padding = 2;

    } else if (flags & hflag_checkbox) {
	wadd = gtk_event_box_new();
	wdata = gtk_check_button_new_with_label(hv->handler_name().c_str());
	if (!hv->editable())
	    gtk_widget_set_sensitive(wdata, FALSE);
	else if (wh->active()) {
	    g_signal_connect(wdata, "toggled", G_CALLBACK(on_handler_check_button_toggled), wh);
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
	gtk_widget_set_size_request(GTK_WIDGET(wadd), -1, 60);

	GtkTextBuffer *buffer = gtk_text_buffer_new(wh->main()->binary_tag_table());
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
	if (!hv->editable()) {
	    gtk_editable_set_editable(GTK_EDITABLE(wdata), FALSE);
	    g_object_set(wdata, "can-focus", FALSE, (const char *) NULL);
	} else if (wh->active()) {
	    g_signal_connect(wdata, "event", G_CALLBACK(on_handler_event), wh);
	    g_signal_connect(wdata, "notify::text", G_CALLBACK(on_handler_entry_changed), wh);
	}
    }

    g_object_set_data_full(G_OBJECT(wcontainer), "clicky_hname", g_strdup(hv->hname().c_str()), g_free);

    gtk_widget_show(wadd);
    if (flags & hflag_collapse)
	gtk_container_add(GTK_CONTAINER(wcontainer), wadd);
    else
	gtk_box_pack_start(GTK_BOX(wcontainer), wadd, expand, expand, 0);

    return padding;
}

void whandler::hinfo::create(whandler *wh, int new_flags, bool always_position)
{
    if (hv->flags() & hflag_special)
	return;

    // don't flash the expander
    if (wcontainer
	&& (((hv->flags() & new_flags) & (hflag_collapse | hflag_visible))
	    == (hflag_collapse | hflag_visible))
	&& (hv->flags() & hflag_preferences) == 0
	&& (new_flags & hflag_preferences) == 0) {
	gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(wcontainer)));
	wdata = 0;
    } else if (wcontainer) {
	gtk_widget_destroy(wcontainer);
	wcontainer = wlabel = wdata = 0;
    } else
	assert(wcontainer == 0 && wlabel == 0 && wdata == 0);

    // set flags, potentially recalculate positions
    bool recalc = ((hv->flags() ^ new_flags) & (hflag_visible | hflag_preferences));
    hv->set_flags(wh->main(), new_flags);
    if (recalc || always_position)
	wh->recalculate_positions();

    // create the body
    int padding;
    if (hv->flags() & hflag_preferences)
	padding = create_preferences(wh);
    else if (hv->flags() & hflag_visible)
	padding = create_display(wh);
    else
	return;

    // add to the container
    if (!wcontainer->parent) {
	gtk_box_pack_start(wh->handler_box(), wcontainer, FALSE, FALSE, padding);
	gtk_box_reorder_child(wh->handler_box(), wcontainer, wposition);
	gtk_widget_show(wcontainer);
    }

    // display contents if visible
    if ((hv->flags() & (hflag_preferences | hflag_visible)) == hflag_visible) {
	wh->_updating++;
	display(wh, true);
	wh->_updating--;
    }
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

void whandler::hinfo::display(whandler *wh, bool change_form)
{
    if (!wdata || (hv->flags() & hflag_button))
	return;

    // Multiline data requires special handling
    StringAccum binary_data;
    Vector<int> positions;
    bool multiline = binary_to_utf8(hv->hvalue(), binary_data, positions);

    // Valid checkbox data?
    if (hv->flags() & hflag_checkbox) {
	bool value;
	if (hv->have_hvalue()
	    && cp_bool(cp_uncomment(hv->hvalue()), &value)) {
	    gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(wdata), FALSE);
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wdata), value);
	    return;
	} else if (!hv->have_hvalue() || !change_form) {
	    gtk_toggle_button_set_inconsistent(GTK_TOGGLE_BUTTON(wdata), TRUE);
	    return;
	} else
	    create(wh, (hv->flags() & ~hflag_checkbox) | (multiline ? hflag_multiline : 0), false);
    }

    // Valid multiline data?
    if (!(hv->flags() & hflag_multiline) && (multiline || positions.size())) {
	if (!change_form) {
	    gtk_entry_set_text(GTK_ENTRY(wdata), "???");
	    gtk_editable_set_position(GTK_EDITABLE(wdata), -1);
	    return;
	} else
	    create(wh, hv->flags() | hflag_multiline, false);
    }

    // Set data
    if (positions.size()) {
	assert(hv->flags() & hflag_multiline);
	GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(wdata));
	gtk_text_buffer_set_text(b, binary_data.data(), binary_data.length());
	GtkTextIter i1, i2;
	for (int *i = positions.begin(); i != positions.end(); i += 2) {
	    gtk_text_buffer_get_iter_at_offset(b, &i1, i[0]);
	    gtk_text_buffer_get_iter_at_offset(b, &i2, i[1]);
	    gtk_text_buffer_apply_tag(b, wh->main()->binary_tag(), &i1, &i2);
	}
	gtk_text_buffer_get_end_iter(b, &i1);
	gtk_text_buffer_place_cursor(b, &i1);
    } else if (hv->flags() & hflag_multiline) {
	GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(wdata));
	gtk_text_buffer_set_text(b, hv->hvalue().data(), hv->hvalue().length());
	GtkTextIter i1;
	gtk_text_buffer_get_end_iter(b, &i1);
	gtk_text_buffer_place_cursor(b, &i1);
    } else {
	gtk_entry_set_text(GTK_ENTRY(wdata), hv->hvalue().c_str());
	gtk_editable_set_position(GTK_EDITABLE(wdata), -1);
    }

    // Display parameters
    if (wlabel && hv->read_param() && (hv->hparam() || hv->have_hvalue())) {
	if (!hv->hparam())
	    gtk_label_set_text(GTK_LABEL(wlabel), hv->handler_name().c_str());
	else {
	    StringAccum sa;
	    sa << hv->handler_name() << ' '
	       << hv->hparam().substring(0, MIN(hv->hparam().length(), 100));
	    gtk_label_set_text(GTK_LABEL(wlabel), sa.c_str());
	}
    }

    // Unhighlight
    set_edit_active(wh->main(), false);
}



/*****
 *
 * Life is elsewhere
 *
 */

void whandler::display(const String &ename, bool incremental)
{
    if ((_display_ename == ename && incremental)
	|| !gtk_widget_get_window(_eviewbox))
	return;
    _display_ename = ename;
    gtk_container_foreach(GTK_CONTAINER(_handlerbox), destroy_callback, NULL);

    // no longer interested in old handler values
    for (std::deque<hinfo>::iterator hiter = _hinfo.begin();
	 hiter != _hinfo.end(); ++hiter)
	hiter->hv->set_flags(main(), hiter->hv->flags() & ~(hflag_preferences | hflag_notify_whandlers));
    _hinfo.clear();

    hide_actions();
    _hpref_actions = 0;

    handler_value *hv = main()->hvalues().find(ename + ".handlers");

    // no information about this element's handlers yet
    if (!hv || !hv->have_hvalue()) {
	gtk_text_view_set_editable(GTK_TEXT_VIEW(_eview_config), TRUE);
	g_object_set(G_OBJECT(_eview_config), "can-focus", TRUE, (const char *) NULL);

	if (!ename || _rw->empty())
	    /* there are no elements; do nothing */;
	else if (!_rw->element_exists(ename, true)) {
	    // either not an element, or a compound (compounds have no
	    // handlers).  Report an error if not an element at all.
	    if (!_rw->element_exists(ename)) {
		GtkWidget *l = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(l), _("Unknown element"));
		gtk_widget_show(l);
		gtk_box_pack_start(_handlerbox, l, FALSE, FALSE, 0);
	    }
	} else if (_rw->driver()) {
	    GtkWidget *l = gtk_label_new(NULL);
	    gtk_label_set_markup(GTK_LABEL(l), _("<i>Loading...</i>"));
	    gtk_widget_show(l);
	    gtk_box_pack_start(_handlerbox, l, FALSE, FALSE, 0);
	    hv = _rw->hvalues().find_force(ename + ".handlers");
	    hv->set_flags(main(), hv->flags() | hflag_notify_whandlers);
	    hv->refresh(main());
	}
	return;
    }

    // parse handlers into _hinfo
    handler_values::iterator hiter = main()->hvalues().begin(ename);
    for (; hiter != main()->hvalues().end(); ++hiter)
	_hinfo.push_back(hiter.operator->());

    // parse _hinfo into widgets
    for (std::deque<hinfo>::iterator hi = _hinfo.begin();
	 hi != _hinfo.end(); ++hi)
	if (hi->hv->flags() & hflag_special) {
	    if (hi->hv->handler_name() == "config") {
		hi->wdata = _eview_config;
		gboolean edit = (active() && hi->editable() ? TRUE : FALSE);
		gtk_text_view_set_editable(GTK_TEXT_VIEW(hi->wdata), edit);
		g_object_set(hi->wdata, "can-focus", edit, (const char *) NULL);
		g_object_set_data_full(G_OBJECT(hi->wdata), "clicky_hname", g_strdup(hi->hv->hname().c_str()), g_free);
		hi->hv->set_flags(_rw, hi->hv->flags() | hflag_notify_whandlers);
		hi->hv->refresh(_rw);
	    }
	} else {
	    hi->create(this, hi->hv->flags() | hflag_notify_whandlers, true);
	    if (hi->hv->refreshable() && !hi->hv->have_hvalue()
		&& hi->hv->have_required_hparam()
		&& (hi->hv->flags() & hflag_collapse) == 0)
		hi->hv->refresh(_rw);
	}

    // the final button box
    GtkWidget *bbox = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
    _hpref_actions = GTK_BUTTON_BOX(bbox);
    on_preferences(onpref_initial);
    gtk_box_pack_end(_handlerbox, bbox, FALSE, FALSE, 0);

    GtkWidget *w = gtk_hseparator_new();
    gtk_box_pack_end(_handlerbox, w, FALSE, FALSE, 4);
    gtk_widget_show(w);
}

void whandler::redisplay()
{
    if (_display_ename)
	display(_display_ename, false);
}

void whandler::on_preferences(int action)
{
    gtk_container_foreach(GTK_CONTAINER(_hpref_actions), destroy_callback, NULL);
    if (action == onpref_initial || action == onpref_prefok
	|| action == onpref_prefcancel) {
	GtkWidget *w = gtk_button_new_from_stock(GTK_STOCK_PROPERTIES);
	gtk_button_set_relief(GTK_BUTTON(w), GTK_RELIEF_NONE);
	gtk_container_add(GTK_CONTAINER(_hpref_actions), w);
	g_signal_connect(w, "clicked", G_CALLBACK(on_hpref_preferences_clicked), this);
    } else {
	GtkWidget *w = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_container_add(GTK_CONTAINER(_hpref_actions), w);
	g_signal_connect(w, "clicked", G_CALLBACK(on_hpref_cancel_clicked), this);
	w = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_container_add(GTK_CONTAINER(_hpref_actions), w);
	g_signal_connect(w, "clicked", G_CALLBACK(on_hpref_ok_clicked), this);
    }
    gtk_widget_show_all(GTK_WIDGET(_hpref_actions));

    if (action == onpref_prefcancel)
	for (std::deque<hinfo>::iterator iter = _hinfo.begin();
	     iter != _hinfo.end(); ++iter)
	    if (!iter->hv->special()) {
		iter->hv->set_autorefresh_period(iter->_old_autorefresh_period);
		iter->hv->set_flags(main(), iter->_old_flags);
	    }

    int clear = 0, set = 0;
    if (action == onpref_showpref)
	set = hflag_preferences;
    if (action == onpref_prefok || action == onpref_prefcancel)
	clear = hflag_preferences;
    if (clear || set)
	for (std::deque<hinfo>::iterator iter = _hinfo.begin();
	     iter != _hinfo.end(); ++iter)
	    if (!iter->hv->special())
		iter->create(this, (iter->hv->flags() & ~clear) | set, false);
}

void whandler::make_actions(int which)
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

void whandler::show_actions(GtkWidget *near, const String &hname, bool changed)
{
    if ((hname == _actions_hname && (!changed || _actions_changed))
	|| _updating)
	return;

    // find handler
    hinfo *hi = find_hinfo(hname);
    if (!hi || !hi->editable() || !active())
	return;

    // mark change
    if (changed) {
	hi->set_edit_active(main(), true);
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

    // erase inconsistent states from "no value yet available"
    if (hi->hv->flags() & hflag_checkbox) {
	GtkToggleButton *b = GTK_TOGGLE_BUTTON(hi->wdata);
	if (gtk_toggle_button_get_inconsistent(b)) {
	    gtk_toggle_button_set_active(b, FALSE);
	    gtk_toggle_button_set_inconsistent(b, FALSE);
	}
    } else if ((hi->hv->flags() & hflag_have_hvalue) == 0) {
	if (hi->hv->flags() & hflag_multiline) {
	    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hi->wdata));
	    if (gtk_text_buffer_get_char_count(b) == 3) {
		GtkTextIter i1, i2;
		gtk_text_buffer_get_start_iter(b, &i1);
		gtk_text_buffer_get_end_iter(b, &i2);
		gchar *data = gtk_text_buffer_get_text(b, &i1, &i2, FALSE);
		if (strcmp(data, "???") == 0)
		    gtk_text_buffer_set_text(b, "", 0);
		g_free(data);
	    }
	} else {
	    if (strcmp(gtk_entry_get_text(GTK_ENTRY(hi->wdata)), "???") == 0)
		gtk_entry_set_text(GTK_ENTRY(hi->wdata), "");
	}
    }

    // get monitor and widget coordinates
    gtk_widget_realize(near);
    GdkScreen *screen = gdk_window_get_screen(near->window);
    gint monitor_num = gdk_screen_get_monitor_at_window(screen, near->window);
    GdkRectangle monitor;
    gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor);

    while (!gtk_widget_get_has_window(near))
	near = near->parent;
    gint near_x1, near_y1, near_x2, near_y2;
    gdk_window_get_origin(near->window, &near_x1, &near_y1);
    near_x2 = near_x1 + gdk_window_get_width(near->window);
    near_y2 = near_y1 + gdk_window_get_height(near->window);

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

void whandler::hide_actions(const String &hname, bool restore)
{
    if (!hname || hname == _actions_hname) {
	if (_actions[0])
	    gtk_widget_hide(_actions[0]);
	if (_actions[1])
	    gtk_widget_hide(_actions[1]);

	hinfo *hi = find_hinfo(_actions_hname);
	if (!hi || !hi->editable() || !active())
	    return;

	// remember checkbox state
	handler_value *hv = hi->hv;
	if ((hv->flags() & hflag_checkbox) && restore) {
	    GtkToggleButton *b = GTK_TOGGLE_BUTTON(hi->wdata);
	    bool value;
	    if (cp_bool(hv->hvalue(), &value)) {
		_updating++;
		gtk_toggle_button_set_active(b, value);
		_updating--;
	    } else
		gtk_toggle_button_set_inconsistent(b, TRUE);
	}

	// unbold label on empty handlers
	if (hv->write_only() || hv->read_param()) {
	    bool empty = false;
	    if (GTK_IS_ENTRY(hi->wdata))
		empty = (strlen(gtk_entry_get_text(GTK_ENTRY(hi->wdata))) == 0);
	    else if (GTK_IS_TEXT_VIEW(hi->wdata)) {
		GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(hi->wdata));
		empty = (gtk_text_buffer_get_char_count(b) == 0);
	    }
	    if (empty) {
		hi->set_edit_active(_rw, false);
		hi->hv->clear_hvalue();
	    }
	}

	_actions_hname = String();
	_actions_changed = false;
    }
}

void whandler::apply_action(const String &action_for, bool activate)
{
    if (active()) {
	hinfo *hi = find_hinfo(action_for);
	if (!hi || !hi->editable())
	    return;

	int which = (hi->writable() ? 0 : 1);
	if (activate)
	    g_signal_emit_by_name(G_OBJECT(_actions_apply[which]), "activate");

	const gchar *data;
	gchar *data_free = 0;
	if (hi->hv->flags() & hflag_button)
	    data = "";
	else if (hi->hv->flags() & hflag_checkbox) {
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
	if (hi->writable()) {
	    _rw->driver()->do_write(action_for, data, 0);
	    hi->hv->clear_hvalue();
	    if (hi->hv->refreshable())
		hi->hv->refresh(main());
	} else
	    _rw->driver()->do_read(action_for, data, 0);

	hide_actions(action_for, false);
	if (data_free)
	    g_free(data_free);
    }
}

extern "C" {
static void on_handler_read_notify(GObject *obj, GParamSpec *, gpointer user_data)
{
    GtkExpander *expander = GTK_EXPANDER(obj);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(expander));
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    if (gtk_expander_get_expanded(expander))
	wh->refresh(hname, false);
}

static gboolean on_handler_event(GtkWidget *w, GdkEvent *event, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(w);

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
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(tb));
    reinterpret_cast<whandler *>(user_data)->show_actions(GTK_WIDGET(tb), hname, true);
}

static void on_handler_entry_changed(GObject *obj, GParamSpec *, gpointer user_data)
{
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(obj));
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    wh->show_actions(GTK_WIDGET(obj), hname, true);
}

static void on_handler_text_buffer_changed(GtkTextBuffer *buffer, gpointer user_data)
{
    GtkWidget *view = (GtkWidget *) g_object_get_data(G_OBJECT(buffer), "clicky_view");
    const gchar *hname = whandler::widget_hname(view);
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    wh->show_actions(view, hname, true);
}

static void on_handler_action_cancel_clicked(GtkButton *, gpointer user_data)
{
    reinterpret_cast<whandler *>(user_data)->hide_actions();
}

static void on_handler_action_apply_clicked(GtkButton *button, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(button));
    String hstr = (hname ? String(hname) : wh->active_action());
    wh->apply_action(hstr, false);
}
}

void whandler::refresh(const String &hname, bool always)
{
    hinfo *hi = find_hinfo(hname);
    if (hi && (always || !hi->hv->have_hvalue()))
	hi->hv->refresh(_rw);
}

/** @brief Read all read handlers and reset all write-only handlers. */
void whandler::refresh_all(bool always)
{
    for (std::deque<hinfo>::iterator hi = _hinfo.begin();
	 hi != _hinfo.end(); ++hi) {
	handler_value *hv = hi->hv;
	if (hv->refreshable() && hv->have_required_hparam()
	    && (always || !hv->have_hvalue()))
	    hi->hv->refresh(_rw);
	else if (hv->write_only() && _actions_hname != hv->hname())
	    hi->display(this, false);
    }
}

void whandler::notify_read(handler_value *hv)
{
    hinfo *hi = find_hinfo(hv);
    if (hi && hv->readable() && hv->visible()) {
	_updating++;
	hi->display(this, true);
	_updating--;
    } else if (hv->hname().length() == _display_ename.length() + 9
	       && memcmp(hv->hname().begin(), _display_ename.begin(), _display_ename.length()) == 0
	       && memcmp(hv->hname().end() - 9, ".handlers", 9) == 0)
	display(_display_ename, false);
}

void whandler::notify_write(const String &hname, const String &, int status)
{
    hinfo *hi = find_hinfo(hname);
    if (hi && hi->writable()) {
	_updating++;
	if (!hi->readable() && status < 300)
	    hi->display(this, false);
	_updating--;
    }
}


/*****
 *
 * Handler preferences
 *
 */

void whandler::set_hinfo_flags(const String &hname, int flags, int flag_values)
{
    if (hinfo *hi = find_hinfo(hname)) {
	hi->hv->set_flags(main(), (hi->hv->flags() & ~flags) | flag_values);
	//if (flags & hflag_notify_delt)
	//   main()->diagram()->hpref_apply(hi->hv);
    }
}

void whandler::set_hinfo_autorefresh_period(const String &hname, int period)
{
    if (hinfo *hi = find_hinfo(hname))
	hi->hv->set_autorefresh_period(period > 0 ? period : 1);
}

extern "C" {
static void on_hpref_visible_toggled(GtkToggleButton *button, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(button));
    gboolean on = gtk_toggle_button_get_active(button);
    wh->set_hinfo_flags(hname, hflag_visible, on ? hflag_visible : 0);

    GtkWidget *widget = reinterpret_cast<GtkWidget *>(g_object_get_data(G_OBJECT(button), "clicky_hider"));
    if (on)
	gtk_widget_show(widget);
    else
	gtk_widget_hide(widget);
}

static void on_hpref_refreshable_toggled(GtkToggleButton *button, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(button));
    gboolean on = gtk_toggle_button_get_active(button);
    wh->set_hinfo_flags(hname, hflag_refresh, on ? hflag_refresh : 0);
}

static void on_hpref_autorefresh_toggled(GtkToggleButton *button, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(button));
    gboolean on = gtk_toggle_button_get_active(button);
    wh->set_hinfo_flags(hname, hflag_autorefresh, on ? hflag_autorefresh : 0);

    GtkWidget *widget = reinterpret_cast<GtkWidget *>(g_object_get_data(G_OBJECT(button), "clicky_hider"));
    if (on)
	gtk_widget_show(widget);
    else
	gtk_widget_hide(widget);
}

static void on_hpref_autorefresh_value_changed(GtkSpinButton *button, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    const gchar *hname = whandler::widget_hname(GTK_WIDGET(button));
    wh->set_hinfo_autorefresh_period(hname, (guint) (gtk_spin_button_get_value(button) * 1000));
}

static void on_hpref_preferences_clicked(GtkButton *, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    wh->on_preferences(whandler::onpref_showpref);
}

static void on_hpref_ok_clicked(GtkButton *, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    wh->on_preferences(whandler::onpref_prefok);
}

static void on_hpref_cancel_clicked(GtkButton *, gpointer user_data)
{
    whandler *wh = reinterpret_cast<whandler *>(user_data);
    wh->on_preferences(whandler::onpref_prefcancel);
}

}
}
