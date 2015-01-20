#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include "wmain.hh"
#include "wdiagram.hh"
#include "whandler.hh"
#include "cdriver.hh"
#include "dstyle.hh"
#include "scopechain.hh"
#include <clicktool/routert.hh>
#include <clicktool/lexert.hh>
#include <clicktool/lexertinfo.hh>
#include <clicktool/toolutils.hh>
#include <clicktool/elementmap.hh>
#include <clicktool/processingt.hh>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/pathvars.h>
#include <math.h>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
extern "C" {
#include "interface.h"
#include "support.h"
}
namespace clicky {

extern "C" {
static void error_expanders_callback(GObject *, GParamSpec *, gpointer);
static void on_eview_classexpander_expanded(GObject *, GParamSpec *, gpointer);
static void elementtreesort_callback(GtkButton *, gpointer);
static gboolean on_error_view_event(GtkWidget *, GdkEvent *, gpointer);
}

String wmain::last_savefile;
Vector<wmain*> wmain::all_wmains;

extern "C" {
static void destroy(gpointer data) {
    delete reinterpret_cast<wmain *>(data);
}
}

wmain::wmain(bool show_toolbar, bool show_list, dcss_set *ccss,
	     gint width, gint height)
    : crouter(ccss),
      _window(create_mainw()),
      _config_clean_errors(true), _config_clean_elements(true),
      _error_hover_tag(0), _error_highlight_tag(0),
      _error_endpos(0), _error_hover_index(-1),
      _config_error_highlight_tag(0), _error_highlight_index(-1),
      _error_highlight_x(-1), _error_highlight_y(-1), _error_scroller(0),
      _elist_view(0), _elist_store(0), _elist_sort(elist_sort_none),
      _config_element_highlight_tag(0), _element_highlight(0),
      _config_changed_signal(0), _binary_tag(0)
{
    g_object_set_data_full(G_OBJECT(_window), "wmain", this, destroy);
    if (width != -1 || height != -1)
	gtk_window_set_default_size(GTK_WINDOW(_window), width, height);

    // settings
    set_show_toolbar(show_toolbar);
    set_show_list(show_list);

    // look up widgets
    _config_view = lookup_widget(_window, "configview");
    _config_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_config_view));
    _error_view = lookup_widget(_window, "errorview");
    _error_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_error_view));

    // cursors
    _normal_cursor = gdk_cursor_new(GDK_LEFT_PTR);
    gdk_cursor_ref(_normal_cursor);
    _link_cursor = gdk_cursor_new(GDK_HAND2);
    gdk_cursor_ref(_link_cursor);
    gtk_widget_realize(_error_view);
    gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(_error_view), GTK_TEXT_WINDOW_TEXT), _normal_cursor);

    // signals
    g_signal_connect(lookup_widget(_window, "elementtreeexpander"),
		     "notify::expanded",
		     G_CALLBACK(error_expanders_callback), this);
    g_signal_connect(lookup_widget(_window, "errorviewexpander"),
		     "notify::expanded",
		     G_CALLBACK(error_expanders_callback), this);
    g_signal_connect(lookup_widget(_window, "eview_classexpander"),
		     "notify::expanded",
		     G_CALLBACK(on_eview_classexpander_expanded), this);
    gtk_widget_set_name(lookup_widget(_window, "eviewbox"), "eviewbox");
    gtk_widget_set_name(lookup_widget(_window, "eview_titlebox"), "eview_titlebox");
    g_signal_connect(lookup_widget(_window, "elementtreesort"),
		     "clicked",
		     G_CALLBACK(elementtreesort_callback), this);
    g_signal_connect(_error_view, "event", G_CALLBACK(on_error_view_event), this);
    gtk_widget_add_events(_error_view, GDK_LEAVE_NOTIFY_MASK);

    // label precision
    _bold_attr = pango_attr_list_new();
    PangoAttribute *a = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    a->start_index = 0;
    a->end_index = G_MAXUINT;
    pango_attr_list_insert(_bold_attr, a);

    _small_attr = pango_attr_list_new();
    a = pango_attr_scale_new(PANGO_SCALE_SMALL);
    a->start_index = 0;
    a->end_index = G_MAXUINT;
    pango_attr_list_insert(_small_attr, a);

    _small_bold_attr = pango_attr_list_new();
    a = pango_attr_scale_new(PANGO_SCALE_SMALL);
    a->start_index = 0;
    a->end_index = G_MAXUINT;
    pango_attr_list_insert(_small_bold_attr, a);
    a = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    a->start_index = 0;
    a->end_index = G_MAXUINT;
    pango_attr_list_insert(_small_bold_attr, a);

    gtk_label_set_attributes(GTK_LABEL(lookup_widget(_window, "eview_label")), _bold_attr);
    gtk_label_set_attributes(GTK_LABEL(lookup_widget(_window, "eview_classinfo_ports")), _small_attr);
    gtk_label_set_attributes(GTK_LABEL(lookup_widget(_window, "eview_classinfo_processing")), _small_attr);
    gtk_label_set_attributes(GTK_LABEL(lookup_widget(_window, "eview_classinfo_flow")), _small_attr);

    // text tags for configuration (order is important for overriding)
    (void) gtk_text_buffer_create_tag(_config_buffer, "comment", "foreground", "grey50", NULL);
    (void) gtk_text_buffer_create_tag(_config_buffer, "keyword", "foreground", "blue", NULL);
    (void) gtk_text_buffer_create_tag(_config_buffer, "error", "foreground", "red", NULL);
    _config_element_highlight_tag = gtk_text_buffer_create_tag(_config_buffer, "element_current", "background", "dark blue", "foreground", "white", NULL);
    _config_error_highlight_tag = gtk_text_buffer_create_tag(_config_buffer, "error_current", "background", "red", "foreground", "white", "weight", PANGO_WEIGHT_BOLD, NULL);
    _error_hover_tag = gtk_text_buffer_create_tag(_error_buffer, "error_current", "underline", PANGO_UNDERLINE_SINGLE, "foreground", "red", NULL);
    _error_highlight_tag = gtk_text_buffer_create_tag(_error_buffer, "error_highlight", "foreground", "red", NULL);

    _binary_tag_table = gtk_text_tag_table_new();
    _binary_tag = gtk_text_tag_new("binary");
    g_object_set(G_OBJECT(_binary_tag), "foreground", "white", "background", "black", (const char *) NULL);
    gtk_text_tag_table_add(_binary_tag_table, _binary_tag);

    // subsystems
    _handlers = new whandler(this);
    _diagram = new wdiagram(this);
    dialogs_connect();
    config_changed_initialize(true, false);
    set_diagram_mode(false, true);
    on_driver_changed();

    all_wmains.push_back(this);
}

