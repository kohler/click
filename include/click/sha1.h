#if !defined( _sha1_h )
#define _sha1_h

#if defined( __cplusplus )
extern "C" {
#endif

/* This defaults to a run time endian test.  You can define BIG_ENDIAN
   or LITTLE_ENDIAN appropriately to speed things up.

   LITTLE_ENDIAN is the broken one: 80x86s, VAXs
   BIG_ENDIAN is: most unix machines, RISC chips, 68000, etc

   The endianess is stored in macros:

         little_endian
   and   big_endian

   These boolean values can be checked in your code in C expressions.

   They should NOT be tested with conditional macro statements (#ifdef
   etc). use BIG_ENDIAN and LITTLE_ENDIAN for this, if they are defined.

   Careful use should ensure the compiler compiles out code where
   possible
*/

#if defined( BIG_ENDIAN )
   #define little_endian 0
#elif defined( LITTLE_ENDIAN )
   #define little_endian 1
#else
   #define little_endian ( (* (long*) "\1\0\0\0" ) == 1 )
#endif

#define big_endian ( ! little_endian )

void swap_endian16( void*, int );
void swap_endian32( void*, int );

#define make_big_endian32( data, len ) \
    ( little_endian ? swap_endian32( data, len ) : 0 )

#define make_big_endian16( data, len ) \
    ( little_endian ? swap_endian16( data, len ) : 0 )

#define BURN( x, n ) memset( x, '\0', n )

#if defined( __cplusplus )
}
#endif

#define SHA1_set_IV( ctx, IV ) \
    memcpy( (ctx)->H, IV, SHA1_DIGEST_BYTES )

#define SHA1_zero_bitcount( ctx )\
    (ctx)->lbits = 0;\
    (ctx)->hbits = 0

#define SHA1_INPUT_BYTES 64	/* 512 bits */
#define SHA1_INPUT_WORDS ( SHA1_INPUT_BYTES >> 2 )
#define SHA1_DIGEST_WORDS 5	/* 160 bits */
#define SHA1_DIGEST_BYTES ( SHA1_DIGEST_WORDS * 4 )

#define SHA1_init( ctx ) \
    SHA1_zero_bitcount( ctx ); \
    SHA1_set_IV( ctx, SHA1_IV )

/* result is in reused memory, copy it if you want to keep it */
#define SHA1_digest( ctx ) \
    ( little_endian ? SHA1_get_digest( ctx ) : (const unsigned char*) (ctx)->H )


#if defined( __cplusplus )
extern "C" {
#endif

typedef struct {
    unsigned H[ SHA1_DIGEST_WORDS ];
    unsigned hbits, lbits;	/* if we don't have one we simulate it */
    unsigned char M[ SHA1_INPUT_BYTES ];
} SHA1_ctx;


extern unsigned SHA1_IV[ 5 ];

void SHA1_update( SHA1_ctx*, const void*, unsigned );
void SHA1_final ( SHA1_ctx* );

/* called by macro */
const unsigned char* SHA1_get_digest( const SHA1_ctx* );

/* these provide extra access to internals of SHA1 for MDC and MACs */

void SHA1_init_with_IV( SHA1_ctx*, const unsigned char[ SHA1_DIGEST_BYTES ] );
void SHA1_transform( unsigned[ SHA1_DIGEST_WORDS ], 
		     const unsigned char[ SHA1_INPUT_BYTES ] );

#if defined( __cplusplus )
}
#endif

#endif
