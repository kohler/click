#ifndef USERUTILS_HH
#define USERUTILS_HH
#include "archive.hh"
#include <stdio.h>
class ErrorHandler;

String file_string(FILE *, ErrorHandler * = 0);
String file_string(const char *, ErrorHandler * = 0);

String unique_tmpnam(const String &, ErrorHandler * = 0);
void remove_file_on_exit(const String &);
String clickpath_find_file(const String &filename, const char *subdir,
			   String default_path, ErrorHandler * = 0);
String click_mktmpdir(ErrorHandler * = 0);

void parse_tabbed_lines(const String &, bool allow_spaces, int, ...);

ArchiveElement init_archive_element(const String &, int);

#endif