wmain::~wmain() {
    // only call from GtkWidget destruction
    clear(false);

    gdk_cursor_unref(_normal_cursor);
    gdk_cursor_unref(_link_cursor);
    pango_attr_list_unref(_small_attr);
    pango_attr_list_unref(_bold_attr);
    pango_attr_list_unref(_small_bold_attr);
    delete _diagram;
    delete _handlers;

    for (int i = 0; i != all_wmains.size(); ++i)
        if (all_wmains[i] == this) {
            all_wmains.erase(all_wmains.begin() + i);
            break;
        }
    if (!all_wmains.size())
	gtk_main_quit();
}

// xxx
class ClickyLexerTInfo : public LexerTInfo { public:

    ClickyLexerTInfo(GtkTextBuffer *buffer, const String &config, GatherErrorHandler *gerrh)
	: _buffer(buffer), _tt(gtk_text_buffer_get_tag_table(buffer)),
	  _comment_tag(0), _keyword_tag(0), _error_tag(0),
	  _config(config), _gerrh(gerrh) {
    }

    void apply_tag(const char *pos1, const char *pos2, GtkTextTag *tag) {
	GtkTextIter i1, i2;
	gtk_text_buffer_get_iter_at_offset(_buffer, &i1, pos1 - _config.begin());
	gtk_text_buffer_get_iter_at_offset(_buffer, &i2, pos2 - _config.begin());
	gtk_text_buffer_apply_tag(_buffer, tag, &i1, &i2);
    }

    void notify_comment(const char *pos1, const char *pos2) {
	if (!_comment_tag)
	    _comment_tag = gtk_text_tag_table_lookup(_tt, "comment");
	apply_tag(pos1, pos2, _comment_tag);
    }

    void notify_keyword(const String &, const char *pos1, const char *pos2) {
	if (!_keyword_tag)
	    _keyword_tag = gtk_text_tag_table_lookup(_tt, "keyword");
	apply_tag(pos1, pos2, _keyword_tag);
    }

    void notify_error(const String &, const char *pos1, const char *pos2) {
	if (_gerrh)
	    _gerrh->set_next_errpos(pos1 - _config.begin(), pos2 - _config.begin());
	else {
	    if (!_error_tag)
		_error_tag = gtk_text_tag_table_lookup(_tt, "error");
	    apply_tag(pos1, pos2, _error_tag);
	}
    }

#if 0
    void add_item(const char *pos1, const String &s1, const char *pos2, const String &s2) {
	::add_item(pos1 - _config.begin(), s1, pos2 - _config.begin(), s2);
    }
    void add_item(const char *pos1, ElementT *e1, const char *pos2, const String &s2) {
	::add_item(pos1 - _config.begin(), e1, pos2 - _config.begin(), s2);
    }
    void add_item(const char *pos1, ElementClassT *e1, const char *pos2, const String &s2) {
	::add_item(pos1 - _config.begin(), e1, pos2 - _config.begin(), s2);
    }
    void notify_comment(const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-cmt'>", pos2, "</span>");
    }
    void notify_error(const String &what, const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-err' title='" + html_quote_attr(what) + "'>", pos2, "</span>");
    }
    void notify_keyword(const String &, const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-kw'>", pos2, "</span>");
    }
    void notify_config_string(const char *pos1, const char *pos2) {
	add_item(pos1, "<span class='c-cfg'>", pos2, "</span>");
    }
    void notify_class_declaration(ElementClassT *ec, bool anonymous, const char *decl_pos1, const char *name_pos1, const char *) {
	if (!anonymous)
	    add_item(name_pos1, "<a name='" + link_class_decl(ec) + "'><span class='c-cd'>", name_pos1 + ec->name().length(), "</span></a>");
	else
	    add_item(decl_pos1, "<a name='" + link_class_decl(ec) + "'>", decl_pos1 + 1, "</a>");
	add_class_href(ec, "#" + link_class_decl(ec));
    }
    void notify_class_extension(ElementClassT *ec, const char *pos1, const char *pos2) {
	add_item(pos1, ec, pos2, "");
    }
    void notify_class_reference(ElementClassT *ec, const char *pos1, const char *pos2) {
	add_item(pos1, ec, pos2, "");
    }
    void notify_element_declaration(ElementT *e, const char *pos1, const char *pos2, const char *decl_pos2) {
	add_item(pos1, "<a name='" + link_element_decl(e) + "'>", pos2, "</a>");
	add_item(pos1, "<span class='c-ed'>", decl_pos2, "</span>");
	notify_element_reference(e, pos1, decl_pos2);
    }
    void notify_element_reference(ElementT *e, const char *pos1, const char *pos2) {
	add_item(pos1, e, pos2, "</span>");
    }
#endif

