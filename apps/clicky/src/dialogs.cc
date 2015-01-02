#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include <click/userutils.hh>
#include <click/confparse.hh>
#include <clicktool/elementmap.hh>
#include "crouter.hh"
#include "cdriver.hh"
#include "wmain.hh"
#include "wdiagram.hh"
#include <netdb.h>
#include <unistd.h>
extern "C" {
#include "interface.h"
#include "support.h"
}
namespace clicky {

extern "C" {
static void on_new_window_activate(GtkMenuItem *, gpointer user_data)
{
    wmain *old_wm = reinterpret_cast<wmain *>(user_data);
    wmain *rw = new wmain(old_wm->show_toolbar(), old_wm->show_list(), old_wm->ccss());
    rw->show();
}

static void on_open_file_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_open_file();
}

static void on_open_socket_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_open_socket();
}

static void on_open_kernel_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_open_kernel();
}

static void on_save_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_save_file(false);
}

static void on_save_as_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_save_file(true);
}

static void on_export_diagram_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->on_export_diagram();
}

static void on_quit_activate(GtkMenuItem *, gpointer)
{
    gtk_main_quit();
}

static void on_check_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->config_check(false);
}

static void on_install_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->config_check(true);
}

static void on_view_toolbar_toggled(GtkCheckMenuItem *check, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->set_show_toolbar(check->active);
}

static void on_view_list_toggled(GtkCheckMenuItem *check, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->set_show_list(check->active);
}

static void on_view_element_toggled(GtkCheckMenuItem *check, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->element_show(rw->element_showing(), check->active ? 1 : -1, false);
}

static void on_view_configuration_toggled(GtkCheckMenuItem *check, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->set_diagram_mode(check->active, -1);
}

static void on_view_diagram_toggled(GtkCheckMenuItem *check, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->set_diagram_mode(-1, check->active);
}

static void on_toolbar_run_activate(GtkToolButton *, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->buffer_to_config();
    rw->run(ErrorHandler::default_handler());
}

static void on_toolbar_stop_activate(GtkToolButton *, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->kill_driver();
}

static void on_toolbar_diagram_activate(GtkToolButton *, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->set_diagram_mode(false, true);
}

static void on_toolbar_configuration_activate(GtkToolButton *, gpointer user_data)
{
    wmain *rw = reinterpret_cast<wmain *>(user_data);
    rw->set_diagram_mode(true, false);
}

static void on_zoom_in_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->diagram()->zoom(true, 1);
}

static void on_zoom_out_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->diagram()->zoom(true, -1);
}

static void on_normal_size_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->diagram()->zoom(false, 0);
}

static void on_zoom_fit_activate(GtkMenuItem *, gpointer user_data)
{
    reinterpret_cast<wmain *>(user_data)->diagram()->zoom(false, -10000);
}

static void on_config_userlevel_activate(GtkMenuItem *m, gpointer user_data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(m)))
	reinterpret_cast<wmain *>(user_data)->config_set_driver(Driver::USERLEVEL);
}

static void on_config_linuxmodule_activate(GtkMenuItem *m, gpointer user_data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(m)))
	reinterpret_cast<wmain *>(user_data)->config_set_driver(Driver::LINUXMODULE);
}

static void on_config_bsdmodule_activate(GtkMenuItem *m, gpointer user_data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(m)))
	reinterpret_cast<wmain *>(user_data)->config_set_driver(Driver::BSDMODULE);
}

static void on_config_ns_activate(GtkMenuItem *m, gpointer user_data)
{
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(m)))
	reinterpret_cast<wmain *>(user_data)->config_set_driver(Driver::NSMODULE);
}
}

