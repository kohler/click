#ifndef TOOLUTILS_HH
#define TOOLUTILS_HH
#include <click/userutils.hh>
class Bitvector;
class RouterT;

extern bool ignore_line_directives;
RouterT *read_router_string(const String &text, const String &landmark, ErrorHandler *);
RouterT *read_router_string(String text, const String &landmark, bool, RouterT *, ErrorHandler *);
RouterT *read_router_file(const char *, ErrorHandler *);
RouterT *read_router_file(const char *, bool, ErrorHandler *);
RouterT *read_router(const char *, bool is_expr, ErrorHandler *);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

#endif