    GtkTextBuffer *_buffer;
    GtkTextTagTable *_tt;
    GtkTextTag *_comment_tag;
    GtkTextTag *_keyword_tag;
    GtkTextTag *_error_tag;
    String _config;
    GatherErrorHandler *_gerrh;

};

void wmain::clear(bool alive)
{
    crouter::clear(alive);

    _error_endpos = 0;
    _savefile = String();
    _config_clean_errors = _config_clean_elements = true;

    _error_hover_index = -1;
    _error_highlight_index = -1;
    _error_highlight_x = _error_highlight_y = -1;

    _element_highlight = 0;
    _eview_name = String();

    if (_config_changed_signal && alive)
	g_signal_handler_disconnect(_config_buffer, _config_changed_signal);
    _config_changed_signal = 0;
    if (_error_scroller)
	g_source_remove(_error_scroller);
    _error_scroller = 0;

    // XXX _hvalues.clear();
    _handlers->clear();
    _diagram->router_create(false, false);

    // Initialize window state
    if (alive) {
	gtk_text_buffer_set_text(_config_buffer, "", 0);
	element_show(String(), 0, false);
	etree_fill();
	on_error(true, String());
    }
}


void wmain::on_landmark_changed()
{
    if (landmark()) {
	String title = "Clicky: " + landmark();
	gtk_window_set_title(GTK_WINDOW(_window), title.c_str());
    } else
	gtk_window_set_title(GTK_WINDOW(_window), "Clicky");
}

void wmain::on_ccss_changed()
{
    _diagram->on_ccss_changed();
}

LexerTInfo *wmain::on_config_changed_prepare()
{
    _error_endpos = 0;
    gtk_text_buffer_set_text(_error_buffer, "", 0);

    error_unhighlight();
    element_unhighlight();
    String conf = config();
    gtk_text_buffer_set_text(_config_buffer, conf.data(), conf.length());
    config_changed_initialize(true, false);
    _config_clean_errors = true;

    return new ClickyLexerTInfo(_config_buffer, conf, error_handler());
}

void wmain::on_config_changed(bool replace, LexerTInfo *linfo)
{
    String conf = config();
    GatherErrorHandler *gerrh = error_handler();

    ClickyLexerTInfo *cinfo = dynamic_cast<ClickyLexerTInfo *>(linfo);
    const char *conf_begin = conf.begin();
    GtkTextTag *error_tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(_config_buffer), "error");
    for (GatherErrorHandler::iterator gi = gerrh->begin();
	 gi != gerrh->end(); ++gi)
	if (gi->errpos1 < gi->errpos2 && gi->level <= ErrorHandler::el_error)
	    cinfo->apply_tag(conf_begin + gi->errpos1, conf_begin + gi->errpos2, error_tag);

    _diagram->router_create(false, false);
    _config_clean_elements = true;
    config_choose_driver();

    if (router() && replace)
	etree_fill();
    if (gerrh->nerrors() || gerrh->nwarnings())
	on_error(true, String());
}

void wmain::set_save_file(const String &savefile, bool loading)
{
    _savefile = last_savefile = savefile;
    if (loading)
	config_changed_initialize(false, true);
}

void wmain::on_driver_changed()
{
    if (driver_local())
	gtk_widget_show(lookup_widget(_window, "toolbar_stop"));
    else
	gtk_widget_hide(lookup_widget(_window, "toolbar_stop"));
    if (!driver()) {
	gtk_widget_show(lookup_widget(_window, "toolbar_run"));
	gtk_widget_hide(lookup_widget(_window, "toolbar_install"));
    } else {
	gtk_widget_hide(lookup_widget(_window, "toolbar_run"));
	gtk_widget_show(lookup_widget(_window, "toolbar_install"));
    }
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_install"), !!driver());
    _handlers->redisplay();
}


/*****
 *
 * Throbber
 *
 */

static GdkPixbufAnimation *throbber_img = 0;
static bool throbber_loaded = false;

void wmain::on_throbber_changed(bool show)
{
    GtkWidget *throbberw = lookup_widget(_window, "throbber");
    if (show) {
	if (!throbber_loaded) {
	    throbber_loaded = true;
	    String throbber_file = clickpath_find_file("throbber.gif", "share/" PACKAGE, PACKAGE_DATA_DIR "/" PACKAGE);
	    // support for running before installing
	    if (!throbber_file && g_file_test("src/clicky", G_FILE_TEST_EXISTS))
		throbber_file = clickpath_find_file("throbber.gif", "", "./images");
	    if (throbber_file)
		throbber_img = gdk_pixbuf_animation_new_from_file(throbber_file.c_str(), NULL);
	}
	if (throbber_img)
	    gtk_image_set_from_animation(GTK_IMAGE(throbberw), throbber_img);
	else
	    gtk_image_set_from_stock(GTK_IMAGE(throbberw), GTK_STOCK_NETWORK, GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show(throbberw);
    } else {
	gtk_image_clear(GTK_IMAGE(throbberw));
    }
}


/*****
 *
 * initializing configuration
 *
 */

void wmain::show()
{
    gtk_widget_show(_window);
}

void wmain::set_show_toolbar(bool show_toolbar)
{
    _show_toolbar = show_toolbar;
    GtkWidget *toolbar = lookup_widget(_window, "toolbar1");
    if (_show_toolbar)
	gtk_widget_show(toolbar);
    else
	gtk_widget_hide(toolbar);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(_window, "menu_view_toolbar")), _show_toolbar);
}


/*****
 *
 * control socket connection
 *
 */

