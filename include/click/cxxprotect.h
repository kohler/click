#ifdef __cplusplus
#define new		xxx_new
#define this		xxx_this
#define delete		xxx_delete
#define class		xxx_class
#define virtual		xxx_virtual
#define typename	xxx_typename
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
