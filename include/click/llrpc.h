#ifndef CLICK_LLRPC_H
#define CLICK_LLRPC_H
#if CLICK_LINUXMODULE
# include <linux/errno.h>
#else
# include <errno.h>
#endif

/* Click low-level RPC interface */

/* We want consistent ioctl numbers across platforms, so we choose FreeBSD's
   numbering system (Linux doesn't seem to apply semantics to its numbering
   system, which switches IOC_OUT and IOC_IN). */
#define _CLICK_IOC_VOID		0x20000000
#define _CLICK_IOC_OUT		0x40000000
#define _CLICK_IOC_IN		0x80000000

#define _CLICK_IOX(d, g, n, sz)	((d) | ((sz) << 16) | ((g) << 8) | (n))
#define _CLICK_IO(n)		_CLICK_IOX(_CLICK_IOC_VOID, 0xC7, (n), 0)
#define _CLICK_IOR(n, sz)	_CLICK_IOX(_CLICK_IOC_OUT, 0xC7, (n), (sz))
#define _CLICK_IOW(n, sz)	_CLICK_IOX(_CLICK_IOC_IN, 0xC7, (n), (sz))
#define _CLICK_IOWR(n, sz)	_CLICK_IOX(_CLICK_IOC_IN|_CLICK_IOC_OUT, 0xC7, (n), (sz))

#define CLICK_LLRPC_GET_RATE			_CLICK_IOWR(0, 4)
#define CLICK_LLRPC_GET_RATES			_CLICK_IO(1)
#define CLICK_LLRPC_GET_COUNT			_CLICK_IOWR(2, 4)
#define CLICK_LLRPC_GET_COUNTS			_CLICK_IO(3)
#define CLICK_LLRPC_GET_SWITCH			_CLICK_IOR(4, 4)
#define CLICK_LLRPC_SET_SWITCH			_CLICK_IOW(5, 4)
#define CLICK_LLRPC_MAP_IPADDRESS		_CLICK_IOWR(6, 4)
#define CLICK_LLRPC_IPREWRITER_MAP_TCP		_CLICK_IOWR(7, 12)
#define CLICK_LLRPC_IPREWRITER_MAP_UDP		_CLICK_IOWR(8, 12)
#define CLICK_LLRPC_IPRATEMON_LEVEL_FWD_AVG	_CLICK_IO(9)
#define CLICK_LLRPC_IPRATEMON_LEVEL_REV_AVG	_CLICK_IO(10)
#define CLICK_LLRPC_IPRATEMON_FWD_N_REV_AVG	_CLICK_IO(11)
#define CLICK_LLRPC_IPRATEMON_SET_ANNO_LEVEL	_CLICK_IO(12)

#define CLICK_LLRPC_COUNTS_SIZE 8
struct click_llrpc_counts_st {
  uint32_t n;
  uint32_t keys[CLICK_LLRPC_COUNTS_SIZE];
  uint32_t values[CLICK_LLRPC_COUNTS_SIZE];
};


/* data manipulation */

#if CLICK_USERLEVEL

# define CLICK_LLRPC_GET_DATA(local, remote, size) (memcpy(local, remote, size), 0)
# define CLICK_LLRPC_PUT_DATA(remote, local, size) (memcpy(remote, local, size), 0)
# define CLICK_LLRPC_GET(local_obj, remote_addr) (memcpy(&(local_obj), remote_addr, sizeof(local_obj)), 0)
# define CLICK_LLRPC_PUT(remote_addr, local_obj) (memcpy(remote_addr, &(local_obj), sizeof(local_obj)), 0)

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

#elif CLICK_BSDMODULE

/*
 * XXX  LLRPC isn't implemented for BSD yet.
 */

# define CLICK_LLRPC_GET_DATA(local, remote, size) ((void)(local), (void)(remote), (void)(size), -EFAULT)
# define CLICK_LLRPC_PUT_DATA(remote, local, size) ((void)(local), (void)(remote), (void)(size), -EFAULT)
# define CLICK_LLRPC_GET(local_obj, remote_addr) ((void)(local_obj), (void)(remote_addr), -EFAULT)
# define CLICK_LLRPC_PUT(remote_addr, local_obj) ((void)(local_obj), (void)(remote_addr), -EFAULT)

#endif

/* sanity checks */
#ifdef __FreeBSD__
# include <sys/ioctl.h>
# if _CLICK_IOC_VOID != IOC_VOID || _CLICK_IOC_OUT != IOC_OUT || _CLICK_IOC_IN != IOC_IN
#  error "bad _CLICK_IOC constants"
# endif
#endif
#endif