void wmain::dialogs_connect()
{
    g_signal_connect(lookup_widget(_window, "menu_new_window"), "activate",
		     G_CALLBACK(on_new_window_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_open_file"), "activate",
		     G_CALLBACK(on_open_file_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_open_socket"), "activate",
		     G_CALLBACK(on_open_socket_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_open_kernel"), "activate",
		     G_CALLBACK(on_open_kernel_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_save"), "activate",
		     G_CALLBACK(on_save_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_save_as"), "activate",
		     G_CALLBACK(on_save_as_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_export_diagram"), "activate",
		     G_CALLBACK(on_export_diagram_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_check"), "activate",
		     G_CALLBACK(on_check_activate), this);
    g_signal_connect(lookup_widget(_window, "toolbar_install"), "clicked",
		     G_CALLBACK(on_install_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_install"), "activate",
		     G_CALLBACK(on_install_activate), this);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_install"), FALSE);
    g_signal_connect(lookup_widget(_window, "menu_quit"), "activate",
		     G_CALLBACK(on_quit_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_view_toolbar"), "toggled",
		     G_CALLBACK(on_view_toolbar_toggled), this);
    g_signal_connect(lookup_widget(_window, "menu_view_list"), "toggled",
		     G_CALLBACK(on_view_list_toggled), this);
    g_signal_connect(lookup_widget(_window, "menu_view_element"), "toggled",
		     G_CALLBACK(on_view_element_toggled), this);
    g_signal_connect(lookup_widget(_window, "menu_view_configuration"), "toggled",
		     G_CALLBACK(on_view_configuration_toggled), this);
    g_signal_connect(lookup_widget(_window, "menu_view_diagram"), "toggled",
		     G_CALLBACK(on_view_diagram_toggled), this);
    g_signal_connect(lookup_widget(_window, "toolbar_run"), "clicked",
		     G_CALLBACK(on_toolbar_run_activate), this);
    g_signal_connect(lookup_widget(_window, "toolbar_stop"), "clicked",
		     G_CALLBACK(on_toolbar_stop_activate), this);
    g_signal_connect(lookup_widget(_window, "toolbar_configuration"), "clicked",
		     G_CALLBACK(on_toolbar_configuration_activate), this);
    g_signal_connect(lookup_widget(_window, "toolbar_diagram"), "clicked",
		     G_CALLBACK(on_toolbar_diagram_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_zoom_in"), "activate",
		     G_CALLBACK(on_zoom_in_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_zoom_out"), "activate",
		     G_CALLBACK(on_zoom_out_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_normal_size"), "activate",
		     G_CALLBACK(on_normal_size_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_zoom_fit"), "activate",
		     G_CALLBACK(on_zoom_fit_activate), this);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_zoom_in"), FALSE);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_zoom_out"), FALSE);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_normal_size"), FALSE);
    gtk_widget_set_sensitive(lookup_widget(_window, "menu_zoom_fit"), FALSE);
    g_signal_connect(lookup_widget(_window, "menu_config_userlevel"), "activate",
		     G_CALLBACK(on_config_userlevel_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_config_linuxmodule"), "activate",
		     G_CALLBACK(on_config_linuxmodule_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_config_bsdmodule"), "activate",
		     G_CALLBACK(on_config_bsdmodule_activate), this);
    g_signal_connect(lookup_widget(_window, "menu_config_ns"), "activate",
		     G_CALLBACK(on_config_ns_activate), this);
}

void wmain::on_open_file()
{
    static GtkFileFilter *filter;

    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File",
				GTK_WINDOW(_window),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				(const char *) NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    String last_filename(_savefile ? _savefile : last_savefile);
    if (last_filename) {
	gchar *dir = g_path_get_dirname(last_filename.c_str());
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);
	g_free(dir);
    }

    if (!filter) {
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Click Configurations");
	gtk_file_filter_add_pattern(filter, "*.click");
	g_object_ref(G_OBJECT(filter));
    }

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
	char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	GatherErrorHandler gerrh(true);
	String s = file_string(filename, &gerrh);
	if (gerrh.nerrors()) {
	    gerrh.translate_prefix(filename, "Error opening '" + String(filename) + "'");
	    gerrh.run_dialog(GTK_WINDOW(dialog));
	} else {
	    wmain *rw;
	    if (empty()) {
		rw = this;
		rw->clear(true);
	    } else
		rw = new wmain(_show_toolbar, _show_list, ccss());
	    rw->set_landmark(filename);
	    rw->set_config(s, true);
	    rw->set_save_file(filename, true);
	    rw->show();
	}
	g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

extern "C" {
struct open_socket_helper {
    GMainLoop *loop;
    bool ready;
};
static gboolean open_socket_timeout_helper(gpointer user_data)
{
    open_socket_helper *h = reinterpret_cast<open_socket_helper *>(user_data);
    if (g_main_loop_is_running(h->loop))
	g_main_loop_quit(h->loop);
    return FALSE;
}
static gboolean open_socket_writable_helper(GIOChannel *, GIOCondition, gpointer user_data)
{
    open_socket_helper *h = reinterpret_cast<open_socket_helper *>(user_data);
    if (g_main_loop_is_running(h->loop)) {
	h->ready = true;
	g_main_loop_quit(h->loop);
    }
    return FALSE;
}
}

bool cp_host_port(const String &hosts, const String &ports, IPAddress *result_addr, uint16_t *result_port, ErrorHandler *errh)
{
    struct hostent *h = gethostbyname(hosts.c_str());
    if (!h) {
	errh->error("No such host '%s': %s", hosts.c_str(), hstrerror(h_errno));
	return false;
    } else if (h->h_addrtype != AF_INET) {
	errh->error("IPv6 addresses not yet supported");
	return false;
    }

    if (!cp_tcpudp_port(ports, IP_PROTO_TCP, result_port)) {
	errh->error("'%s' does not describe a TCP port", ports.c_str());
	return false;
    }

    *result_addr = IPAddress(*(struct in_addr *) h->h_addr);
    return true;
}

int do_fd_connected(int fd, ErrorHandler *errh)
{
    int x;
    socklen_t socklen = sizeof(x);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *) &x, &socklen) == -1)
	return errh->error("%s", strerror(errno));
    else if (x == EINPROGRESS)
	return 0;
    else if (x != 0)
	return errh->error("%s", strerror(x));
    else
	return 1;
}

void wmain::on_open_socket()
{
    GtkWidget *dialog = create_opensocketdialog();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(_window));

    while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
	GatherErrorHandler gerrh(true);
	String hosts(gtk_entry_get_text(GTK_ENTRY(lookup_widget(dialog, "host"))));
	String ports(gtk_entry_get_text(GTK_ENTRY(lookup_widget(dialog, "port"))));
	IPAddress addr;
	uint16_t port;
	if (!cp_host_port(hosts, ports, &addr, &port, &gerrh)) {
	    gerrh.run_dialog(GTK_WINDOW(dialog));
	    continue;
	}

	open_socket_helper helper = { NULL, false };
	GIOChannel *socket = csocket_cdriver::start_connect(addr, port, &helper.ready, &gerrh);
	if (!socket) {
	    gerrh.run_dialog(GTK_WINDOW(dialog));
	    continue;
	}

	// wait 1/4 second in case connection quickly fails
	if (!helper.ready) {
	    helper.loop = g_main_loop_new(NULL, FALSE);
	    guint a = g_io_add_watch(socket, (GIOCondition) (G_IO_OUT | G_IO_ERR | G_IO_HUP), open_socket_writable_helper, &helper);
	    guint b = g_timeout_add(250, open_socket_timeout_helper, &helper);
	    g_main_loop_run(helper.loop);
	    g_main_loop_unref(helper.loop);
	    g_source_remove(a);
	    g_source_remove(b);

	    if (helper.ready) {
		int r = do_fd_connected(g_io_channel_unix_get_fd(socket), &gerrh);
		if (r < 0) {
		    gerrh.run_dialog(GTK_WINDOW(dialog));
		    continue;
		} else if (r == 0)
		    helper.ready = false;
	    }
	}

	wmain *rw;
	if (empty()) {
	    rw = this;
	    rw->clear(true);
	} else
	    rw = new wmain(_show_toolbar, _show_list, ccss());
	rw->set_landmark(String(hosts) + ":" + String(ports));
	(void) new csocket_cdriver(rw, socket, helper.ready);
	rw->show();
	break;
    }

    gtk_widget_destroy(dialog);
}

void wmain::on_open_kernel()
{
    String prefix = "/click/";
    String config_name = prefix + "config";
    if (access(config_name.c_str(), R_OK) < 0) {
	GatherErrorHandler *gerrh = error_handler();
	int gerrh_pos = gerrh->size();
	gerrh->error("No kernel configuration installed: %s", strerror(errno));
	gerrh->run_dialog(GTK_WINDOW(_window), gerrh_pos);
    } else {
	wmain *rw;
	if (empty()) {
	    rw = this;
	    rw->clear(true);
	} else
	    rw = new wmain(_show_toolbar, _show_list, ccss());
	rw->set_landmark(prefix);
	(void) new clickfs_cdriver(rw, prefix);
	rw->show();
    }
}

void wmain::buffer_to_config()
{
    GtkTextIter i1, i2;
    gtk_text_buffer_get_start_iter(_config_buffer, &i1);
    gtk_text_buffer_get_end_iter(_config_buffer, &i2);
    char *data = gtk_text_buffer_get_text(_config_buffer, &i1, &i2, FALSE);
    set_config(String(data), true);
    g_free(data);
}

void wmain::on_save_file(bool save_as)
{
    if (save_as || !_savefile) {
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File",
				GTK_WINDOW(_window),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				(const char *) NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	if (_savefile)
	    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), _savefile.c_str());
	else if (last_savefile) {
	    gchar *dir = g_path_get_dirname(last_savefile.c_str());
	    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);
	    g_free(dir);
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
	    gtk_widget_destroy(dialog);
	    return;
	} else {
	    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	    if (!driver())
		set_landmark(filename);
	    set_save_file(filename, false);
	    g_free(filename);
	    gtk_widget_destroy(dialog);
	}
    }

    buffer_to_config();

    GError *err = NULL;
    String conf = config();
    if (!g_file_set_contents(_savefile.c_str(), conf.data(), conf.length(), &err)) {
	GatherErrorHandler gerrh(true);
	gerrh.error("Save error: %s", err->message);
	gerrh.run_dialog(GTK_WINDOW(_window));
	g_error_free(err);
    } else
	set_save_file(_savefile, true);
}

void wmain::on_export_diagram()
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Export Diagram",
				GTK_WINDOW(_window),
				GTK_FILE_CHOOSER_ACTION_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				(const char *) NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    GtkWidget *combo_extensions = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_extensions), "PDF");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_extensions), "SVG");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_extensions), 0);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), combo_extensions);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
	gtk_widget_destroy(dialog);
	return;
    } else {
	char final_filename[200];
	char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	char *filename_ext = strrchr(filename, '.');
	int export_to_index = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_extensions));
	const char *export_to;
	if (filename_ext && (strncmp(".pdf", filename_ext, 5) == 0 || strncmp(".svg", filename_ext, 5) == 0))
	    export_to = ""; // extension already valid
	else if (export_to_index == 0)
	    export_to = ".pdf";
	else
	    export_to = ".svg";
	snprintf(final_filename, 200, "%s%s", filename, export_to);
	final_filename[200-1] = '\0';
	g_free(filename);
	_diagram->export_diagram(final_filename, false);
	gtk_widget_destroy(dialog);
    }
}

void wmain::config_choose_driver()
{
    cdriver *cd = driver();
    ElementMap *emap = element_map();
    int driver_mask = (cd ? cd->driver_mask() : emap->provided_driver_mask());
    // first choose a driver
    for (int d = 0; d < Driver::COUNT; ++d)
	if ((1 << d) & driver_mask) {
	    select_driver(d);
	    break;
	}
    // then set menu items accordingly
    for (int d = 0; d < Driver::COUNT; ++d) {
	String name = String("menu_config_") + Driver::name(d);
	GtkWidget *w = lookup_widget(_window, name.c_str());
	gtk_widget_set_sensitive(w, (driver_mask & (1 << d)) != 0);
	if (d == selected_driver())
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), TRUE);
    }
}

void wmain::config_set_driver(int driver)
{
    if (driver != selected_driver()) {
	select_driver(driver);
	on_config_changed();
    }
}

}