void wmain::on_handler_create(handler_value *hv, bool was_empty)
{
    if (was_empty) {	// first load, read style
	const delt *e = _diagram->elt(hv->element_name());
	ref_ptr<dhandler_style> dhs = ccss()->handler_style(this, e, hv);
	if (dhs) {
	    hv->set_flags(this, (hv->flags() & ~dhs->flags_mask) | dhs->flags);
	    if (dhs->autorefresh_period > 0
		&& dhs->autorefresh_period < hv->autorefresh_period())
		    hv->set_autorefresh_period(dhs->autorefresh_period);
	}
    }
    if (hv->notify_delt())
	_diagram->notify_read(hv);
    crouter::on_handler_create(hv, was_empty);
}

void wmain::on_handler_read(handler_value *hv, bool changed)
{
    if (changed && hv->notify_whandlers())
	_handlers->notify_read(hv);
    if (hv->notify_delt(changed))
	_diagram->notify_read(hv);
    crouter::on_handler_read(hv, changed);
}

void wmain::on_handler_write(const String &hname, const String &hvalue,
			     int status, messagevector &messages)
{
    _handlers->notify_write(hname, hvalue, status);
    crouter::on_handler_write(hname, hvalue, status, messages);
}

void wmain::on_handler_check_write(const String &hname,
				   int status, messagevector &messages)
{
    if (hname == "hotconfig" && status < 300) {
	// allow install handlers from now on
	gtk_widget_set_sensitive(lookup_widget(_window, "toolbar_install"), TRUE);
	gtk_widget_set_sensitive(lookup_widget(_window, "menu_install"), TRUE);
    }
    crouter::on_handler_check_write(hname, status, messages);
}


/*****
 *
 * element list
 *
 */

void wmain::set_show_list(bool show_list)
{
    _show_list = show_list;
    GtkWidget *errorpane = lookup_widget(_window, "errorpane");
    if (_show_list)
	gtk_widget_show(errorpane);
    else
	gtk_widget_hide(errorpane);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(_window, "menu_view_list")), _show_list);
}

void wmain::element_unhighlight()
{
    if (_element_highlight) {
	GtkTextIter i1, i2;
	gtk_text_buffer_get_start_iter(_config_buffer, &i1);
	gtk_text_buffer_get_end_iter(_config_buffer, &i2);
	gtk_text_buffer_remove_tag(_config_buffer, _config_element_highlight_tag, &i1, &i2);
	_element_highlight = 0;
    }
}

void wmain::element_show(String ename, int expand, bool incremental)
{
    // check if element exists
    RouterT *r = router();
    ScopeChain chain(r);
    ElementT *element = chain.push_element(ename);
    if (!element)
	ename = String();

    if (_eview_name == ename && incremental)
	/* do not change existing widgets, but continue to potentially
	   expand pane below */;

    else if (!r || !ename) {
	element_unhighlight();
	_eview_name = String();

	GtkLabel *l = GTK_LABEL(lookup_widget(_window, "eview_label"));
	gtk_label_set_text(l, "Element");

	GtkWidget *n = lookup_widget(_window, "eview_elementbox");
	gtk_widget_hide(n);

	n = lookup_widget(_window, "eview_classinfo_ports");
	gtk_label_set_text(GTK_LABEL(n), "");
	n = lookup_widget(_window, "eview_classinfo_processing");
	gtk_label_set_text(GTK_LABEL(n), "");
	n = lookup_widget(_window, "eview_classinfo_flow");
	gtk_label_set_text(GTK_LABEL(n), "");

	incremental = false;

    } else {
	element_unhighlight();
	_eview_name = ename;

	ElementClassT *eclass = chain.resolved_type(element);
	String config = chain.resolved_config(element->config());

	// set element name, class, and config
	GtkLabel *l = GTK_LABEL(lookup_widget(_window, "eview_label"));
	gtk_label_set_text(l, ename.c_str());

	GtkWidget *n = lookup_widget(_window, "eview_class");
	gtk_entry_set_text(GTK_ENTRY(n), element->type_name_c_str());
	gtk_widget_show(gtk_widget_get_parent(n));

	n = lookup_widget(_window, "eview_config");
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(n));
	gtk_text_buffer_set_text(buffer, config.data(), config.length());

	ElementMap::push_default(element_map());
	n = lookup_widget(_window, "eview_classinfo_ports");
	gtk_label_set_text(GTK_LABEL(n), eclass->port_count_code().c_str());
	n = lookup_widget(_window, "eview_classinfo_processing");
	gtk_label_set_text(GTK_LABEL(n), eclass->processing_code().c_str());
	n = lookup_widget(_window, "eview_classinfo_flow");
	gtk_label_set_text(GTK_LABEL(n), element->flow_code().c_str());
	ElementMap::pop_default();

	// clear handlers
	n = lookup_widget(_window, "eview_refresh");
	if (driver())
	    gtk_widget_show(n);
	else
	    gtk_widget_hide(n);

	// highlight config and diagram
	if (element->landmarkt().offset1() != element->landmarkt().offset2()
	    && expand >= 0 && _config_clean_elements)
	    _element_highlight = element;
    }

    // expand and highlight whether or not viewed element changed
    if (expand != 0) {
	GtkWidget *n = lookup_widget(_window, "eviewbox");
	if (expand > 0 && !gtk_widget_get_visible(n)) {
	    gtk_widget_show(n);
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(_window, "menu_view_element")), TRUE);
	    GtkPaned *paned = GTK_PANED(lookup_widget(_window, "eviewpane"));
	    if (GTK_WIDGET(paned)->allocation.width > 275)
		gtk_paned_set_position(paned, GTK_WIDGET(paned)->allocation.width - 225);
	    else
		gtk_paned_set_position(paned, GTK_WIDGET(paned)->allocation.width - (guint) (GTK_WIDGET(paned)->allocation.width * 225. / 275.));
	} else if (expand < 0) {
	    gtk_widget_hide(n);
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(_window, "menu_view_element")), FALSE);
	}
    }

    // display in handlers
    _handlers->display(_eview_name, incremental);

    // highlight config
    if (_element_highlight && expand >= 0) {
	GtkTextIter i1, i2;
	gtk_text_buffer_get_iter_at_offset(_config_buffer, &i1, _element_highlight->landmarkt().offset1());
	gtk_text_buffer_get_iter_at_offset(_config_buffer, &i2, _element_highlight->landmarkt().offset2());
	gtk_text_buffer_apply_tag(_config_buffer, _config_element_highlight_tag, &i1, &i2);
	if (expand > 0) {
	    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(_config_view), &i2, 0, FALSE, 0, 0);
	    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(_config_view), &i1, 0, FALSE, 0, 0);
	}
    }
}

