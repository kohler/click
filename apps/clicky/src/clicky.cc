#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include <click/error.hh>
#include <click/driver.hh>
#include <clicktool/toolutils.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/pair.hh>
#include <click/vector.cc>
#include <clicktool/routert.hh>
#include <clicktool/lexert.hh>
#include <clicktool/lexertinfo.hh>
#include "wrouter.hh"
#include "wdriver.hh"
#include <netdb.h>

extern "C" {
#include "interface.h"
#include "support.h"
}

int
main(int argc, char *argv[])
{
    click_static_initialize();

#ifdef ENABLE_NLS
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);
#endif

    // local styles
    String gtkrc_file = clickpath_find_file("clicky.gtkrc", "share/" PACKAGE, PACKAGE_DATA_DIR "/" PACKAGE);
    // support for running before installing 
    if (!gtkrc_file && g_file_test("src/clicky", G_FILE_TEST_EXISTS))
	gtkrc_file = clickpath_find_file("clicky.gtkrc", "", ".");
    if (gtkrc_file)
	gtk_rc_add_default_file(gtkrc_file.c_str());
    
    gtk_set_locale();
    gtk_init(&argc, &argv);
    add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");

    if (argc == 1) {
	RouterWindow *rw = new RouterWindow;
	rw->show();
    }

    for (; argc > 1; argc--, argv++) {
	RouterWindow *rw = new RouterWindow;
	String filename = argv[1];
	rw->set_landmark(filename);
	int colon = filename.find_right(':');
	uint16_t port;
	if (colon >= 0 && cp_tcpudp_port(filename.substring(colon + 1), IP_PROTO_TCP, &port)) {
	    IPAddress addr;
	    if (cp_host_port(filename.substring(0, colon), filename.substring(colon + 1), &addr, &port, rw->error_handler())) {
		bool ready = false;
		GIOChannel *channel = RouterWindow::wdriver_csocket::start_connect(addr, port, &ready, rw->error_handler());
		if (rw->error_handler()->size())
		    rw->error_handler()->run_dialog(rw->window());
		if (channel)
		    (void) new RouterWindow::wdriver_csocket(rw, channel, ready);
	    }
	} else {
	    String s = file_string(filename, rw->error_handler());
	    if (!s && rw->error_handler()->nerrors())
		rw->error_handler()->run_dialog(rw->window());
	    rw->set_config(s, true);
	    rw->set_save_file(filename, (bool) s);
	}
	rw->show();
    }

    gtk_main();
    return 0;
}

