#ifndef CLICK_TOOLUTILS_HH
#define CLICK_TOOLUTILS_HH
#include <click/userutils.hh>
class RouterT;
class VariableEnvironment;

extern VariableEnvironment global_scope;
extern bool ignore_line_directives;

int click_maybe_define(const char *arg, ErrorHandler *errh);
RouterT *read_router_string(const String &text, const String &landmark, ErrorHandler *);
RouterT *read_router_string(String text, const String &landmark, bool, RouterT *, ErrorHandler *);
RouterT *read_router_file(const char *filename, ErrorHandler *);
RouterT *read_router_file(const char *filename, bool empty_ok, ErrorHandler *);
RouterT *read_router(const String &, bool is_expr, ErrorHandler *);
void write_router_file(RouterT *, FILE *, ErrorHandler * = 0);
int write_router_file(RouterT *, const char *, ErrorHandler * = 0);

String xml_quote(const String &);

#endif