extern "C" {
static void on_elementtree_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    GtkTreeIter iter;
    char *ename;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
	gtk_tree_model_get(model, &iter, 1, &ename, -1);
	rw->element_show(ename, 1, true);
	rw->diagram()->element_show(ename, true);
	g_free(ename);
    }
}

static void on_elementtree_select(GtkTreeSelection *selection, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    GtkTreeIter iter;
    GtkTreeModel *model;
    char *ename;
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
	gtk_tree_model_get(model, &iter, 1, &ename, -1);
	rw->element_show(ename, 0, true);
	rw->diagram()->element_show(ename, false);
	g_free(ename);
    }
}

static void on_eview_close_clicked(GtkButton *button, gpointer)
{
    gtk_widget_hide(lookup_widget(GTK_WIDGET(button), "eviewbox"));
}

static void on_eview_refresh_clicked(GtkButton *, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->handlers()->refresh_all(true);
}
}

namespace {
bool ename_sorter(const wmain::element_lister &a,
		  const wmain::element_lister &b) {
    return click_strcmp(a.compound + a.name, b.compound + b.name) < 0;
}
bool eclass_sorter(const wmain::element_lister &a,
		   const wmain::element_lister &b) {
    int c = click_strcmp(a.element->type_name(), b.element->type_name());
    if (c == 0)
	return click_strcmp(a.name, b.name) < 0;
    else
	return c < 0;
}
}

void wmain::fill_elements(RouterT *r, const String &compound, bool only_primitive, const VariableEnvironment &scope, Vector<element_lister> &v)
{
    for (RouterT::iterator i = r->begin_elements(); i != r->end_elements(); ++i) {
	if (i->tunnel())
	    continue;

	element_lister el;
	el.compound = compound;
	el.name = i->name();
	el.element = i.get();

	VariableEnvironment new_scope(0);
	ElementClassT *eclass = i->resolve(scope, &new_scope);
	RouterT *subr = eclass->cast_router();

	if (!only_primitive || !subr)
	    v.push_back(el);
	if (subr)
	    fill_elements(subr, el.compound + el.name + "/", only_primitive, new_scope, v);
    }
}

Vector<wmain::element_lister>::iterator wmain::fill_elements_tree_store_helper(GtkTreeStore *store, GtkTreeIter *parent, Vector<element_lister>::iterator it, Vector<element_lister>::iterator end)
{
    StringAccum sa1, sa2;
    GtkTreeIter li;

    gtk_tree_store_append(store, &li, parent);

    if (_elist_sort == elist_sort_class)
	sa1 << it->element->printable_type_name() << ' ' << it->compound << it->name;
    else
	sa1 << it->name << " :: " << it->element->printable_type_name();
    sa2 << it->compound << it->name;
    gtk_tree_store_set(store, &li, 0, sa1.c_str(), 1, sa2.c_str(), -1);

    Vector<element_lister>::iterator next = it + 1;
    while (next != end && next->compound.length() > it->compound.length()
	   && _elist_sort != elist_sort_class
	   && next->compound == it->compound + it->name + "/")
	next = fill_elements_tree_store_helper(store, &li, next, end);

    return next;
}

void wmain::fill_elements_tree_store(GtkTreeStore *store, RouterT *r)
{
    Vector<element_lister> v;
    fill_elements(r, "", (_elist_sort == elist_sort_class),
		  VariableEnvironment(0), v);

    if (_elist_sort == elist_sort_name)
	std::sort(v.begin(), v.end(), ename_sorter);
    else if (_elist_sort == elist_sort_class)
	std::sort(v.begin(), v.end(), eclass_sorter);

    Vector<element_lister>::iterator it = v.begin();
    while (it != v.end())
	it = fill_elements_tree_store_helper(store, 0, it, v.end());
}

void wmain::element_tree_sort(int state)
{
    if (state >= 0 && state <= 2 && _elist_sort == state)
	return;
    if (state >= 0 && state <= 2)
	_elist_sort = state;
    else
	_elist_sort = (_elist_sort + 1) % 3;

    GtkButton *b = GTK_BUTTON(lookup_widget(_window, "elementtreesort"));
    if (_elist_sort == elist_sort_none)
	gtk_button_set_label(b, "Sort: None");
    else if (_elist_sort == elist_sort_name)
	gtk_button_set_label(b, "Sort: Name");
    else if (_elist_sort == elist_sort_class)
	gtk_button_set_label(b, "Sort: Class");

    gtk_tree_store_clear(_elist_store);
    fill_elements_tree_store(_elist_store, router());
}

