#ifndef CLICK_LLRPC_HH
#define CLICK_LLRPC_HH
#include <errno.h>

// Click low-level RPC interface

#define CLICK_LLRPC_IPRATEMON_LEVEL_FWD_AVG	0xC400C701
#define CLICK_LLRPC_IPRATEMON_LEVEL_REV_AVG	0xC400C702
#define CLICK_LLRPC_IPRATEMON_FWD_N_REV_AVG	0xC400C703


// data manipulation

#if CLICK_USERLEVEL

# define CLICK_LLRPC_GET_DATA(local, remote, size) -EFAULT
# define CLICK_LLRPC_PUT_DATA(remote, local, size) -EFAULT
# define CLICK_LLRPC_GET(local_obj, remote_addr) -EFAULT
# define CLICK_LLRPC_PUT(remote_addr, local_obj) -EFAULT

#elif CLICK_LINUXMODULE
# include <asm/uaccess.h>

# define __CLICK_LLRPC_GENERIC_GET_DATA(local, remote, size) \
		(copy_from_user(local, remote, size) > 0 ? -EFAULT : 0)
# define __CLICK_LLRPC_CONSTANT_GET_DATA(local, remote, size) \
		(size == 1 ? get_user(*reinterpret_cast<unsigned char *>(local), reinterpret_cast<unsigned char *>(remote)) \
		 : (size == 2 ? get_user(*reinterpret_cast<unsigned short *>(local), reinterpret_cast<unsigned short *>(remote)) \
		    : (size == 4 ? get_user(*reinterpret_cast<unsigned *>(local), reinterpret_cast<unsigned *>(remote)) \
		       : __CLICK_LLRPC_GENERIC_GET_DATA(local, remote, size))))
		 
# define __CLICK_LLRPC_GENERIC_PUT_DATA(remote, local, size) \
		(copy_to_user(remote, local, size) > 0 ? -EFAULT : 0)
# define __CLICK_LLRPC_CONSTANT_PUT_DATA(remote, local, size) \
		(size == 1 ? put_user(*reinterpret_cast<unsigned char *>(local), reinterpret_cast<unsigned char *>(remote)) \
		 : (size == 2 ? put_user(*reinterpret_cast<unsigned short *>(local), reinterpret_cast<unsigned short *>(remote)) \
		    : (size == 4 ? put_user(*reinterpret_cast<unsigned *>(local), reinterpret_cast<unsigned *>(remote)) \
		       : __CLICK_LLRPC_GENERIC_PUT_DATA(local, remote, size))))
		 
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
