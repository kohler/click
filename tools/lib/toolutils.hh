#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include "string.hh"
#include <stdio.h>
class RouterT;
class ErrorHandler;

String file_string(FILE *, ErrorHandler * = 0);
String file_string(const char *, ErrorHandler * = 0);
RouterT *read_router_file(const char *, ErrorHandler * = 0, RouterT * = 0);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

String unique_tmpnam(const String &, ErrorHandler * = 0);
void remove_file_on_exit(const String &);
String clickpath_find_file(const String &filename, const char *subdir,
			   const String &default_path, ErrorHandler * = 0);
String click_mktmpdir(ErrorHandler * = 0);

#endif
