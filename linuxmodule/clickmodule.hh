#ifndef CLICKMODULE_HH
#define CLICKMODULE_HH
class Element;

extern "C" {
#define __NO_VERSION__
#define new linux_new
#include <linux/module.h>
#undef new

void click_provide(const char *);
void click_unprovide(const char *);
int click_add_element_type(const char *, Element *);
void click_remove_element_type(int);

}

#endif
