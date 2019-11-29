#include <stddef.h>
#include <errno.h>
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <endian.h>

#if !defined(le32toh) || !defined(le16toh)
#    include <byteswap.h>
#    define le32toh(x) bswap_32(x)
#    define le16toh(x) bswap_16(x)
#endif

#define le16_to_cpu		le16toh
#define le32_to_cpu		le32toh
#define get_unaligned(p)					\
({								\
	struct packed_dummy_struct {				\
		typeof(*(p)) __val;				\
	} __attribute__((packed)) *__ptr = (void *) (p);	\
								\
	__ptr->__val;						\
})
#define get_unaligned_le16(p)	le16_to_cpu(get_unaligned((uint16_t *)(p)))
#define get_unaligned_le32(p)	le32_to_cpu(get_unaligned((uint32_t *)(p)))
