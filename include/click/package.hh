#ifndef CLICK_PACKAGE_HH
#define CLICK_PACKAGE_HH
class Element;
extern "C" {

#if defined(CLICK_LINUXMODULE) && defined(CLICK_PACKAGE)
# define __NO_VERSION__
# define new linux_new
# include <linux/module.h>
# undef new
#else
# define MOD_INC_USE_COUNT
# define MOD_DEC_USE_COUNT
# define MOD_IN_USE		0
#endif

void click_provide(const char *);
void click_unprovide(const char *);
int click_add_element_type(const char *, Element *);
void click_remove_element_type(int);

}
#endif
