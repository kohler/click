// -*- c-basic-offset: 4; related-file-name: "../../lib/driver.cc" -*-
#ifndef CLICK_DRIVER_HH
#define CLICK_DRIVER_HH
#include <click/package.hh>

#define CLICK_DEFAULT_PROVIDES	/* nada */

#if CLICK_USERLEVEL
CLICK_DECLS
class Router;
class Master;
class ErrorHandler;
class Lexer;
struct ArchiveElement;

void click_static_initialize();
void click_static_cleanup();

Lexer *click_lexer();
Router *click_read_router(String filename, bool is_expr, ErrorHandler * = 0, bool initialize = true, Master * = 0);

String click_compile_archive_file(const Vector<ArchiveElement> &ar,
		const ArchiveElement *ae,
		String package, const String &target, int quiet,
		bool &tmpdir_populated, ErrorHandler *errh);

CLICK_ENDDECLS
#elif CLICK_TOOL
CLICK_DECLS
class ErrorHandler;
struct ArchiveElement;

void click_static_initialize();

String click_compile_archive_file(const Vector<ArchiveElement> &archive,
		const ArchiveElement *ae,
		String package, const String &target, int quiet,
		bool &tmpdir_populated, ErrorHandler *errh);

CLICK_ENDDECLS
#endif

#endif
