#ifndef USERUTILS_HH
#define USERUTILS_HH
#include <click/archive.hh>
#include <stdio.h>
class ErrorHandler;

bool glob_match(const String &string, const String &pattern);

String file_string(FILE *, ErrorHandler * = 0);
String file_string(const char *, ErrorHandler * = 0);

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

#endif
