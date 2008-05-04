#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <click/config.h>
#include <click/error.hh>
#include <click/driver.hh>
#include <clicktool/toolutils.hh>
#include <click/confparse.hh>
#include <click/clp.h>
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
#include <click/hashtable.hh>
#include <netdb.h>

extern "C" {
#include "interface.h"
#include "support.h"
}

#define VERSION_OPT 300
#define STYLE_OPT 301
#define PORT_OPT 302
#define EXPRESSION_OPT 303
#define CLICKPATH_OPT 304
#define HELP_OPT 305
#define FILE_OPT 306
#define KERNEL_OPT 307
#define STYLE_EXPR_OPT 308

static const Clp_Option options[] = {
    { "version", 0, VERSION_OPT, 0, 0 },
    { "style", 's', STYLE_OPT, Clp_ValString, Clp_Negate|Clp_PreferredMatch },
    { "style-expr", 0, STYLE_EXPR_OPT, Clp_ValString, 0 },
    { "file", 'f', FILE_OPT, Clp_ValString, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
    { "port", 'p', PORT_OPT, Clp_ValString, 0 },
    { "kernel", 'k', KERNEL_OPT, 0, 0 },
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
    { "help", 0, HELP_OPT, 0, 0 }
};

static const char *program_name;

void usage()
{
    printf("\
'Clicky' is a Click configuration parser and displayer.\n\
\n\
Usage: %s [OPTION]... [ROUTERFILE]\n\
\n\
Options:\n\
  -f, --file FILE              Read router configuration from FILE.\n\
  -e, --expression EXPR        Use EXPR as router configuration.\n\
  -p, --port [HOST:]PORT       Connect to HOST:PORT for configuration.\n\
  -k, --kernel                 Read configuration from kernel.\n\
  -s, --style FILE             Add CCSS style information from FILE.\n\
      --style-expr STYLE       Add STYLE as CCSS style information.\n\
  -C, --clickpath PATH         Use PATH for CLICKPATH.\n\
      --help                   Print this message and exit.\n\
  -v, --version                Print version number and exit.\n\
\n\
Report bugs to <click@pdos.lcs.mit.edu>.\n", program_name);
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

    // read command line arguments
    Clp_Parser *clp =
	Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
    program_name = Clp_ProgramName(clp);
    
    String css_text;
    Vector<String> wfiles;
    Vector<int> wtypes;
    
    while (1) {
	int opt = Clp_Next(clp);
	switch (opt) {
	    
	  case VERSION_OPT:
	    printf("clicky (Click) %s\n", CLICK_VERSION);
	    printf("Copyright (c) 2008 Regents of the University of California\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
	    exit(0);
	    break;

	  case CLICKPATH_OPT:
	    set_clickpath(clp->vstr);
	    break;

	  case STYLE_OPT:
	    if (clp->negated)
		css_text = String();
	    else
		css_text += file_string(clp->vstr, ErrorHandler::default_handler());
	    break;

	  case STYLE_EXPR_OPT:
	    css_text += clp->vstr;
	    break;

	  case EXPRESSION_OPT:
	    wfiles.push_back(clp->vstr);
	    wtypes.push_back(1);
	    break;

	  case PORT_OPT:
	    if (strchr(clp->vstr, ':'))
		wfiles.push_back(clp->vstr);
	    else
		wfiles.push_back("localhost:" + String(clp->vstr));
	    wtypes.push_back(2);
	    break;

	  case FILE_OPT:
	    wfiles.push_back(clp->vstr);
	    wtypes.push_back(3);
	    break;

	  case KERNEL_OPT:
	    wfiles.push_back("<kernel>");
	    wtypes.push_back(4);
	    break;

	  case HELP_OPT:
	    usage();
	    exit(0);
	    break;

	  case Clp_NotOption:
	    wfiles.push_back(clp->vstr);
	    wtypes.push_back(2);
	    break;
      
	  case Clp_BadOption:
	    exit(1);
	    break;
      
	  case Clp_Done:
	    goto done;
	    
	}
    }
  
  done:
    if (wfiles.size() == 0) {
	clicky::wmain *rw = new clicky::wmain;
	rw->set_ccss_text(css_text);
	rw->show();
    }

    int colon;
    uint16_t port;
    for (int i = 0; i < wfiles.size(); i++) {
	clicky::wmain *rw = new clicky::wmain;
	rw->set_landmark(wtypes[i] == 1 ? "<config>" : wfiles[i]);
	rw->set_ccss_text(css_text);
	if (wtypes[i] == 1)
	    rw->set_config(wfiles[i], true);
	else if (wtypes[i] == 2
		 && (colon = wfiles[i].find_right(':')) >= 0
		 && cp_tcpudp_port(wfiles[i].substring(colon + 1), IP_PROTO_TCP, &port)) {
	    IPAddress addr;
	    if (clicky::cp_host_port(wfiles[i].substring(0, colon), wfiles[i].substring(colon + 1), &addr, &port, rw->error_handler())) {
		bool ready = false;
		GIOChannel *channel = clicky::csocket_wdriver::start_connect(addr, port, &ready, rw->error_handler());
		if (rw->error_handler()->size())
		    rw->error_handler()->run_dialog(rw->window());
		if (channel)
		    (void) new clicky::csocket_wdriver(rw, channel, ready);
	    }
	} else if (wtypes[i] == 4) {
	    (void) new clicky::clickfs_wdriver(rw, "/click/");
	} else {
	    String s = file_string(wfiles[i], rw->error_handler());
	    if (!s && rw->error_handler()->nerrors())
		rw->error_handler()->run_dialog(rw->window());
	    rw->set_config(s, true);
	    rw->set_save_file(wfiles[i], (bool) s);
	}
	rw->show();
    }

    gtk_main();
    return 0;
}
