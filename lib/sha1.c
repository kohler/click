/*
 * Implementation of Federal Information Processing Standards Publication
 * FIPS 180-1 (17 Apr 1995) which supersedes FIPS 180 (11 May 1993)
 *
 * Speed hack version optimised for speed (see also reference version)
 * uses macros so you need to recompile, not just relink
 *
 * Adam Back <aba@dcs.ex.ac.uk>
 *
 */

/* define VERBOSE to get output as in fip180-1.txt */
#if defined( VERBOSE )
    #include <stdio.h>
#endif
#include <click/sha1.h>


void swap_endian16( void* data, int len )
{
    unsigned short tmp16;
    unsigned char* tmp16_as_bytes = (unsigned char*) &tmp16;
    unsigned short* data_as_word16s = (unsigned short*) data;
    unsigned char* data_as_bytes;
    int i;
    
    for ( i = 0; i < len; i++ )
    {
        tmp16 = data_as_word16s[ i ];
        data_as_bytes = (unsigned char*) &( data_as_word16s[ i ] );
        
        data_as_bytes[ 0 ] = tmp16_as_bytes[ 1 ];
        data_as_bytes[ 1 ] = tmp16_as_bytes[ 0 ];
    }
}

void swap_endian32( void* data, int len )
{
    unsigned int tmp32;
    unsigned char* tmp32_as_bytes = (unsigned char*) &tmp32;
    unsigned int* data_as_word32s = (unsigned int*) data;
    unsigned char* data_as_bytes;
    int i;
    
    for ( i = 0; i < len; i++ )
    {
        tmp32 = data_as_word32s[ i ];
        data_as_bytes = (unsigned char*) &( data_as_word32s[ i ] );
        
        data_as_bytes[ 0 ] = tmp32_as_bytes[ 3 ];
        data_as_bytes[ 1 ] = tmp32_as_bytes[ 2 ];
        data_as_bytes[ 2 ] = tmp32_as_bytes[ 1 ];
        data_as_bytes[ 3 ] = tmp32_as_bytes[ 0 ];
    }
}



#define min( x, y ) ( ( x ) < ( y ) ? ( x ) : ( y ) )

/********************* function used for rounds 0..19 ***********/

/* #define F1( B, C, D ) ( ( (B) & (C) ) | ( ~(B) & (D) ) ) */

/* equivalent, one less operation: */
#define F1( B, C, D ) ( (D) ^ ( (B) & ( (C) ^ (D) ) ) )


/********************* function used for rounds 20..39 ***********/

#define F2( B, C, D ) ( (B) ^ (C) ^ (D) )

/********************* function used for rounds 40..59 ***********/

/* #define F3( B, C, D ) ( (B) & (C) ) | ( (C) & (D) ) | ( (C) & (D) ) */

/* equivalent, one less operation */

#define F3( B, C, D ) ( ( (B) & ( (C) | (D) )) | ( (C) & (D) ) )

/********************* function used for rounds 60..79 ***********/

#define F4( B, C, D ) ( (B) ^ (C) ^ (D) )

#define K1 0x5A827999  /* constant used for rounds 0..19 */
#define K2 0x6ED9EBA1  /* constant used for rounds 20..39 */
#define K3 0x8F1BBCDC  /* constant used for rounds 40..59 */
#define K4 0xCA62C1D6  /* constant used for rounds 60..79 */

/* magic constants */

#define H0 0x67452301
#define H1 0xEFCDAB89
#define H2 0x98BADCFE
#define H3 0x10325476
#define H4 0xC3D2E1F0

unsigned SHA1_IV[ 5 ] = { H0, H1, H2, H3, H4 };

/* rotate X n bits left   ( X <<< n ) */

#define S(n, X) ( ( (X) << (n) ) | ( (X) >> ( 32 - (n) ) ) )

