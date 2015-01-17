#ifdef HAVE_CONFIG_H
# include <config.h>
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
#include <clicktool/routert.hh>
#include <clicktool/lexert.hh>
#include <clicktool/lexertinfo.hh>
#include "crouter.hh"
#include "cdriver.hh"
#include "wmain.hh"
#include "tmain.hh"
#include "wdiagram.hh"
#include "dstyle.hh"
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
#define PDF_OPT 309
#define PDF_SCALE_OPT 310
#define PDF_MULTIPAGE_OPT 311
#define LIST_OPT 312
#define NO_LIST_OPT 313
#define TOOLBAR_OPT 314
#define NO_TOOLBAR_OPT 315
#define GEOMETRY_OPT 316
#define RUN_OPT 317

static const Clp_Option options[] = {
    { "version", 0, VERSION_OPT, 0, 0 },
    { "style", 's', STYLE_OPT, Clp_ValString, Clp_Negate|Clp_PreferredMatch },
    { "style-expr", 'S', STYLE_EXPR_OPT, Clp_ValString, 0 },
    { "list", 0, LIST_OPT, 0, Clp_Negate },
    { 0, 'L', NO_LIST_OPT, 0, Clp_Negate },
    { "toolbar", 0, TOOLBAR_OPT, 0, Clp_Negate },
    { 0, 'T', NO_TOOLBAR_OPT, 0, Clp_Negate },
    { "geometry", 'g', GEOMETRY_OPT, Clp_ValString, 0 },
    { "file", 'f', FILE_OPT, Clp_ValString, 0 },
    { "expression", 'e', EXPRESSION_OPT, Clp_ValString, 0 },
    { "port", 'p', PORT_OPT, Clp_ValString, 0 },
    { "kernel", 'k', KERNEL_OPT, 0, 0 },
    { "clickpath", 'C', CLICKPATH_OPT, Clp_ValString, 0 },
    { "pdf", 0, PDF_OPT, Clp_ValString, Clp_Optional },
    { "pdf-scale", 0, PDF_SCALE_OPT, Clp_ValDouble, 0 },
    { "pdf-multipage", 0, PDF_MULTIPAGE_OPT, 0, Clp_Negate },
    { "run", 'r', RUN_OPT, 0, Clp_Negate },
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
  -r, --run                    Run configuration in a Click driver.\n\
  -p, --port [HOST:]PORT       Connect to HOST:PORT for configuration.\n\
  -k, --kernel                 Read configuration from kernel.\n\
  -s, --style FILE             Add CCSS style information from FILE.\n\
  -S, --style-expr STYLE       Add STYLE as CCSS style information.\n\
      --pdf[=FILE]             Output diagram to FILE (default stdout).\n\
      --pdf-multipage          Output diagram on multiple letter pages.\n\
      --pdf-scale=SCALE        Scale output diagram by SCALE (default 1).\n\
  -T, --no-toolbar             Hide toolbar on startup.\n\
  -L, --no-list                Hide element list on startup.\n\
  -C, --clickpath PATH         Use PATH for CLICKPATH.\n\
      --help                   Print this message and exit.\n\
  -v, --version                Print version number and exit.\n\
\n\
Report bugs to <click@librelist.com>.\n", program_name);
}

enum {
    wt_mask = 7, wt_expr = 1, wt_port = 2, wt_file = 3, wt_kernel = 4,
    wt_run = 8
};