void wmain::etree_fill() {
    // elements list
    _elist_view = GTK_TREE_VIEW(lookup_widget(_window, "elementtree"));

    if (!_elist_store) {
	GtkTreeViewColumn *col = gtk_tree_view_column_new();
	gtk_tree_view_append_column(_elist_view, col);

	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, TRUE);
	gtk_tree_view_column_add_attribute(col, renderer, "text", 0);

	_elist_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

	GtkTreeModel *model = GTK_TREE_MODEL(_elist_store);
	gtk_tree_view_set_model(_elist_view, model);
	g_object_unref(model);

	GtkTreeSelection *selection = gtk_tree_view_get_selection(_elist_view);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(selection, "changed",
			 G_CALLBACK(on_elementtree_select), this);
	g_signal_connect(_elist_view, "row-activated",
			 G_CALLBACK(on_elementtree_row_activated), this);

	g_signal_connect(G_OBJECT(lookup_widget(_window, "eview_close")), "clicked",
			 G_CALLBACK(on_eview_close_clicked), this);
	g_signal_connect(G_OBJECT(lookup_widget(_window, "eview_refresh")), "clicked",
			 G_CALLBACK(on_eview_refresh_clicked), this);
    } else
	gtk_tree_store_clear(_elist_store);

    if (router())
	fill_elements_tree_store(_elist_store, router());

    element_show(_eview_name, 0, false);
    diagram()->element_show(_eview_name, false);
}


/*****
 *
 * Error views
 *
 */

void wmain::error_unhighlight()
{
    if (_error_highlight_index >= 0) {
	GatherErrorHandler *gerrh = error_handler();
	GatherErrorHandler::iterator gi = gerrh->begin() + _error_highlight_index;
	GtkTextIter i1, i2;
	if (gi->errpos1 < gi->errpos2) {
	    gtk_text_buffer_get_start_iter(_config_buffer, &i1);
	    gtk_text_buffer_get_end_iter(_config_buffer, &i2);
	    gtk_text_buffer_remove_tag(_config_buffer, _config_error_highlight_tag, &i1, &i2);
	}
	gtk_text_buffer_get_iter_at_offset(_error_buffer, &i1, gi->offset1);
	gtk_text_buffer_get_iter_at_offset(_error_buffer, &i2, gi->offset2);
	gtk_text_buffer_remove_tag(_error_buffer, _error_highlight_tag, &i1, &i2);
	_error_highlight_index = -1;
    }
}

bool wmain::error_view_motion_offsets(int off1, int off2, int eindex)
{
    if (eindex != _error_hover_index) {
	GtkTextIter i1, i2;
	GatherErrorHandler *gerrh = error_handler();
	if (_error_hover_index >= 0) {
	    GatherErrorHandler::iterator gi = gerrh->begin() + _error_hover_index;
	    gtk_text_buffer_get_iter_at_offset(_error_buffer, &i1, gi->offset1);
	    gtk_text_buffer_get_iter_at_offset(_error_buffer, &i2, gi->offset2);
	    gtk_text_buffer_remove_tag(_error_buffer, _error_hover_tag, &i1, &i2);
	}
	if (off1 != off2) {
	    gtk_text_buffer_get_iter_at_offset(_error_buffer, &i1, off1);
	    gtk_text_buffer_get_iter_at_offset(_error_buffer, &i2, off2);
	    gtk_text_buffer_apply_tag(_error_buffer, _error_hover_tag, &i1, &i2);
	    gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(_error_view), GTK_TEXT_WINDOW_TEXT), _link_cursor);
	} else
	    gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(_error_view), GTK_TEXT_WINDOW_TEXT), _normal_cursor);
	_error_hover_index = eindex;
	return true;
    } else
	return false;
}

bool wmain::error_view_motion_position(gint x, gint y)
{
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(_error_view), GTK_TEXT_WINDOW_WIDGET, x, y, &x, &y);

    GtkTextIter i;
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(_error_view), &i, x, y);
    GatherErrorHandler *gerrh = error_handler();
    GatherErrorHandler::iterator message = gerrh->find_offset(gtk_text_iter_get_offset(&i));

    // get buffer positions
    if (message != gerrh->end() && !_config_clean_errors)
	message->errpos1 = message->errpos2 = 0;
    if (message != gerrh->end() && message->errpos1 == 0 && message->errpos2 == 0) {
	const char *next;
	unsigned lineno;
	if (message->message.length() > 8
	    && memcmp(message->message.data(), "config:", 7) == 0
	    && (next = cp_integer(message->message.begin() + 7, message->message.end(), 10, &lineno)) < message->message.end()
	    && *next == ':'
	    && lineno <= (unsigned) gtk_text_buffer_get_line_count(_config_buffer)) {
	    GtkTextIter iter1, iter2;
	    gtk_text_buffer_get_iter_at_line_offset(_config_buffer, &iter1, lineno - 1, 0);
	    iter2 = iter1;
	    gtk_text_iter_forward_line(&iter2);
	    message->errpos1 = gtk_text_iter_get_offset(&iter1);
	    message->errpos2 = gtk_text_iter_get_offset(&iter2);
	}
    }

    // scroll to named position
    bool result;
    if (message == gerrh->end() || message->errpos1 >= message->errpos2)
	result = error_view_motion_offsets(0, 0, -1);
    else
	result = error_view_motion_offsets(message->offset1, message->offset2, message - gerrh->begin());

    // get more motion events
    GdkModifierType mod;
    (void) gdk_window_get_pointer(_error_view->window, &x, &y, &mod);

    return result;
}

