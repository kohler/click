#ifdef __cplusplus
#define new		xxx_new
#define this		xxx_this
#define delete		xxx_delete
#define class		xxx_class
#define virtual		xxx_virtual
#define typename	xxx_typename
#define private		xxx_private
#define protected	xxx_protected
#define public		xxx_public
#define namespace	xxx_namespace
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