void SHA1_transform(  unsigned H[ SHA1_DIGEST_WORDS ], 
		      const unsigned char M[ SHA1_INPUT_BYTES ] )
{
    int t;
    unsigned A = H[ 0 ];
    unsigned B = H[ 1 ];
    unsigned C = H[ 2 ];
    unsigned D = H[ 3 ];
    unsigned E = H[ 4 ];
    unsigned W[ 16 ];

    memcpy( W, M, SHA1_INPUT_BYTES );

/* Use method B from FIPS-180 (see fip-180.txt) where the use of
   temporary array W of 80 unsigned is avoided by working in a circular
   buffer of size 16 unsigned.  

*/

/********************* define some macros *********************/

/* Wc = access W as 16 word circular buffer */

#define Wc( t ) ( W[ (t) & 15 ] )

/* Calculate access to W array on the fly for entries 16 .. 79 */

#define Wf( t ) \
    ( Wc( t ) = S( 1, Wc( t ) ^ Wc( t - 14 ) ^ Wc( t - 8 ) ^ Wc( t - 3 ) ) )

/* Calculate access to W virtual array calculating access to W on the fly */

#define Wfly( t ) ( (t) < 16 ? W[ (t) ] : Wf( (t) ) )

#if defined( VERBOSE )
#define REPORT( t ) \
    fprintf( stderr, "t = %2d: %08x   %08x   %08x   %08x   %08x\n",\
	     t, A, B, C, D, E );
#else
#define REPORT( t )
#endif

#define ROUND( t, A, B, C, D, E, Func, K ) \
    E += S( 5, A ) + Func( B, C, D ) + Wfly( t ) + K;\
    B = S( 30, B ); REPORT( t )

/* Remove rotatation E' = D; D' = C; C' = B; B' = A; A' = E; by
   completely unrolling and rotating the arguments to the macro ROUND
   manually so the rotation is compiled in.
*/

#define ROUND5( t, Func, K ) \
    ROUND( t + 0, A, B, C, D, E, Func, K );\
    ROUND( t + 1, E, A, B, C, D, Func, K );\
    ROUND( t + 2, D, E, A, B, C, Func, K );\
    ROUND( t + 3, C, D, E, A, B, Func, K );\
    ROUND( t + 4, B, C, D, E, A, Func, K )

#define ROUND20( t, Func, K )\
    ROUND5( t +  0, Func, K );\
    ROUND5( t +  5, Func, K );\
    ROUND5( t + 10, Func, K );\
    ROUND5( t + 15, Func, K )

/********************* use the macros *********************/

#if defined( VERBOSE )
    for ( t = 0; t < 16; t++ )
    {
	fprintf( stderr, "W[%2d] = %08x\n", t, W[ t ] );
    }
    fprintf( stderr, 
"            A           B           C           D           E\n\n" );
#endif


/* rounds  0..19 */

    ROUND20(  0, F1, K1 );

/* rounds 21..39 */

    ROUND20( 20, F2, K2 );

/* rounds 40..59 */

    ROUND20( 40, F3, K3 );

/* rounds 60..79 */

    ROUND20( 60, F4, K4 );
    
    H[ 0 ] += A;
    H[ 1 ] += B;
    H[ 2 ] += C;
    H[ 3 ] += D;
    H[ 4 ] += E;
}

void SHA1_update( SHA1_ctx* ctx, const void* pdata, unsigned data_len )
{
    const unsigned char* data = pdata;
    unsigned use;
    unsigned low_bits;
    int mlen;

/* convert data_len to bits and add to the 64-bit bit count */

#if defined( word64 )
    mlen = ( ctx->bits >> 3 ) % SHA1_INPUT_BYTES;
    ctx->bits += ( (word64) data_len ) << 3;
#else
    mlen = ( ctx->lbits >> 3 ) % SHA1_INPUT_BYTES;
    ctx->hbits += data_len >> 29; /* simulate 64 bit addition */
    low_bits = data_len << 3;
    ctx->lbits += low_bits;
    if ( ctx->lbits < low_bits ) { ctx->hbits++; }
#endif

/* deal with first block */

    use = min( SHA1_INPUT_BYTES - mlen, data_len );
    memcpy( ctx->M + mlen, data, use );
    mlen += use;
    data_len -= use;
    data += use;

    while ( mlen == SHA1_INPUT_BYTES )
    {
	make_big_endian32( (unsigned*)ctx->M, SHA1_INPUT_WORDS );
	SHA1_transform( ctx->H, ctx->M );
	use = min( SHA1_INPUT_BYTES, data_len );
	memcpy( ctx->M, data, use );
	mlen = use;
	data_len -= use;
        data += use;
    }
}

void SHA1_final( SHA1_ctx* ctx )
{
    int mlen;
    int padding;
#if defined( word64 )
    word64 temp;
#endif

#if defined( word64 )
    mlen = ( ctx->bits >> 3 ) % SHA1_INPUT_BYTES;
#else
    mlen = ( ctx->lbits >> 3 ) % SHA1_INPUT_BYTES;
#endif

    ctx->M[ mlen ] = 0x80; mlen++; /* append a 1 bit */
    padding = SHA1_INPUT_BYTES - mlen;

#define BIT_COUNT_WORDS 2
#define BIT_COUNT_BYTES ( BIT_COUNT_WORDS * sizeof( unsigned ) )

    if ( padding >= BIT_COUNT_BYTES )
    {
	memset( ctx->M + mlen, 0x00, padding - BIT_COUNT_BYTES );
	make_big_endian32( ctx->M, SHA1_INPUT_WORDS - BIT_COUNT_WORDS );
    }
    else
    {
	memset( ctx->M + mlen, 0x00, SHA1_INPUT_BYTES - mlen );
	make_big_endian32( ctx->M, SHA1_INPUT_WORDS );
	SHA1_transform( ctx->H, ctx->M );
	memset( ctx->M, 0x00, SHA1_INPUT_BYTES - BIT_COUNT_BYTES );
    }
    
#if defined( word64 )
    if ( little_endian )
    {
	temp = ( ctx->bits << 32 | ctx->bits >> 32 );
    }
    else
    {
	temp = ctx->bits;
    }
    memcpy( ctx->M + SHA1_INPUT_BYTES - BIT_COUNT_BYTES, &temp, 
	    BIT_COUNT_BYTES );
#else
    memcpy( ctx->M + SHA1_INPUT_BYTES - BIT_COUNT_BYTES, &(ctx->hbits), 
	    BIT_COUNT_BYTES );
#endif
    SHA1_transform( ctx->H, ctx->M );
}

const unsigned char* SHA1_get_digest( const SHA1_ctx* ctx )
{
    static unsigned char digest[ SHA1_DIGEST_BYTES ];
    
    if ( little_endian )
    {
	memcpy( digest, ctx->H, SHA1_DIGEST_BYTES );
	make_big_endian32( digest, SHA1_DIGEST_WORDS );
	return digest;
    }
    else
    {
	return (const unsigned char*) ( ctx->H );
    }
}
