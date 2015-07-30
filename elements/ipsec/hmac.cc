/* crypto/hmac/hmac.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <click/config.h>
#include "hmac.hh"
CLICK_DECLS

void OpenSSLDie(void)
	{
	   click_chatter("HMAC computation internal error, assertion failed\n");
	}


void HMAC_Init_ex(HMAC_CTX *ctx, const void *key, int len)
	{
	int i,j,reset=0;
	unsigned char pad[HMAC_MAX_MD_CBLOCK];

	if (key != NULL)
		{
		reset=1;
		j=SHA_CBLOCK;
		OPENSSL_assert(j <= (int)sizeof(ctx->key));
		if (j < len)
			{
			SHA1_init(&ctx->md_ctx);
			SHA1_update(&ctx->md_ctx,(unsigned char*)key,len);
			SHA1_final(ctx->key,&(ctx->md_ctx));
			}
		else
			{
			OPENSSL_assert(len>=0 && len<=(int)sizeof(ctx->key));
			memcpy(ctx->key,key,len);
			ctx->key_length=len;
			}
		if(ctx->key_length != HMAC_MAX_MD_CBLOCK)
			memset(&ctx->key[ctx->key_length], 0,
				HMAC_MAX_MD_CBLOCK - ctx->key_length);
		}

	if (reset)
		{
		for (i=0; i<HMAC_MAX_MD_CBLOCK; i++)
			pad[i]=0x36^ctx->key[i];
		SHA1_init(&ctx->i_ctx);
		SHA1_update(&ctx->i_ctx,pad,SHA_CBLOCK);

		for (i=0; i<HMAC_MAX_MD_CBLOCK; i++)
			pad[i]=0x5c^ctx->key[i];
		SHA1_init(&ctx->o_ctx);
		SHA1_update(&ctx->o_ctx,pad,SHA_CBLOCK);
		}
	memcpy((void *)&ctx->md_ctx,(void*)&ctx->i_ctx,sizeof(SHA1_ctx));
	}

void HMAC_Init(HMAC_CTX *ctx, const void *key, int len)
	{
	if(key /*&& md*/)
	    HMAC_CTX_init(ctx);
	    HMAC_Init_ex(ctx,key,len);
	}

void HMAC_Update(HMAC_CTX *ctx, unsigned char *data, size_t len)
	{
	   SHA1_update(&ctx->md_ctx,data,len);
	}

void HMAC_Final(HMAC_CTX *ctx, unsigned char *md, unsigned int *len)
	{
	 int j;
	 unsigned char buf[EVP_MAX_MD_SIZE];

	 j=SHA_BLOCK;
	 SHA1_final(buf,&ctx->md_ctx);
	 memcpy((void *)&ctx->md_ctx,(void*)&ctx->o_ctx,sizeof(SHA1_ctx));
	 SHA1_update(&ctx->md_ctx,buf,(unsigned long)*len);
	 SHA1_final(md,&ctx->md_ctx);
	}

void HMAC_CTX_init(HMAC_CTX *ctx)
	{
		SHA1_init(&ctx->i_ctx);
		SHA1_init(&ctx->o_ctx);
		SHA1_init(&ctx->md_ctx);
	}

void HMAC_CTX_cleanup(HMAC_CTX *ctx)
	{
	     memset(ctx,0,sizeof *ctx);
	}

unsigned char *HMAC(void *key, int key_len,unsigned char *d, size_t n, unsigned char *md,unsigned int *md_len)
	{
	HMAC_CTX c;
	static unsigned char m[EVP_MAX_MD_SIZE];

	if (md == NULL) md=m;
		HMAC_CTX_init(&c);
		HMAC_Init(&c,key,key_len);
		HMAC_Update(&c,d,n);
		HMAC_Final(&c,md,md_len);
		HMAC_CTX_cleanup(&c);
		return(md);
	}

CLICK_ENDDECLS
