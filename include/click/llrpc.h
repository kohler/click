#ifndef CLICK_LLRPC_H
#define CLICK_LLRPC_H
#include <errno.h>

// Click low-level RPC interface

#define CLICK_LLRPC_IPRATEMON_LEVEL_FWD_AVG		0xC400C701
#define CLICK_LLRPC_IPRATEMON_LEVEL_REV_AVG		0xC400C702
#define CLICK_LLRPC_IPRATEMON_FWD_N_REV_AVG		0xC400C703
#define CLICK_LLRPC_IPRATEMON_SET_ANNO_LEVEL		0xC400C704
#define CLICK_LLRPC_TCPCOUNTER_GET_RATES		0xC400C705
#define CLICK_LLRPC_KUTUNNEL_GET_PACKET			0xC400C706
#define CLICK_LLRPC_IPREWRITER_MAP_TCP			0xC400C707
#define CLICK_LLRPC_IPREWRITER_MAP_UDP			0xC400C708
#define CLICK_LLRPC_MARK_TIMESTAMP			0xC400C709
#define CLICK_LLRPC_GET_RATE				0xC400C70A
#define CLICK_LLRPC_GET_RATES				0xC400C70B
#define CLICK_LLRPC_GET_COUNT				0xC400C70C
#define CLICK_LLRPC_GET_COUNTS				0xC400C70D

#define CLICK_LLRPC_COUNTS_SIZE 8
struct click_llrpc_counts_st {
  unsigned n;
  unsigned keys[CLICK_LLRPC_COUNTS_SIZE];
  unsigned values[CLICK_LLRPC_COUNTS_SIZE];
};


// data manipulation

#if CLICK_USERLEVEL

# define CLICK_LLRPC_GET_DATA(local, remote, size) ((void)(local), (void)(remote), (void)(size), -EFAULT)
# define CLICK_LLRPC_PUT_DATA(remote, local, size) ((void)(local), (void)(remote), (void)(size), -EFAULT)
# define CLICK_LLRPC_GET(local_obj, remote_addr) ((void)(local_obj), (void)(remote_addr), -EFAULT)
# define CLICK_LLRPC_PUT(remote_addr, local_obj) ((void)(local_obj), (void)(remote_addr), -EFAULT)

#elif CLICK_LINUXMODULE

# ifdef __cplusplus
#  define __CLICK_LLRPC_CAST(x) reinterpret_cast< x >
extern "C" {
#  include <asm/uaccess.h>
}
# else
#  define __CLICK_LLRPC_CAST(x) (x)
#  include <asm/uaccess.h>
# endif

# define __CLICK_LLRPC_GENERIC_GET_DATA(local, remote, size) \
		(copy_from_user(local, remote, size) > 0 ? -EFAULT : 0)
# define __CLICK_LLRPC_CONSTANT_GET_DATA(local, remote, size) \
		(size == 1 ? get_user(*__CLICK_LLRPC_CAST(unsigned char *)(local), __CLICK_LLRPC_CAST(unsigned char *)(remote)) \
		 : (size == 2 ? get_user(*__CLICK_LLRPC_CAST(unsigned short *)(local), __CLICK_LLRPC_CAST(unsigned short *)(remote)) \
		    : (size == 4 ? get_user(*__CLICK_LLRPC_CAST(unsigned *)(local), __CLICK_LLRPC_CAST(unsigned *)(remote)) \
		       : __CLICK_LLRPC_GENERIC_GET_DATA(local, remote, size))))

# define __CLICK_LLRPC_GENERIC_PUT_DATA(remote, local, size) \
		(copy_to_user(remote, local, size) > 0 ? -EFAULT : 0)
# define __CLICK_LLRPC_CONSTANT_PUT_DATA(remote, local, size) \
		(size == 1 ? put_user(*__CLICK_LLRPC_CAST(const unsigned char *)(local), __CLICK_LLRPC_CAST(unsigned char *)(remote)) \
		 : (size == 2 ? put_user(*__CLICK_LLRPC_CAST(const unsigned short *)(local), __CLICK_LLRPC_CAST(unsigned short *)(remote)) \
		    : (size == 4 ? put_user(*__CLICK_LLRPC_CAST(const unsigned *)(local), __CLICK_LLRPC_CAST(unsigned *)(remote)) \
		       : __CLICK_LLRPC_GENERIC_PUT_DATA(remote, local, size))))

# define CLICK_LLRPC_GET_DATA(local, remote, size) \
		(__builtin_constant_p(size) && size <= 4 \
		 ? __CLICK_LLRPC_CONSTANT_GET_DATA(local, remote, size) \
		 : __CLICK_LLRPC_GENERIC_GET_DATA(local, remote, size))
# define CLICK_LLRPC_PUT_DATA(remote, local, size) \
		(__builtin_constant_p(size) && size <= 4 \
		 ? __CLICK_LLRPC_CONSTANT_PUT_DATA(remote, local, size) \
		 : __CLICK_LLRPC_GENERIC_PUT_DATA(remote, local, size))

# define CLICK_LLRPC_GET(local_obj, remote_addr) \
		get_user((local_obj), (remote_addr))
# define CLICK_LLRPC_PUT(remote_addr, local_obj) \
		put_user((local_obj), (remote_addr))

#endif

#endif