int
main(int argc, char *argv[])
{
    // disable bug-buddy
    setenv("GNOME_DISABLE_CRASH_DIALOG", "1", 0);
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
    bool have_gui = gtk_init_check(&argc, &argv);
    add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");

    // read command line arguments
    Clp_Parser *clp =
	Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
    program_name = Clp_ProgramName(clp);

    String css_text;
    Vector<String> wfiles;
    Vector<int> wtypes;
    int runmask = 0;
    bool do_pdf = false;
    String pdf_file = "-";
    double pdf_scale = 2.5;
    bool pdf_multipage = false;
    bool show_toolbar = true, show_list = true;
    gint width = -1, height = -1;

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
		css_text += clicky::dcss_set::expand_imports(file_string(clp->vstr, ErrorHandler::default_handler()), clp->vstr, ErrorHandler::default_handler());
	    break;

	  case STYLE_EXPR_OPT:
	    css_text += clicky::dcss_set::expand_imports(clp->vstr, "<style>", ErrorHandler::default_handler());
	    break;

	case RUN_OPT:
	    runmask = clp->negated ? 0 : wt_run;
	    break;

	case EXPRESSION_OPT:
	    wfiles.push_back(clp->vstr);
	    wtypes.push_back(wt_expr | runmask);
	    break;

	case PORT_OPT:
	    if (strchr(clp->vstr, ':'))
		wfiles.push_back(clp->vstr);
	    else
		wfiles.push_back("localhost:" + String(clp->vstr));
	    wtypes.push_back(wt_port);
	    break;

	  case FILE_OPT:
	    wfiles.push_back(clp->vstr);
	    wtypes.push_back(wt_file | runmask);
	    break;

	  case KERNEL_OPT:
	    wfiles.push_back("<kernel>");
	    wtypes.push_back(wt_kernel);
	    break;

	  case PDF_OPT:
	    do_pdf = true;
	    pdf_file = clp->vstr;
	    break;

	  case PDF_SCALE_OPT:
	    pdf_scale = 2.5 / clp->val.d;
	    break;

	  case PDF_MULTIPAGE_OPT:
	    pdf_multipage = !clp->negated;
	    break;

	case LIST_OPT:
	    show_list = !clp->negated;
	    break;

	case NO_LIST_OPT:
	    show_list = clp->negated;
	    break;

	case TOOLBAR_OPT:
	    show_toolbar = !clp->negated;
	    break;

	case NO_TOOLBAR_OPT:
	    show_toolbar = clp->negated;
	    break;

	case GEOMETRY_OPT: {
	    const char *s = clp->vstr, *end = s + strlen(s);
	    if ((s = cp_integer(s, end, 10, &width)) != clp->vstr
		&& s + 1 < end && *s == 'x'
		&& cp_integer(s + 1, end, 10, &height) == end)
		break;
	    else {
		usage();
		exit(1);
	    }
	}

	  case HELP_OPT:
	    usage();
	    exit(0);
	    break;

	  case Clp_NotOption:
	    wfiles.push_back(clp->vstr);
	    wtypes.push_back(wt_port | runmask);
	    break;

	case Clp_BadOption:
	    usage();
	    exit(1);
	    break;

	  case Clp_Done:
	    goto done;

	}
    }

  done:
    // check exit conditions
    if (!do_pdf && !have_gui) {
	fprintf(stderr, "can't initialize GUI\n");
	exit(1);
    }
    if (do_pdf && wfiles.size() != 1) {
	fprintf(stderr, wfiles.size() ? "too many files\n" : "file missing\n");
	exit(1);
    }

    // create styles
    clicky::dcss_set *ccss = clicky::dcss_set::default_set("screen");
    if (css_text) {
	ccss = new clicky::dcss_set(ccss);
	ccss->parse(css_text);
    }

    // create GUIs
    if (wfiles.size() == 0) {
	clicky::wmain *rw = new clicky::wmain(show_toolbar, show_list, ccss, width, height);
	rw->show();
    }

    int colon;
    uint16_t port;
    clicky::crouter *cr = 0;
    clicky::wmain *wm = 0;

    for (int i = 0; i < wfiles.size(); i++) {
	if (!do_pdf)
	    cr = wm = new clicky::wmain(show_toolbar, show_list, ccss, width, height);
	else {
	    cr = new clicky::tmain(ccss);
	    wm = 0;
	}

	GatherErrorHandler *gerrh = cr->error_handler();
	int type = wtypes[i] & wt_mask;
	cr->set_landmark(type == wt_expr ? "config" : wfiles[i]);

	if (type == wt_expr)
	    cr->set_config(wfiles[i], true);
	else if (type == wt_port
		 && (colon = wfiles[i].find_right(':')) >= 0
		 && cp_tcpudp_port(wfiles[i].substring(colon + 1), IP_PROTO_TCP, &port)) {
	    IPAddress addr;
	    if (clicky::cp_host_port(wfiles[i].substring(0, colon), wfiles[i].substring(colon + 1), &addr, &port, cr->error_handler())) {
		bool ready = false;
		GIOChannel *channel = clicky::csocket_cdriver::start_connect(addr, port, &ready, gerrh);
		if (gerrh->size())
		    cr->on_error(true, gerrh->message_string(gerrh->begin(), gerrh->end()));
		if (channel)
		    (void) new clicky::csocket_cdriver(cr, channel, ready);
	    }
	} else if (type == wt_kernel) {
	    (void) new clicky::clickfs_cdriver(cr, "/click/");
	} else {
	    String s = file_string(wfiles[i], cr->error_handler());
	    if (compressed_data((const unsigned char *) s.begin(), s.length())
		&& wfiles[i] != "-" && wfiles[i] != "") {
		if (FILE *f = open_uncompress_pipe(wfiles[i], (const unsigned char *) s.begin(), s.length(), cr->error_handler())) {
		    s = file_string(f, cr->error_handler());
		    pclose(f);
		}
	    }
	    if (!s && gerrh->nerrors())
		cr->on_error(true, gerrh->message_string(gerrh->begin(), gerrh->end()));
	    cr->set_config(s, true);
	    if (wm)
		wm->set_save_file(wfiles[i], (bool) s);
	}

	if (!cr->driver() && (wtypes[i] & wt_run))
	    cr->run(gerrh);
	if (wm)
	    wm->show();
    }

    if (do_pdf) {
	if (pdf_multipage)
	    clicky::cdiagram::export_to_file(pdf_file.c_str(), cr, point(612, 792), point(24, 24), pdf_scale, true);
	else
	    clicky::cdiagram::export_to_file(pdf_file.c_str(), cr, point(0, 0), point(24, 24), pdf_scale, false);
	exit(0);
    }

    gtk_main();
    return 0;
}
