#ifndef USERUTILS_HH
#define USERUTILS_HH
#include <click/archive.hh>
#ifdef CLICK_USERLEVEL
#include <stdio.h>
#endif
class ErrorHandler;

bool glob_match(const String &string, const String &pattern);

#ifdef CLICK_USERLEVEL
String file_string(FILE *, ErrorHandler * = 0);
String file_string(const char *, ErrorHandler * = 0);
#endif

String unique_tmpnam(const String &, ErrorHandler * = 0);
void remove_file_on_exit(const String &);
bool path_allows_default_path(String path);
String click_mktmpdir(ErrorHandler * = 0);

const char *clickpath();
void set_clickpath(const char *);

String clickpath_find_file(const String &filename, const char *subdir,
			   String default_path, ErrorHandler * = 0);

void parse_tabbed_lines(const String &, Vector<String> *, ...);

ArchiveElement init_archive_element(const String &, int);

#if HAVE_DYNAMIC_LINKING
int clickdl_load_package(String, ErrorHandler *);
void clickdl_load_requirement(String, const Vector<ArchiveElement> *archive, ErrorHandler *);
#endif

#endif