gboolean wmain::error_view_event(GdkEvent *event)
{
    if (event->type == GDK_MOTION_NOTIFY)
	error_view_motion_position((gint) event->motion.x, (gint) event->motion.y);
    else if (event->type == GDK_LEAVE_NOTIFY && event->crossing.mode == GDK_CROSSING_NORMAL)
	error_view_motion_offsets(0, 0, -1);
    else if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
	error_unhighlight();
	if (_error_hover_index >= 0) {
	    _error_highlight_x = event->button.x;
	    _error_highlight_y = event->button.y;
	}
    } else if (event->type == GDK_BUTTON_PRESS && event->button.button == 1)
	error_unhighlight();
    else if (event->type == GDK_BUTTON_RELEASE && event->button.button == 1
	     && _error_hover_index >= 0
	     && fabs(event->button.x - _error_highlight_x) <= 10
	     && fabs(event->button.y - _error_highlight_y) <= 10) {
	GatherErrorHandler *gerrh = error_handler();
	GatherErrorHandler::iterator gi = gerrh->begin() + _error_hover_index;
	if (gi->errpos1 < gi->errpos2) {
	    _error_highlight_index = _error_hover_index;
	    GatherErrorHandler::iterator gi = gerrh->begin() + _error_highlight_index;
	    GtkTextIter i1, i2;
	    gtk_text_buffer_get_iter_at_offset(_config_buffer, &i1, gi->errpos1);
	    gtk_text_buffer_get_iter_at_offset(_config_buffer, &i2, gi->errpos2);
	    gtk_text_buffer_apply_tag(_config_buffer, _config_error_highlight_tag, &i1, &i2);
	    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(_config_view), &i2, 0, FALSE, 0, 0);
	    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(_config_view), &i1, 0, FALSE, 0, 0);
	    gtk_text_buffer_get_iter_at_offset(_error_buffer, &i1, gi->offset1);
	    gtk_text_buffer_get_iter_at_offset(_error_buffer, &i2, gi->offset2);
	    gtk_text_buffer_apply_tag(_error_buffer, _error_highlight_tag, &i1, &i2);
	}
    }
    return FALSE;
}

extern "C" {
static gboolean on_error_view_event(GtkWidget *, GdkEvent *event, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    return rw->error_view_event(event);
}

static gboolean on_error_scroll_timeout(gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_error_scroll_timeout();
    return FALSE;
}
}

void wmain::on_error(bool replace, const String &dialog)
{
    GatherErrorHandler *gerrh = error_handler();
    if (!gerrh->size()) {
	gtk_widget_hide(lookup_widget(_window, "errorviewbox"));
	gtk_widget_show(lookup_widget(_window, "elementtreelabel"));
	gtk_widget_hide(lookup_widget(_window, "elementtreeexpander"));
	gtk_widget_show(lookup_widget(_window, "elementtreewindow"));
	gtk_widget_show(lookup_widget(_window, "elementtreesort"));
	gtk_paned_set_position(GTK_PANED(lookup_widget(_window, "errorpane")), 0);
	gtk_text_buffer_set_text(_error_buffer, "", 0);
	return;
    }

    // set up window
    gtk_widget_show(lookup_widget(_window, "errorviewbox"));
    gtk_widget_hide(lookup_widget(_window, "elementtreelabel"));
    GtkWidget *treeexpander = lookup_widget(_window, "elementtreeexpander");
    gtk_widget_show(treeexpander);
    GtkPaned *pane = GTK_PANED(lookup_widget(_window, "errorpane"));
    GtkWidget *treewindow = lookup_widget(_window, "elementtreewindow");
    if (gerrh->nerrors() && replace) {
	if (!_show_list)
	    set_show_list(true);
	gtk_widget_hide(treewindow);
	gtk_widget_hide(lookup_widget(_window, "elementtreesort"));
	gtk_paned_set_position(pane, 2147483647);
    }
    gtk_expander_set_expanded(GTK_EXPANDER(treeexpander), gtk_widget_get_visible(treewindow));
    if ((!gerrh->nerrors() || !replace) && gtk_widget_get_visible(treewindow))
	gtk_paned_set_position(pane, GTK_WIDGET(pane)->allocation.height / 3);

    // strip filename errors from error list
    if (replace) {
	_error_endpos = 0;
	gtk_text_buffer_set_text(_error_buffer, "", 0);
    }
    gerrh->translate_prefix(landmark() + ": ", "", _error_endpos);
    StringAccum sa;
    for (GatherErrorHandler::iterator gi = gerrh->begin() + _error_endpos;
	 gi != gerrh->end(); ++gi)
	sa << gi->message;
    if (sa.length()) {
	GtkTextIter iter;
	gtk_text_buffer_get_end_iter(_error_buffer, &iter);
	gtk_text_buffer_insert(_error_buffer, &iter, sa.data(), sa.length());
    }
    _error_endpos = gerrh->size();

    if (_error_scroller)
	g_source_remove(_error_scroller);

    // show the error dialog
    if (dialog) {
	gtk_widget_show(_window);
	GtkWidget *dwidget = gtk_message_dialog_new(GTK_WINDOW(_window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    "%s",
				    dialog.c_str());
	gtk_dialog_run(GTK_DIALOG(dwidget));
	gtk_widget_destroy(dwidget);
    }

    _error_scroller = g_timeout_add(20, clicky::on_error_scroll_timeout, this);
}

void wmain::on_error_scroll_timeout()
{
    GtkWidget *error_scrolled = lookup_widget(_window, "errorviewwindow");
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(error_scrolled));
    gtk_adjustment_set_value(vadj, vadj->upper - vadj->page_size);
    _error_scroller = 0;
}

