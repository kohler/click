#ifndef CLICK_PACKAGE_HH
#define CLICK_PACKAGE_HH
class Element;
extern "C" {

#ifdef CLICK_LINUXMODULE
# define __NO_VERSION__
# define new linux_new
# include <linux/module.h>
# undef new
#else
# define MOD_INC_USE_COUNT
# define MOD_DEC_USE_COUNT
#endif

void click_provide(const char *);
void click_unprovide(const char *);
int click_add_element_type(const char *, Element *);
void click_remove_element_type(int);

}
#endif
