#ifndef CLICK_PACKAGE_HH
#define CLICK_PACKAGE_HH
#include <click/vector.hh>
#include <click/string.hh>
CLICK_DECLS
#ifndef CLICK_TOOL
class Element;
#endif
CLICK_ENDDECLS

extern "C" {

void click_provide(const char *);
void click_unprovide(const char *);
bool click_has_provision(const char *);
void click_public_packages(CLICK_NAME(Vector)<CLICK_NAME(String)> &);

#if CLICK_LINUXMODULE
int click_add_element_type(const char *element_name, CLICK_NAME(Element) *(*factory)(uintptr_t thunk), uintptr_t thunk, struct module *);
int click_add_element_type_stable(const char *element_name, CLICK_NAME(Element) *(*factory)(uintptr_t thunk), uintptr_t thunk, struct module *);
void click_remove_element_type(int);
#elif !CLICK_TOOL
int click_add_element_type(const char *element_name, CLICK_NAME(Element) *(*factory)(uintptr_t thunk), uintptr_t thunk);
int click_add_element_type_stable(const char *element_name, CLICK_NAME(Element) *(*factory)(uintptr_t thunk), uintptr_t thunk);
void click_remove_element_type(int);
#endif

}

#endif