extern "C" {
static void error_expanders_callback(GObject *object, GParamSpec *, gpointer)
{
    GtkExpander *e = GTK_EXPANDER(object);
    GtkExpander *listex = GTK_EXPANDER(lookup_widget(GTK_WIDGET(e), "elementtreeexpander"));
    GtkExpander *errorex = GTK_EXPANDER(lookup_widget(GTK_WIDGET(e), "errorviewexpander"));

    if (!gtk_expander_get_expanded(listex) && !gtk_expander_get_expanded(errorex))
	gtk_expander_set_expanded(e == listex ? errorex : listex, TRUE);

    const char *child_name = (e == listex ? "elementtreewindow" : "errorviewwindow");
    GtkPaned *pane = GTK_PANED(lookup_widget(GTK_WIDGET(e), "errorpane"));
    if (gtk_expander_get_expanded(e)) {
	gtk_widget_show(lookup_widget(GTK_WIDGET(e), child_name));
	if (e == listex)
	    gtk_widget_show(lookup_widget(GTK_WIDGET(e), "elementtreesort"));
	if (gtk_expander_get_expanded(e == listex ? errorex : listex))
	    gtk_paned_set_position(pane, GTK_WIDGET(pane)->allocation.height / 2);
    } else {
	gtk_widget_hide(lookup_widget(GTK_WIDGET(e), child_name));
	if (e == listex)
	    gtk_widget_hide(lookup_widget(GTK_WIDGET(e), "elementtreesort"));
	gtk_paned_set_position(pane, (e == listex ? 2147483647 : 0));
    }
}

static void elementtreesort_callback(GtkButton *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->element_tree_sort(-1);
}
}


/*****
 *
 * Configuration editing
 *
 */

extern "C" {
static void on_eview_classexpander_expanded(GObject *object, GParamSpec *, gpointer)
{
    GtkExpander *e = GTK_EXPANDER(object);
    GtkWidget *other = lookup_widget(GTK_WIDGET(e), "eview_classinfo");
    if (gtk_expander_get_expanded(e))
	gtk_widget_show(other);
    else
	gtk_widget_hide(other);
}

static void config_changed(GtkTextBuffer *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_config_changed();
}
}

void wmain::config_changed_initialize(bool check, bool save)
{
    if (!_config_changed_signal)
	_config_changed_signal = g_signal_connect(_config_buffer, "changed", G_CALLBACK(clicky::config_changed), this);
    (void) check, (void) save;
}

void wmain::on_config_changed()
{
    if (_config_changed_signal)
	g_signal_handler_disconnect(_config_buffer, _config_changed_signal);
    _config_changed_signal = 0;
    _config_clean_errors = _config_clean_elements = false;
    error_unhighlight();
    element_unhighlight();
}

void wmain::config_check(bool install)
{
    GtkTextIter i1, i2;
    gtk_text_buffer_get_start_iter(_config_buffer, &i1);
    gtk_text_buffer_get_end_iter(_config_buffer, &i2);
    gchar *str = gtk_text_buffer_get_text(_config_buffer, &i1, &i2, FALSE);

    String config;
    RouterT *r = router();
    if (r && r->narchive()) {
	Vector<ArchiveElement> ar(r->archive());
	ArchiveElement ae = init_archive_element("config", 0600);
	ae.data = String(str);
	ar.push_back(ae);
	config = ArchiveElement::unparse(ar);
    } else
	config = str;

    g_free(str);

    if (install)
	if (cdriver *cd = driver()) {
	    cd->do_write("hotconfig", config, cdriver::dflag_clear);
	    cd->do_read("list", String(), 0);
	}

    set_landmark("config");
    set_config(config, install || !driver());
}


/*****
 *
 * Diagram mode
 *
 */

void wmain::set_diagram_mode(int configuration, int diagram)
{
    if (configuration < 0)
	configuration = gtk_widget_get_visible(lookup_widget(_window, "configwindow"));
    if (diagram < 0)
	diagram = gtk_widget_get_visible(lookup_widget(_window, "diagramwindow"));
    if (!configuration && !diagram)
	return;

    if (configuration && diagram) {
	GtkWidget *paned = lookup_widget(_window, "configdiagrampaned");
	if (gtk_paned_get_position(GTK_PANED(paned)) <= 0)
	    gtk_paned_set_position(GTK_PANED(paned), paned->allocation.width / 2);
    }

    GtkWidget *configm = lookup_widget(_window, "menu_view_configuration");
    gtk_widget_set_sensitive(configm, !configuration || diagram);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(configm), configuration);
    if (configuration)
	gtk_widget_show(lookup_widget(_window, "configwindow"));
    else
	gtk_widget_hide(lookup_widget(_window, "configwindow"));
    if (!configuration || diagram)
	gtk_widget_show(lookup_widget(_window, "toolbar_configuration"));
    else
	gtk_widget_hide(lookup_widget(_window, "toolbar_configuration"));

    GtkWidget *diagramm = lookup_widget(_window, "menu_view_diagram");
    gtk_widget_set_sensitive(diagramm, configuration || !diagram);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(diagramm), diagram);
    if (diagram)
	gtk_widget_show(lookup_widget(_window, "diagramwindow"));
    else
	gtk_widget_hide(lookup_widget(_window, "diagramwindow"));
    if (!diagram || configuration)
	gtk_widget_show(lookup_widget(_window, "toolbar_diagram"));
    else
	gtk_widget_hide(lookup_widget(_window, "toolbar_diagram"));
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_zoom_in"), diagram);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_zoom_out"), diagram);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_normal_size"), diagram);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_zoom_fit"), diagram);
}

void wmain::repaint(const rectangle &rect)
{
    if (_diagram->visible())
	_diagram->redraw(rect);
}

void wmain::repaint_if_visible(const rectangle &rect, double dimen)
{
    if (_diagram->visible() && fabs(dimen * _diagram->scale()) > 0.5)
	_diagram->redraw(rect);
}

}
