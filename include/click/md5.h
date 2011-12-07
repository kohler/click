/* -*- related-file-name: "../../lib/md5.cc" -*-
  Copyright (c) 2006-2007 Regents of the University of California
  Altered for Click by Eddie Kohler. */
/*
  Copyright (C) 1999, 2002 Aladdin Enterprises.  All rights reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  L. Peter Deutsch
  ghost@aladdin.com

 */
/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321, whose
  text is available at
	http://www.ietf.org/rfc/rfc1321.txt
  The code is derived from the text of the RFC, including the test suite
  (section A.5) but excluding the rest of Appendix A.  It does not include
  any code or documentation that is identified in the RFC as being
  copyrighted.

  The original and principal author of md5.h is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  2002-04-13 lpd Removed support for non-ANSI compilers; removed
	references to Ghostscript; clarified derivation from RFC 1321;
	now handles byte order either statically or dynamically.
  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5);
	added conditionalization for C++ compilation from Martin
	Purschke <purschke@bnl.gov>.
  1999-05-03 lpd Original version.
 */

#ifndef CLICK_MD5_H
#define CLICK_MD5_H
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/err.h>
# include <linux/crypto.h>
# include <asm/scatterlist.h>
# include <linux/scatterlist.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
typedef struct hash_desc md5_state_t;
#else
typedef struct crypto_tfm *md5_state_t;
#endif

static inline int md5_init(md5_state_t *pms) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
    pms->tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(pms->tfm))
	return PTR_ERR(pms->tfm);
    pms->flags = 0;
    crypto_hash_init(pms);
    return 0;
#else
    *pms = crypto_alloc_tfm("md5", 0);
    if (IS_ERR(*pms))
	return PTR_ERR(*pms);
    crypto_digest_init(*pms);
    return 0;
#endif
}

static inline void md5_reinit(md5_state_t *pms) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
    crypto_hash_init(pms);
#else
    crypto_digest_init(*pms);
#endif
}

static inline void md5_append(md5_state_t *pms, const unsigned char *data, int nbytes) {
    struct scatterlist sg;
    sg_init_one(&sg, const_cast<uint8_t *>(data), nbytes);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
    crypto_hash_update(pms, &sg, nbytes);
#else
    crypto_digest_update(*pms, &sg, 1);
#endif
}

static inline void md5_finish(md5_state_t *pms, unsigned char digest[16]) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
    crypto_hash_final(pms, digest);
#else
    crypto_digest_final(*pms, digest);
#endif
}

static inline void md5_free(md5_state_t *pms) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
    crypto_free_hash(pms->tfm);
#else
    crypto_free_tfm(*pms);
#endif
}

#else

/*
 * This package supports both compile-time and run-time determination of CPU
 * byte order.  If ARCH_IS_BIG_ENDIAN is defined as 0, the code will be
 * compiled to run only on little-endian CPUs; if ARCH_IS_BIG_ENDIAN is
 * defined as non-zero, the code will be compiled to run only on big-endian
 * CPUs; if ARCH_IS_BIG_ENDIAN is not defined, the code will be compiled to
 * run on either big- or little-endian CPUs, but will run slightly less
 * efficiently on either one than if ARCH_IS_BIG_ENDIAN is defined.
 */

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef uint32_t md5_word_t; /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct md5_state_s {
    md5_word_t count[2];	/* message length in bits, lsw first */
    md5_word_t abcd[4];		/* digest buffer */
    md5_byte_t buf[64];		/* accumulate block */
} md5_state_t;

#ifdef __cplusplus
extern "C"
{
#endif

/* Initialize the algorithm. */
int md5_init(md5_state_t *pms);

/* Re-initialize the algorithm after a prior md5_init(). */
static inline void md5_reinit(md5_state_t *pms) {
    md5_init(pms);
}

/* Append a string to the message. */
void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);

/* Finish the message and return the digest. */
#define MD5_DIGEST_SIZE			16
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);

/* Finish the message and return the digest in ASCII.  DOES NOT write a
   terminating NUL character.
   If 'allow_at == 0', the digest uses characters [A-Za-z0-9_], and has
   length between MD5_TEXT_DIGEST_SIZE and MD5_TEXT_DIGEST_MAX_SIZE.
   If 'allow_at != 0', the digest uses characters [A-Za-z0-9_@], and has
   length of exactly MD5_TEXT_DIGEST_SIZE.
   Returns the number of characters written.  Again, this will NOT include
   a terminating NUL. */
#define MD5_TEXT_DIGEST_SIZE		22	/* min len */
#define MD5_TEXT_DIGEST_MAX_SIZE	26	/* max len if !allow_at */
int md5_finish_text(md5_state_t *pms, char *text_digest, int allow_at);

#define md5_free(pms)		/* do nothing */

#ifdef __cplusplus
}
#endif

#endif
#endif

