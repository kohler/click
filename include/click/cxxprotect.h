#ifdef __cplusplus
# define new		linux_new
# define this		linux_this
# define delete		linux_delete
# define class		linux_class
# define virtual	linux_virtual
# define typename	linux_typename
# define protected	linux_protected
# define public		linux_public
# define namespace	linux_namespace
# define false		linux_false
# define true		linux_true
#endif

#ifndef CLICK_CXX_PROTECT
# ifdef __cplusplus
#  define CLICK_CXX_PROTECT	extern "C" {
#  define CLICK_CXX_UNPROTECT	}
# else
#  define CLICK_CXX_PROTECT	/* nothing */
#  define CLICK_CXX_UNPROTECT	/* nothing */
# endif
#endif

#define CLICK_CXX_PROTECTED 1
