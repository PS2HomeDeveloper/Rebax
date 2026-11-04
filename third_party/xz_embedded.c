/* Generated from xz-embedded-master; implementation amalgamation. */
#include <stdint.h>
#include <stddef.h>
#include "xz_embedded.h"
/* SPDX-License-Identifier: 0BSD */

/*
 * Private includes and definitions for userspace use of XZ Embedded
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 */

#ifndef XZ_CONFIG_H
#define XZ_CONFIG_H

/* Uncomment to enable building of xz_dec_catrun(). */
/* #define XZ_DEC_CONCATENATED */

/* Uncomment to enable CRC64 support. */
/* #define XZ_USE_CRC64 */

/* Uncomment as needed to enable BCJ filter decoders. */
/* #define XZ_DEC_X86 */
/* #define XZ_DEC_ARM */
/* #define XZ_DEC_ARMTHUMB */
/* #define XZ_DEC_ARM64 */
/* #define XZ_DEC_RISCV */
/* #define XZ_DEC_POWERPC */
/* #define XZ_DEC_IA64 */
/* #define XZ_DEC_SPARC */

/*
 * Visual Studio 2013 update 2 supports only __inline, not inline.
 * MSVC v19.0 / VS 2015 and newer support both.
 */
#if defined(_MSC_VER) && _MSC_VER < 1900 && !defined(inline)
#	define inline __inline
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "xz_embedded.h"

#define kmalloc(size, flags) malloc(size)
#define kfree(ptr) free(ptr)
#define vmalloc(size) malloc(size)
#define vfree(ptr) free(ptr)

#define memeq(a, b, size) (memcmp(a, b, size) == 0)
#define memzero(buf, size) memset(buf, 0, size)

#ifndef min
#	define min(x, y) ((x) < (y) ? (x) : (y))
#endif
#define min_t(type, x, y) min(x, y)

#ifndef fallthrough
#	if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311
#		define fallthrough [[fallthrough]]
#	elif (defined(__GNUC__) && __GNUC__ >= 7) \
			|| (defined(__clang_major__) && __clang_major__ >= 10)
#		define fallthrough __attribute__((__fallthrough__))
#	else
#		define fallthrough do {} while (0)
#	endif
#endif

/*
 * Some functions have been marked with __always_inline to keep the
 * performance reasonable even when the compiler is optimizing for
 * small code size. You may be able to save a few bytes by #defining
 * __always_inline to plain inline, but don't complain if the code
 * becomes slow.
 *
 * NOTE: System headers on GNU/Linux may #define this macro already,
 * so if you want to change it, you need to #undef it first.
 */
#ifndef __always_inline
#	ifdef __GNUC__
#		define __always_inline \
			inline __attribute__((__always_inline__))
#	else
#		define __always_inline inline
#	endif
#endif

/* Inline functions to access unaligned unsigned 32-bit integers */
#ifndef get_unaligned_le32
static inline uint32_t get_unaligned_le32(const uint8_t *buf)
{
	return (uint32_t)buf[0]
			| ((uint32_t)buf[1] << 8)
			| ((uint32_t)buf[2] << 16)
			| ((uint32_t)buf[3] << 24);
}
#endif

#ifndef get_unaligned_be32
static inline uint32_t get_unaligned_be32(const uint8_t *buf)
{
	return (uint32_t)((uint32_t)buf[0] << 24)
			| ((uint32_t)buf[1] << 16)
			| ((uint32_t)buf[2] << 8)
			| (uint32_t)buf[3];
}
#endif

#ifndef put_unaligned_le32
static inline void put_unaligned_le32(uint32_t val, uint8_t *buf)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
	buf[2] = (uint8_t)(val >> 16);
	buf[3] = (uint8_t)(val >> 24);
}
#endif

#ifndef put_unaligned_be32
static inline void put_unaligned_be32(uint32_t val, uint8_t *buf)
{
	buf[0] = (uint8_t)(val >> 24);
	buf[1] = (uint8_t)(val >> 16);
	buf[2] = (uint8_t)(val >> 8);
	buf[3] = (uint8_t)val;
}
#endif

/*
 * To keep things simpler, use the generic unaligned methods also for
 * aligned access. The only place where performance could matter is
 * SHA-256 but files using SHA-256 aren't common.
 */
#ifndef get_le32
#	define get_le32 get_unaligned_le32
#endif
#ifndef get_be32
#	define get_be32 get_unaligned_be32
#endif

#endif
/* SPDX-License-Identifier: 0BSD */

/*
 * Definitions for handling the .xz file format
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 */

#ifndef XZ_STREAM_H
#define XZ_STREAM_H

#if defined(__KERNEL__) && !XZ_INTERNAL_CRC32
#	include <linux/crc32.h>
#	undef crc32
#	define xz_crc32(buf, size, crc) \
		(~crc32_le(~(uint32_t)(crc), buf, size))
#endif

/*
 * See the .xz file format specification at
 * https://tukaani.org/xz/xz-file-format.txt
 * to understand the container format.
 */

#define STREAM_HEADER_SIZE 12

#define HEADER_MAGIC "\3757zXZ"
#define HEADER_MAGIC_SIZE 6

#define FOOTER_MAGIC "YZ"
#define FOOTER_MAGIC_SIZE 2

/*
 * Variable-length integer can hold a 63-bit unsigned integer or a special
 * value indicating that the value is unknown.
 *
 * Experimental: vli_type can be defined to uint32_t to save a few bytes
 * in code size (no effect on speed). Doing so limits the uncompressed and
 * compressed size of the file to less than 256 MiB and may also weaken
 * error detection slightly.
 */
typedef uint64_t vli_type;

#define VLI_MAX ((vli_type)-1 / 2)
#define VLI_UNKNOWN ((vli_type)-1)

/* Maximum encoded size of a VLI */
#define VLI_BYTES_MAX (sizeof(vli_type) * 8 / 7)

/* Integrity Check types */
enum xz_check {
	XZ_CHECK_NONE = 0,
	XZ_CHECK_CRC32 = 1,
	XZ_CHECK_CRC64 = 4,
	XZ_CHECK_SHA256 = 10
};

/* Maximum possible Check ID */
#define XZ_CHECK_MAX 15

#endif
/* SPDX-License-Identifier: 0BSD */

/*
 * LZMA2 definitions
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 */

#ifndef XZ_LZMA2_H
#define XZ_LZMA2_H

/* Range coder constants */
#define RC_SHIFT_BITS 8
#define RC_TOP_BITS 24
#define RC_TOP_VALUE (1 << RC_TOP_BITS)
#define RC_BIT_MODEL_TOTAL_BITS 11
#define RC_BIT_MODEL_TOTAL (1 << RC_BIT_MODEL_TOTAL_BITS)
#define RC_MOVE_BITS 5

/*
 * Maximum number of position states. A position state is the lowest pb
 * number of bits of the current uncompressed offset. In some places there
 * are different sets of probabilities for different position states.
 */
#define POS_STATES_MAX (1 << 4)

/*
 * This enum is used to track which LZMA symbols have occurred most recently
 * and in which order. This information is used to predict the next symbol.
 *
 * Symbols:
 *  - Literal: One 8-bit byte
 *  - Match: Repeat a chunk of data at some distance
 *  - Long repeat: Multi-byte match at a recently seen distance
 *  - Short repeat: One-byte repeat at a recently seen distance
 *
 * The symbol names are in from STATE_oldest_older_previous. REP means
 * either short or long repeated match, and NONLIT means any non-literal.
 */
enum lzma_state {
	STATE_LIT_LIT,
	STATE_MATCH_LIT_LIT,
	STATE_REP_LIT_LIT,
	STATE_SHORTREP_LIT_LIT,
	STATE_MATCH_LIT,
	STATE_REP_LIT,
	STATE_SHORTREP_LIT,
	STATE_LIT_MATCH,
	STATE_LIT_LONGREP,
	STATE_LIT_SHORTREP,
	STATE_NONLIT_MATCH,
	STATE_NONLIT_REP
};

/* Total number of states */
#define STATES 12

/* The lowest 7 states indicate that the previous state was a literal. */
#define LIT_STATES 7

/* Indicate that the latest symbol was a literal. */
static inline void lzma_state_literal(enum lzma_state *state)
{
	if (*state <= STATE_SHORTREP_LIT_LIT)
		*state = STATE_LIT_LIT;
	else if (*state <= STATE_LIT_SHORTREP)
		*state -= 3;
	else
		*state -= 6;
}

/* Indicate that the latest symbol was a match. */
static inline void lzma_state_match(enum lzma_state *state)
{
	*state = *state < LIT_STATES ? STATE_LIT_MATCH : STATE_NONLIT_MATCH;
}

/* Indicate that the latest state was a long repeated match. */
static inline void lzma_state_long_rep(enum lzma_state *state)
{
	*state = *state < LIT_STATES ? STATE_LIT_LONGREP : STATE_NONLIT_REP;
}

/* Indicate that the latest symbol was a short match. */
static inline void lzma_state_short_rep(enum lzma_state *state)
{
	*state = *state < LIT_STATES ? STATE_LIT_SHORTREP : STATE_NONLIT_REP;
}

/* Test if the previous symbol was a literal. */
static inline bool lzma_state_is_literal(enum lzma_state state)
{
	return state < LIT_STATES;
}

/* Each literal coder is divided in three sections:
 *   - 0x001-0x0FF: Without match byte
 *   - 0x101-0x1FF: With match byte; match bit is 0
 *   - 0x201-0x2FF: With match byte; match bit is 1
 *
 * Match byte is used when the previous LZMA symbol was something else than
 * a literal (that is, it was some kind of match).
 */
#define LITERAL_CODER_SIZE 0x300

/* Maximum number of literal coders */
#define LITERAL_CODERS_MAX (1 << 4)

/* Minimum length of a match is two bytes. */
#define MATCH_LEN_MIN 2

/* Match length is encoded with 4, 5, or 10 bits.
 *
 * Length   Bits
 *  2-9      4 = Choice=0 + 3 bits
 * 10-17     5 = Choice=1 + Choice2=0 + 3 bits
 * 18-273   10 = Choice=1 + Choice2=1 + 8 bits
 */
#define LEN_LOW_BITS 3
#define LEN_LOW_SYMBOLS (1 << LEN_LOW_BITS)
#define LEN_MID_BITS 3
#define LEN_MID_SYMBOLS (1 << LEN_MID_BITS)
#define LEN_HIGH_BITS 8
#define LEN_HIGH_SYMBOLS (1 << LEN_HIGH_BITS)
#define LEN_SYMBOLS (LEN_LOW_SYMBOLS + LEN_MID_SYMBOLS + LEN_HIGH_SYMBOLS)

/*
 * Maximum length of a match is 273 which is a result of the encoding
 * described above.
 */
#define MATCH_LEN_MAX (MATCH_LEN_MIN + LEN_SYMBOLS - 1)

/*
 * Different sets of probabilities are used for match distances that have
 * very short match length: Lengths of 2, 3, and 4 bytes have a separate
 * set of probabilities for each length. The matches with longer length
 * use a shared set of probabilities.
 */
#define DIST_STATES 4

/*
 * Get the index of the appropriate probability array for decoding
 * the distance slot.
 */
static inline uint32_t lzma_get_dist_state(uint32_t len)
{
	return len < DIST_STATES + MATCH_LEN_MIN
			? len - MATCH_LEN_MIN : DIST_STATES - 1;
}

/*
 * The highest two bits of a 32-bit match distance are encoded using six bits.
 * This six-bit value is called a distance slot. This way encoding a 32-bit
 * value takes 6-36 bits, larger values taking more bits.
 */
#define DIST_SLOT_BITS 6
#define DIST_SLOTS (1 << DIST_SLOT_BITS)

/* Match distances up to 127 are fully encoded using probabilities. Since
 * the highest two bits (distance slot) are always encoded using six bits,
 * the distances 0-3 don't need any additional bits to encode, since the
 * distance slot itself is the same as the actual distance. DIST_MODEL_START
 * indicates the first distance slot where at least one additional bit is
 * needed.
 */
#define DIST_MODEL_START 4

/*
 * Match distances greater than 127 are encoded in three pieces:
 *   - distance slot: the highest two bits
 *   - direct bits: 2-26 bits below the highest two bits
 *   - alignment bits: four lowest bits
 *
 * Direct bits don't use any probabilities.
 *
 * The distance slot value of 14 is for distances 128-191.
 */
#define DIST_MODEL_END 14

/* Distance slots that indicate a distance <= 127. */
#define FULL_DISTANCES_BITS (DIST_MODEL_END / 2)
#define FULL_DISTANCES (1 << FULL_DISTANCES_BITS)

/*
 * For match distances greater than 127, only the highest two bits and the
 * lowest four bits (alignment) is encoded using probabilities.
 */
#define ALIGN_BITS 4
#define ALIGN_SIZE (1 << ALIGN_BITS)
#define ALIGN_MASK (ALIGN_SIZE - 1)

/* Total number of all probability variables */
#define PROBS_TOTAL (1846 + LITERAL_CODERS_MAX * LITERAL_CODER_SIZE)

/*
 * LZMA remembers the four most recent match distances. Reusing these
 * distances tends to take less space than re-encoding the actual
 * distance value.
 */
#define REPS 4

#endif
/* SPDX-License-Identifier: 0BSD */

/*
 * Private includes and definitions
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 */

#ifndef XZ_PRIVATE_H
#define XZ_PRIVATE_H

#ifdef __KERNEL__
#	include <linux/xz.h>
#	include <linux/kernel.h>
#	include <linux/unaligned.h>
	/* XZ_PREBOOT may be defined only via decompress_unxz.c. */
#	ifndef XZ_PREBOOT
#		include <linux/slab.h>
#		include <linux/vmalloc.h>
#		include <linux/string.h>
#		ifdef CONFIG_XZ_DEC_X86
#			define XZ_DEC_X86
#		endif
#		ifdef CONFIG_XZ_DEC_POWERPC
#			define XZ_DEC_POWERPC
#		endif
#		ifdef CONFIG_XZ_DEC_IA64
#			define XZ_DEC_IA64
#		endif
#		ifdef CONFIG_XZ_DEC_ARM
#			define XZ_DEC_ARM
#		endif
#		ifdef CONFIG_XZ_DEC_ARMTHUMB
#			define XZ_DEC_ARMTHUMB
#		endif
#		ifdef CONFIG_XZ_DEC_SPARC
#			define XZ_DEC_SPARC
#		endif
#		ifdef CONFIG_XZ_DEC_ARM64
#			define XZ_DEC_ARM64
#		endif
#		ifdef CONFIG_XZ_DEC_RISCV
#			define XZ_DEC_RISCV
#		endif
#		ifdef CONFIG_XZ_DEC_MICROLZMA
#			define XZ_DEC_MICROLZMA
#		endif
#		define memeq(a, b, size) (memcmp(a, b, size) == 0)
#		define memzero(buf, size) memset(buf, 0, size)
#	endif
#	define get_le32(p) le32_to_cpup((const uint32_t *)(p))
#else
	/*
	 * For userspace builds, use a separate header to define the required
	 * macros and functions. This makes it easier to adapt the code into
	 * different environments and avoids clutter in the Linux kernel tree.
	 */
#endif

/* If no specific decoding mode is requested, enable support for all modes. */
#if !defined(XZ_DEC_SINGLE) && !defined(XZ_DEC_PREALLOC) \
		&& !defined(XZ_DEC_DYNALLOC)
#	define XZ_DEC_SINGLE
#	define XZ_DEC_PREALLOC
#	define XZ_DEC_DYNALLOC
#endif

/*
 * The DEC_IS_foo(mode) macros are used in "if" statements. If only some
 * of the supported modes are enabled, these macros will evaluate to true or
 * false at compile time and thus allow the compiler to omit unneeded code.
 */
#ifdef XZ_DEC_SINGLE
#	define DEC_IS_SINGLE(mode) ((mode) == XZ_SINGLE)
#else
#	define DEC_IS_SINGLE(mode) (false)
#endif

#ifdef XZ_DEC_PREALLOC
#	define DEC_IS_PREALLOC(mode) ((mode) == XZ_PREALLOC)
#else
#	define DEC_IS_PREALLOC(mode) (false)
#endif

#ifdef XZ_DEC_DYNALLOC
#	define DEC_IS_DYNALLOC(mode) ((mode) == XZ_DYNALLOC)
#else
#	define DEC_IS_DYNALLOC(mode) (false)
#endif

#if !defined(XZ_DEC_SINGLE)
#	define DEC_IS_MULTI(mode) (true)
#elif defined(XZ_DEC_PREALLOC) || defined(XZ_DEC_DYNALLOC)
#	define DEC_IS_MULTI(mode) ((mode) != XZ_SINGLE)
#else
#	define DEC_IS_MULTI(mode) (false)
#endif

/*
 * If any of the BCJ filter decoders are wanted, define XZ_DEC_BCJ.
 * XZ_DEC_BCJ is used to enable generic support for BCJ decoders.
 */
#ifndef XZ_DEC_BCJ
#	if defined(XZ_DEC_X86) || defined(XZ_DEC_POWERPC) \
			|| defined(XZ_DEC_IA64) \
			|| defined(XZ_DEC_ARM) || defined(XZ_DEC_ARMTHUMB) \
			|| defined(XZ_DEC_SPARC) || defined(XZ_DEC_ARM64) \
			|| defined(XZ_DEC_RISCV)
#		define XZ_DEC_BCJ
#	endif
#endif

struct xz_sha256 {
	/* Buffered input data */
	uint8_t data[64];

	/* Internal state and the final hash value */
	uint32_t state[8];

	/* Size of the input data */
	uint64_t size;
};

/* Reset the SHA-256 state to prepare for a new calculation. */
XZ_EXTERN void xz_sha256_reset(struct xz_sha256 *s);

/* Update the SHA-256 state with new data. */
XZ_EXTERN void xz_sha256_update(const uint8_t *buf, size_t size,
				struct xz_sha256 *s);

/*
 * Finish the SHA-256 calculation. Compare the result with the first 32 bytes
 * from buf. Return true if the values are equal and false if they aren't.
 */
XZ_EXTERN bool xz_sha256_validate(const uint8_t *buf, struct xz_sha256 *s);

/*
 * Allocate memory for LZMA2 decoder. xz_dec_lzma2_reset() must be used
 * before calling xz_dec_lzma2_run().
 */
XZ_EXTERN struct xz_dec_lzma2 *xz_dec_lzma2_create(enum xz_mode mode,
						   uint32_t dict_max);

/*
 * Decode the LZMA2 properties (one byte) and reset the decoder. Return
 * XZ_OK on success, XZ_MEMLIMIT_ERROR if the preallocated dictionary is not
 * big enough, and XZ_OPTIONS_ERROR if props indicates something that this
 * decoder doesn't support.
 */
XZ_EXTERN enum xz_ret xz_dec_lzma2_reset(struct xz_dec_lzma2 *s,
					 uint8_t props);

/* Decode raw LZMA2 stream from b->in to b->out. */
XZ_EXTERN enum xz_ret xz_dec_lzma2_run(struct xz_dec_lzma2 *s,
				       struct xz_buf *b);

/* Free the memory allocated for the LZMA2 decoder. */
XZ_EXTERN void xz_dec_lzma2_end(struct xz_dec_lzma2 *s);

#ifdef XZ_DEC_BCJ
/*
 * Allocate memory for BCJ decoders. xz_dec_bcj_reset() must be used before
 * calling xz_dec_bcj_run().
 */
XZ_EXTERN struct xz_dec_bcj *xz_dec_bcj_create(bool single_call);

/*
 * Decode the Filter ID of a BCJ filter. This implementation doesn't
 * support custom start offsets, so no decoding of Filter Properties
 * is needed. Returns XZ_OK if the given Filter ID is supported.
 * Otherwise XZ_OPTIONS_ERROR is returned.
 */
XZ_EXTERN enum xz_ret xz_dec_bcj_reset(struct xz_dec_bcj *s, uint8_t id);

/*
 * Decode raw BCJ + LZMA2 stream. This must be used only if there actually is
 * a BCJ filter in the chain. If the chain has only LZMA2, xz_dec_lzma2_run()
 * must be called directly.
 */
XZ_EXTERN enum xz_ret xz_dec_bcj_run(struct xz_dec_bcj *s,
				     struct xz_dec_lzma2 *lzma2,
				     struct xz_buf *b);

/* Free the memory allocated for the BCJ filters. */
#define xz_dec_bcj_end(s) kfree(s)
#endif

#endif
/* BEGIN xz_crc32.c */
// SPDX-License-Identifier: 0BSD

/*
 * CRC32 using the polynomial from IEEE-802.3
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 */

/*
 * This is not the fastest implementation, but it is pretty compact.
 * The fastest versions of xz_crc32() on modern CPUs without hardware
 * accelerated CRC instruction are 3-5 times as fast as this version,
 * but they are bigger and use more memory for the lookup table.
 */


/*
 * STATIC_RW_DATA is used in the pre-boot environment on some architectures.
 * See <linux/decompress/mm.h> for details.
 */
#ifndef STATIC_RW_DATA
#	define STATIC_RW_DATA static
#endif

STATIC_RW_DATA uint32_t xz_crc32_table[256];

XZ_EXTERN void xz_crc32_init(void)
{
	const uint32_t poly = 0xEDB88320;

	uint32_t i;
	uint32_t j;
	uint32_t r;

	for (i = 0; i < 256; ++i) {
		r = i;
		for (j = 0; j < 8; ++j)
			r = (r >> 1) ^ (poly & ~((r & 1) - 1));

		xz_crc32_table[i] = r;
	}

	return;
}

XZ_EXTERN uint32_t xz_crc32(const uint8_t *buf, size_t size, uint32_t crc)
{
	crc = ~crc;

	while (size != 0) {
		crc = xz_crc32_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
/* END xz_crc32.c */
/* BEGIN xz_crc64.c */
// SPDX-License-Identifier: 0BSD

/*
 * CRC64 using the polynomial from ECMA-182
 *
 * This file is similar to xz_crc32.c. See the comments there.
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 */


#ifndef STATIC_RW_DATA
#	define STATIC_RW_DATA static
#endif

STATIC_RW_DATA uint64_t xz_crc64_table[256];

XZ_EXTERN void xz_crc64_init(void)
{
	/*
	 * The ULL suffix is needed for -std=gnu89 compatibility
	 * on 32-bit platforms.
	 */
	const uint64_t poly = 0xC96C5795D7870F42ULL;

	uint32_t i;
	uint32_t j;
	uint64_t r;

	for (i = 0; i < 256; ++i) {
		r = i;
		for (j = 0; j < 8; ++j)
			r = (r >> 1) ^ (poly & ~((r & 1) - 1));

		xz_crc64_table[i] = r;
	}

	return;
}

XZ_EXTERN uint64_t xz_crc64(const uint8_t *buf, size_t size, uint64_t crc)
{
	crc = ~crc;

	while (size != 0) {
		crc = xz_crc64_table[*buf++ ^ (crc & 0xFF)] ^ (crc >> 8);
		--size;
	}

	return ~crc;
}
/* END xz_crc64.c */
/* BEGIN xz_sha256.c */
// SPDX-License-Identifier: 0BSD

/*
 * SHA-256
 *
 * This is based on the XZ Utils version which is based public domain code
 * from Crypto++ Library 5.5.1 released in 2007: https://www.cryptopp.com/
 *
 * Authors: Wei Dai
 *          Lasse Collin <lasse.collin@tukaani.org>
 */


static inline uint32_t
rotr_32(uint32_t num, unsigned amount)
{
	return (num >> amount) | (num << (32 - amount));
}

#define blk0(i) (W[i] = get_be32(&data[4 * i]))
#define blk2(i) (W[i & 15] += s1(W[(i - 2) & 15]) + W[(i - 7) & 15] \
		+ s0(W[(i - 15) & 15]))

#define Ch(x, y, z) (z ^ (x & (y ^ z)))
#define Maj(x, y, z) ((x & (y ^ z)) + (y & z))

#define a(i) T[(0 - i) & 7]
#define b(i) T[(1 - i) & 7]
#define c(i) T[(2 - i) & 7]
#define d(i) T[(3 - i) & 7]
#define e(i) T[(4 - i) & 7]
#define f(i) T[(5 - i) & 7]
#define g(i) T[(6 - i) & 7]
#define h(i) T[(7 - i) & 7]

#define R(i, j, blk) \
	h(i) += S1(e(i)) + Ch(e(i), f(i), g(i)) + SHA256_K[i + j] + blk; \
	d(i) += h(i); \
	h(i) += S0(a(i)) + Maj(a(i), b(i), c(i))
#define R0(i) R(i, 0, blk0(i))
#define R2(i) R(i, j, blk2(i))

#define S0(x) rotr_32(x ^ rotr_32(x ^ rotr_32(x, 9), 11), 2)
#define S1(x) rotr_32(x ^ rotr_32(x ^ rotr_32(x, 14), 5), 6)
#define s0(x) (rotr_32(x ^ rotr_32(x, 11), 7) ^ (x >> 3))
#define s1(x) (rotr_32(x ^ rotr_32(x, 2), 17) ^ (x >> 10))

static const uint32_t SHA256_K[64] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
	0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
	0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
	0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
	0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
	0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
	0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
	0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
	0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

static void
transform(uint32_t state[8], const uint8_t data[64])
{
	uint32_t W[16];
	uint32_t T[8];
	unsigned int j;

	/* Copy state[] to working vars. */
	memcpy(T, state, sizeof(T));

	/* The first 16 operations unrolled */
	R0( 0); R0( 1); R0( 2); R0( 3);
	R0( 4); R0( 5); R0( 6); R0( 7);
	R0( 8); R0( 9); R0(10); R0(11);
	R0(12); R0(13); R0(14); R0(15);

	/* The remaining 48 operations partially unrolled */
	for (j = 16; j < 64; j += 16) {
		R2( 0); R2( 1); R2( 2); R2( 3);
		R2( 4); R2( 5); R2( 6); R2( 7);
		R2( 8); R2( 9); R2(10); R2(11);
		R2(12); R2(13); R2(14); R2(15);
	}

	/* Add the working vars back into state[]. */
	state[0] += a(0);
	state[1] += b(0);
	state[2] += c(0);
	state[3] += d(0);
	state[4] += e(0);
	state[5] += f(0);
	state[6] += g(0);
	state[7] += h(0);
}

XZ_EXTERN void xz_sha256_reset(struct xz_sha256 *s)
{
	static const uint32_t initial_state[8] = {
		0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
		0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19
	};

	memcpy(s->state, initial_state, sizeof(initial_state));
	s->size = 0;
}

XZ_EXTERN void xz_sha256_update(const uint8_t *buf, size_t size,
				struct xz_sha256 *s)
{
	size_t copy_start;
	size_t copy_size;

	/*
	 * Copy the input data into a properly aligned temporary buffer.
	 * This way we can be called with arbitrarily sized buffers
	 * (no need to be a multiple of 64 bytes).
	 *
	 * Full 64-byte chunks could be processed directly from buf with
	 * unaligned access. It seemed to make very little difference in
	 * speed on x86-64 though. Thus it was omitted.
	 */
	while (size > 0) {
		copy_start = s->size & 0x3F;
		copy_size = 64 - copy_start;
		if (copy_size > size)
			copy_size = size;

		memcpy(s->data + copy_start, buf, copy_size);

		buf += copy_size;
		size -= copy_size;
		s->size += copy_size;

		if ((s->size & 0x3F) == 0)
			transform(s->state, s->data);
	}
}

XZ_EXTERN bool xz_sha256_validate(const uint8_t *buf, struct xz_sha256 *s)
{
	/*
	 * Add padding as described in RFC 3174 (it describes SHA-1 but
	 * the same padding style is used for SHA-256 too).
	 */
	size_t i = s->size & 0x3F;
	s->data[i++] = 0x80;

	while (i != 64 - 8) {
		if (i == 64) {
			transform(s->state, s->data);
			i = 0;
		}

		s->data[i++] = 0x00;
	}

	/* Convert the message size from bytes to bits. */
	s->size *= 8;

	/*
	 * Store the message size in big endian byte order and
	 * calculate the final hash value.
	 */
	for (i = 0; i < 8; ++i)
		s->data[64 - 8 + i] = (uint8_t)(s->size >> ((7 - i) * 8));

	transform(s->state, s->data);

	/* Compare if the hash value matches the first 32 bytes in buf. */
	for (i = 0; i < 8; ++i)
		if (get_unaligned_be32(buf + 4 * i) != s->state[i])
			return false;

	return true;
}
/* END xz_sha256.c */
/* BEGIN xz_dec_lzma2.c */
// SPDX-License-Identifier: 0BSD

/*
 * LZMA2 decoder
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 */


/*
 * Range decoder initialization eats the first five bytes of each LZMA chunk.
 */
#define RC_INIT_BYTES 5

/*
 * Minimum number of usable input buffer to safely decode one LZMA symbol.
 * The worst case is that we decode 22 bits using probabilities and 26
 * direct bits. This may decode at maximum of 20 bytes of input. However,
 * lzma_main() does an extra normalization before returning, thus we
 * need to put 21 here.
 */
#define LZMA_IN_REQUIRED 21

/*
 * Dictionary (history buffer)
 *
 * These are always true:
 *    start <= pos <= full <= end
 *    pos <= limit <= end
 *
 * In multi-call mode, also these are true:
 *    end == size
 *    size <= size_max
 *    allocated <= size
 *
 * Most of these variables are size_t to support single-call mode,
 * in which the dictionary variables address the actual output
 * buffer directly.
 */
struct dictionary {
	/* Beginning of the history buffer */
	uint8_t *buf;

	/* Old position in buf (before decoding more data) */
	size_t start;

	/* Position in buf */
	size_t pos;

	/*
	 * How full dictionary is. This is used to detect corrupt input that
	 * would read beyond the beginning of the uncompressed stream.
	 */
	size_t full;

	/* Write limit; we don't write to buf[limit] or later bytes. */
	size_t limit;

	/*
	 * End of the dictionary buffer. In multi-call mode, this is
	 * the same as the dictionary size. In single-call mode, this
	 * indicates the size of the output buffer.
	 */
	size_t end;

	/*
	 * Size of the dictionary as specified in Block Header. This is used
	 * together with "full" to detect corrupt input that would make us
	 * read beyond the beginning of the uncompressed stream.
	 */
	uint32_t size;

	/*
	 * Maximum allowed dictionary size in multi-call mode.
	 * This is ignored in single-call mode.
	 */
	uint32_t size_max;

	/*
	 * Amount of memory currently allocated for the dictionary.
	 * This is used only with XZ_DYNALLOC. (With XZ_PREALLOC,
	 * size_max is always the same as the allocated size.)
	 */
	uint32_t allocated;

	/* Operation mode */
	enum xz_mode mode;
};

/* Range decoder */
struct rc_dec {
	uint32_t range;
	uint32_t code;

	/*
	 * Number of initializing bytes remaining to be read
	 * by rc_read_init().
	 */
	uint32_t init_bytes_left;

	/*
	 * Buffer from which we read our input. It can be either
	 * temp.buf or the caller-provided input buffer.
	 */
	const uint8_t *in;
	size_t in_pos;
	size_t in_limit;
};

/* Probabilities for a length decoder. */
struct lzma_len_dec {
	/* Probability of match length being at least 10 */
	uint16_t choice;

	/* Probability of match length being at least 18 */
	uint16_t choice2;

	/* Probabilities for match lengths 2-9 */
	uint16_t low[POS_STATES_MAX][LEN_LOW_SYMBOLS];

	/* Probabilities for match lengths 10-17 */
	uint16_t mid[POS_STATES_MAX][LEN_MID_SYMBOLS];

	/* Probabilities for match lengths 18-273 */
	uint16_t high[LEN_HIGH_SYMBOLS];
};

struct lzma_dec {
	/* Distances of latest four matches */
	uint32_t rep0;
	uint32_t rep1;
	uint32_t rep2;
	uint32_t rep3;

	/* Types of the most recently seen LZMA symbols */
	enum lzma_state state;

	/*
	 * Length of a match. This is updated so that dict_repeat can
	 * be called again to finish repeating the whole match.
	 */
	uint32_t len;

	/*
	 * LZMA properties or related bit masks (number of literal
	 * context bits, a mask derived from the number of literal
	 * position bits, and a mask derived from the number
	 * position bits)
	 */
	uint32_t lc;
	uint32_t literal_pos_mask; /* (1 << lp) - 1 */
	uint32_t pos_mask;         /* (1 << pb) - 1 */

	/* If 1, it's a match. Otherwise it's a single 8-bit literal. */
	uint16_t is_match[STATES][POS_STATES_MAX];

	/* If 1, it's a repeated match. The distance is one of rep0 .. rep3. */
	uint16_t is_rep[STATES];

	/*
	 * If 0, distance of a repeated match is rep0.
	 * Otherwise check is_rep1.
	 */
	uint16_t is_rep0[STATES];

	/*
	 * If 0, distance of a repeated match is rep1.
	 * Otherwise check is_rep2.
	 */
	uint16_t is_rep1[STATES];

	/* If 0, distance of a repeated match is rep2. Otherwise it is rep3. */
	uint16_t is_rep2[STATES];

	/*
	 * If 1, the repeated match has length of one byte. Otherwise
	 * the length is decoded from rep_len_decoder.
	 */
	uint16_t is_rep0_long[STATES][POS_STATES_MAX];

	/*
	 * Probability tree for the highest two bits of the match
	 * distance. There is a separate probability tree for match
	 * lengths of 2 (i.e. MATCH_LEN_MIN), 3, 4, and [5, 273].
	 */
	uint16_t dist_slot[DIST_STATES][DIST_SLOTS];

	/*
	 * Probability trees for additional bits for match distance
	 * when the distance is in the range [4, 127].
	 */
	uint16_t dist_special[FULL_DISTANCES - DIST_MODEL_END];

	/*
	 * Probability tree for the lowest four bits of a match
	 * distance that is equal to or greater than 128.
	 */
	uint16_t dist_align[ALIGN_SIZE];

	/* Length of a normal match */
	struct lzma_len_dec match_len_dec;

	/* Length of a repeated match */
	struct lzma_len_dec rep_len_dec;

	/* Probabilities of literals */
	uint16_t literal[LITERAL_CODERS_MAX][LITERAL_CODER_SIZE];
};

struct lzma2_dec {
	/* Position in xz_dec_lzma2_run(). */
	enum lzma2_seq {
		SEQ_CONTROL,
		SEQ_UNCOMPRESSED_1,
		SEQ_UNCOMPRESSED_2,
		SEQ_COMPRESSED_0,
		SEQ_COMPRESSED_1,
		SEQ_PROPERTIES,
		SEQ_LZMA_PREPARE,
		SEQ_LZMA_RUN,
		SEQ_COPY
	} sequence;

	/* Next position after decoding the compressed size of the chunk. */
	enum lzma2_seq next_sequence;

	/* Uncompressed size of LZMA chunk (2 MiB at maximum) */
	uint32_t uncompressed;

	/*
	 * Compressed size of LZMA chunk or compressed/uncompressed
	 * size of uncompressed chunk (64 KiB at maximum)
	 */
	uint32_t compressed;

	/*
	 * True if dictionary reset is needed. This is false before
	 * the first chunk (LZMA or uncompressed).
	 */
	bool need_dict_reset;

	/*
	 * True if new LZMA properties are needed. This is false
	 * before the first LZMA chunk.
	 */
	bool need_props;

#ifdef XZ_DEC_MICROLZMA
	bool pedantic_microlzma;
#endif
};

struct xz_dec_lzma2 {
	/*
	 * The order below is important on x86 to reduce code size and
	 * it shouldn't hurt on other platforms. Everything up to and
	 * including lzma.pos_mask are in the first 128 bytes on x86-32,
	 * which allows using smaller instructions to access those
	 * variables. On x86-64, fewer variables fit into the first 128
	 * bytes, but this is still the best order without sacrificing
	 * the readability by splitting the structures.
	 */
	struct rc_dec rc;
	struct dictionary dict;
	struct lzma2_dec lzma2;
	struct lzma_dec lzma;

	/*
	 * Temporary buffer which holds small number of input bytes between
	 * decoder calls. See lzma2_lzma() for details.
	 */
	struct {
		uint32_t size;
		uint8_t buf[3 * LZMA_IN_REQUIRED];
	} temp;
};

/**************
 * Dictionary *
 **************/

/*
 * Reset the dictionary state. When in single-call mode, set up the beginning
 * of the dictionary to point to the actual output buffer.
 */
static void dict_reset(struct dictionary *dict, struct xz_buf *b)
{
	if (DEC_IS_SINGLE(dict->mode)) {
		dict->buf = b->out + b->out_pos;
		dict->end = b->out_size - b->out_pos;
	}

	dict->start = 0;
	dict->pos = 0;
	dict->limit = 0;
	dict->full = 0;
}

/* Set dictionary write limit */
static void dict_limit(struct dictionary *dict, size_t out_max)
{
	if (dict->end - dict->pos <= out_max)
		dict->limit = dict->end;
	else
		dict->limit = dict->pos + out_max;
}

/* Return true if at least one byte can be written into the dictionary. */
static inline bool dict_has_space(const struct dictionary *dict)
{
	return dict->pos < dict->limit;
}

/*
 * Get a byte from the dictionary at the given distance. The distance is
 * assumed to valid, or as a special case, zero when the dictionary is
 * still empty. This special case is needed for single-call decoding to
 * avoid writing a '\0' to the end of the destination buffer.
 */
static inline uint32_t dict_get(const struct dictionary *dict, uint32_t dist)
{
	size_t offset = dict->pos - dist - 1;

	if (dist >= dict->pos)
		offset += dict->end;

	return dict->full > 0 ? dict->buf[offset] : 0;
}

/*
 * Put one byte into the dictionary. It is assumed that there is space for it.
 */
static inline void dict_put(struct dictionary *dict, uint8_t byte)
{
	dict->buf[dict->pos++] = byte;

	if (dict->full < dict->pos)
		dict->full = dict->pos;
}

/*
 * Repeat given number of bytes from the given distance. If the distance is
 * invalid, false is returned. On success, true is returned and *len is
 * updated to indicate how many bytes were left to be repeated.
 */
static bool dict_repeat(struct dictionary *dict, uint32_t *len, uint32_t dist)
{
	size_t back;
	uint32_t left;

	if (dist >= dict->full || dist >= dict->size)
		return false;

	left = min_t(size_t, dict->limit - dict->pos, *len);
	*len -= left;

	back = dict->pos - dist - 1;
	if (dist >= dict->pos)
		back += dict->end;

	do {
		dict->buf[dict->pos++] = dict->buf[back++];
		if (back == dict->end)
			back = 0;
	} while (--left > 0);

	if (dict->full < dict->pos)
		dict->full = dict->pos;

	return true;
}

/* Copy uncompressed data as is from input to dictionary and output buffers. */
static void dict_uncompressed(struct dictionary *dict, struct xz_buf *b,
			      uint32_t *left)
{
	size_t copy_size;

	while (*left > 0 && b->in_pos < b->in_size
			&& b->out_pos < b->out_size) {
		copy_size = min(b->in_size - b->in_pos,
				b->out_size - b->out_pos);
		if (copy_size > dict->end - dict->pos)
			copy_size = dict->end - dict->pos;
		if (copy_size > *left)
			copy_size = *left;

		*left -= copy_size;

		/*
		 * If doing in-place decompression in single-call mode and the
		 * uncompressed size of the file is larger than the caller
		 * thought (i.e. it is invalid input!), the buffers below may
		 * overlap and cause undefined behavior with memcpy().
		 * With valid inputs memcpy() would be fine here.
		 */
		memmove(dict->buf + dict->pos, b->in + b->in_pos, copy_size);
		dict->pos += copy_size;

		if (dict->full < dict->pos)
			dict->full = dict->pos;

		if (DEC_IS_MULTI(dict->mode)) {
			if (dict->pos == dict->end)
				dict->pos = 0;

			/*
			 * Like above but for multi-call mode: use memmove()
			 * to avoid undefined behavior with invalid input.
			 */
			memmove(b->out + b->out_pos, b->in + b->in_pos,
					copy_size);
		}

		dict->start = dict->pos;

		b->out_pos += copy_size;
		b->in_pos += copy_size;
	}
}

#ifdef XZ_DEC_MICROLZMA
#	define DICT_FLUSH_SUPPORTS_SKIPPING true
#else
#	define DICT_FLUSH_SUPPORTS_SKIPPING false
#endif

/*
 * Flush pending data from dictionary to b->out. It is assumed that there is
 * enough space in b->out. This is guaranteed because caller uses dict_limit()
 * before decoding data into the dictionary.
 */
static uint32_t dict_flush(struct dictionary *dict, struct xz_buf *b)
{
	size_t copy_size = dict->pos - dict->start;

	if (DEC_IS_MULTI(dict->mode)) {
		if (dict->pos == dict->end)
			dict->pos = 0;

		/*
		 * These buffers cannot overlap even if doing in-place
		 * decompression because in multi-call mode dict->buf
		 * has been allocated by us in this file; it's not
		 * provided by the caller like in single-call mode.
		 *
		 * With MicroLZMA, b->out can be NULL to skip bytes that
		 * the caller doesn't need. This cannot be done with XZ
		 * because it would break BCJ filters.
		 */
		if (!DICT_FLUSH_SUPPORTS_SKIPPING || b->out != NULL)
			memcpy(b->out + b->out_pos, dict->buf + dict->start,
					copy_size);
	}

	dict->start = dict->pos;
	b->out_pos += copy_size;
	return copy_size;
}

/*****************
 * Range decoder *
 *****************/

/* Reset the range decoder. */
static void rc_reset(struct rc_dec *rc)
{
	rc->range = (uint32_t)-1;
	rc->code = 0;
	rc->init_bytes_left = RC_INIT_BYTES;
}

/*
 * Read the first five initial bytes into rc->code if they haven't been
 * read already. (Yes, the first byte gets completely ignored.)
 */
static bool rc_read_init(struct rc_dec *rc, struct xz_buf *b)
{
	while (rc->init_bytes_left > 0) {
		if (b->in_pos == b->in_size)
			return false;

		rc->code = (rc->code << 8) + b->in[b->in_pos++];
		--rc->init_bytes_left;
	}

	return true;
}

/* Return true if there may not be enough input for the next decoding loop. */
static inline bool rc_limit_exceeded(const struct rc_dec *rc)
{
	return rc->in_pos > rc->in_limit;
}

/*
 * Return true if it is possible (from point of view of range decoder) that
 * we have reached the end of the LZMA chunk.
 */
static inline bool rc_is_finished(const struct rc_dec *rc)
{
	return rc->code == 0;
}

/* Read the next input byte if needed. */
static __always_inline void rc_normalize(struct rc_dec *rc)
{
	if (rc->range < RC_TOP_VALUE) {
		rc->range <<= RC_SHIFT_BITS;
		rc->code = (rc->code << RC_SHIFT_BITS) + rc->in[rc->in_pos++];
	}
}

/*
 * Decode one bit. In some versions, this function has been split in three
 * functions so that the compiler is supposed to be able to more easily avoid
 * an extra branch. In this particular version of the LZMA decoder, this
 * doesn't seem to be a good idea (tested with GCC 3.3.6, 3.4.6, and 4.3.3
 * on x86). Using a non-split version results in nicer looking code too.
 *
 * NOTE: This must return an int. Do not make it return a bool or the speed
 * of the code generated by GCC 3.x decreases 10-15 %. (GCC 4.3 doesn't care,
 * and it generates 10-20 % faster code than GCC 3.x from this file anyway.)
 */
static __always_inline int rc_bit(struct rc_dec *rc, uint16_t *prob)
{
	uint32_t bound;
	int bit;

	rc_normalize(rc);
	bound = (rc->range >> RC_BIT_MODEL_TOTAL_BITS) * *prob;
	if (rc->code < bound) {
		rc->range = bound;
		*prob += (RC_BIT_MODEL_TOTAL - *prob) >> RC_MOVE_BITS;
		bit = 0;
	} else {
		rc->range -= bound;
		rc->code -= bound;
		*prob -= *prob >> RC_MOVE_BITS;
		bit = 1;
	}

	return bit;
}

/* Decode a bittree starting from the most significant bit. */
static __always_inline uint32_t rc_bittree(struct rc_dec *rc,
					   uint16_t *probs, uint32_t limit)
{
	uint32_t symbol = 1;

	do {
		if (rc_bit(rc, &probs[symbol]))
			symbol = (symbol << 1) + 1;
		else
			symbol <<= 1;
	} while (symbol < limit);

	return symbol;
}

/* Decode a bittree starting from the least significant bit. */
static __always_inline void rc_bittree_reverse(struct rc_dec *rc,
					       uint16_t *probs,
					       uint32_t *dest, uint32_t limit)
{
	uint32_t symbol = 1;
	uint32_t i = 0;

	do {
		if (rc_bit(rc, &probs[symbol])) {
			symbol = (symbol << 1) + 1;
			*dest += 1 << i;
		} else {
			symbol <<= 1;
		}
	} while (++i < limit);
}

/* Decode direct bits (fixed fifty-fifty probability) */
static inline void rc_direct(struct rc_dec *rc, uint32_t *dest, uint32_t limit)
{
	uint32_t mask;

	do {
		rc_normalize(rc);
		rc->range >>= 1;
		rc->code -= rc->range;
		mask = (uint32_t)0 - (rc->code >> 31);
		rc->code += rc->range & mask;
		*dest = (*dest << 1) + (mask + 1);
	} while (--limit > 0);
}

/********
 * LZMA *
 ********/

/* Get pointer to literal coder probability array. */
static uint16_t *lzma_literal_probs(struct xz_dec_lzma2 *s)
{
	uint32_t prev_byte = dict_get(&s->dict, 0);
	uint32_t low = prev_byte >> (8 - s->lzma.lc);
	uint32_t high = (s->dict.pos & s->lzma.literal_pos_mask) << s->lzma.lc;
	return s->lzma.literal[low + high];
}

/* Decode a literal (one 8-bit byte) */
static void lzma_literal(struct xz_dec_lzma2 *s)
{
	uint16_t *probs;
	uint32_t symbol;
	uint32_t match_byte;
	uint32_t match_bit;
	uint32_t offset;
	uint32_t i;

	probs = lzma_literal_probs(s);

	if (lzma_state_is_literal(s->lzma.state)) {
		symbol = rc_bittree(&s->rc, probs, 0x100);
	} else {
		symbol = 1;
		match_byte = dict_get(&s->dict, s->lzma.rep0) << 1;
		offset = 0x100;

		do {
			match_bit = match_byte & offset;
			match_byte <<= 1;
			i = offset + match_bit + symbol;

			if (rc_bit(&s->rc, &probs[i])) {
				symbol = (symbol << 1) + 1;
				offset &= match_bit;
			} else {
				symbol <<= 1;
				offset &= ~match_bit;
			}
		} while (symbol < 0x100);
	}

	dict_put(&s->dict, (uint8_t)symbol);
	lzma_state_literal(&s->lzma.state);
}

/* Decode the length of the match into s->lzma.len. */
static void lzma_len(struct xz_dec_lzma2 *s, struct lzma_len_dec *l,
		     uint32_t pos_state)
{
	uint16_t *probs;
	uint32_t limit;

	if (!rc_bit(&s->rc, &l->choice)) {
		probs = l->low[pos_state];
		limit = LEN_LOW_SYMBOLS;
		s->lzma.len = MATCH_LEN_MIN;
	} else {
		if (!rc_bit(&s->rc, &l->choice2)) {
			probs = l->mid[pos_state];
			limit = LEN_MID_SYMBOLS;
			s->lzma.len = MATCH_LEN_MIN + LEN_LOW_SYMBOLS;
		} else {
			probs = l->high;
			limit = LEN_HIGH_SYMBOLS;
			s->lzma.len = MATCH_LEN_MIN + LEN_LOW_SYMBOLS
					+ LEN_MID_SYMBOLS;
		}
	}

	s->lzma.len += rc_bittree(&s->rc, probs, limit) - limit;
}

/* Decode a match. The distance will be stored in s->lzma.rep0. */
static void lzma_match(struct xz_dec_lzma2 *s, uint32_t pos_state)
{
	uint16_t *probs;
	uint32_t dist_slot;
	uint32_t limit;

	lzma_state_match(&s->lzma.state);

	s->lzma.rep3 = s->lzma.rep2;
	s->lzma.rep2 = s->lzma.rep1;
	s->lzma.rep1 = s->lzma.rep0;

	lzma_len(s, &s->lzma.match_len_dec, pos_state);

	probs = s->lzma.dist_slot[lzma_get_dist_state(s->lzma.len)];
	dist_slot = rc_bittree(&s->rc, probs, DIST_SLOTS) - DIST_SLOTS;

	if (dist_slot < DIST_MODEL_START) {
		s->lzma.rep0 = dist_slot;
	} else {
		limit = (dist_slot >> 1) - 1;
		s->lzma.rep0 = 2 + (dist_slot & 1);

		if (dist_slot < DIST_MODEL_END) {
			s->lzma.rep0 <<= limit;
			probs = s->lzma.dist_special + s->lzma.rep0
					- dist_slot - 1;
			rc_bittree_reverse(&s->rc, probs,
					&s->lzma.rep0, limit);
		} else {
			rc_direct(&s->rc, &s->lzma.rep0, limit - ALIGN_BITS);
			s->lzma.rep0 <<= ALIGN_BITS;
			rc_bittree_reverse(&s->rc, s->lzma.dist_align,
					&s->lzma.rep0, ALIGN_BITS);
		}
	}
}

/*
 * Decode a repeated match. The distance is one of the four most recently
 * seen matches. The distance will be stored in s->lzma.rep0.
 */
static void lzma_rep_match(struct xz_dec_lzma2 *s, uint32_t pos_state)
{
	uint32_t tmp;

	if (!rc_bit(&s->rc, &s->lzma.is_rep0[s->lzma.state])) {
		if (!rc_bit(&s->rc, &s->lzma.is_rep0_long[
				s->lzma.state][pos_state])) {
			lzma_state_short_rep(&s->lzma.state);
			s->lzma.len = 1;
			return;
		}
	} else {
		if (!rc_bit(&s->rc, &s->lzma.is_rep1[s->lzma.state])) {
			tmp = s->lzma.rep1;
		} else {
			if (!rc_bit(&s->rc, &s->lzma.is_rep2[s->lzma.state])) {
				tmp = s->lzma.rep2;
			} else {
				tmp = s->lzma.rep3;
				s->lzma.rep3 = s->lzma.rep2;
			}

			s->lzma.rep2 = s->lzma.rep1;
		}

		s->lzma.rep1 = s->lzma.rep0;
		s->lzma.rep0 = tmp;
	}

	lzma_state_long_rep(&s->lzma.state);
	lzma_len(s, &s->lzma.rep_len_dec, pos_state);
}

/* LZMA decoder core */
static bool lzma_main(struct xz_dec_lzma2 *s)
{
	uint32_t pos_state;

	/*
	 * If the dictionary was reached during the previous call, try to
	 * finish the possibly pending repeat in the dictionary.
	 */
	if (dict_has_space(&s->dict) && s->lzma.len > 0)
		dict_repeat(&s->dict, &s->lzma.len, s->lzma.rep0);

	/*
	 * Decode more LZMA symbols. One iteration may consume up to
	 * LZMA_IN_REQUIRED - 1 bytes.
	 */
	while (dict_has_space(&s->dict) && !rc_limit_exceeded(&s->rc)) {
		pos_state = s->dict.pos & s->lzma.pos_mask;

		if (!rc_bit(&s->rc, &s->lzma.is_match[
				s->lzma.state][pos_state])) {
			lzma_literal(s);
		} else {
			if (rc_bit(&s->rc, &s->lzma.is_rep[s->lzma.state]))
				lzma_rep_match(s, pos_state);
			else
				lzma_match(s, pos_state);

			if (!dict_repeat(&s->dict, &s->lzma.len, s->lzma.rep0))
				return false;
		}
	}

	/*
	 * Having the range decoder always normalized when we are outside
	 * this function makes it easier to correctly handle end of the chunk.
	 */
	rc_normalize(&s->rc);

	return true;
}

/*
 * Reset the LZMA decoder and range decoder state. Dictionary is not reset
 * here, because LZMA state may be reset without resetting the dictionary.
 */
static void lzma_reset(struct xz_dec_lzma2 *s)
{
	uint16_t *probs;
	size_t i;

	s->lzma.state = STATE_LIT_LIT;
	s->lzma.rep0 = 0;
	s->lzma.rep1 = 0;
	s->lzma.rep2 = 0;
	s->lzma.rep3 = 0;
	s->lzma.len = 0;

	/*
	 * All probabilities are initialized to the same value. This hack
	 * makes the code smaller by avoiding a separate loop for each
	 * probability array.
	 *
	 * This could be optimized so that only that part of literal
	 * probabilities that are actually required. In the common case
	 * we would write 12 KiB less.
	 */
	probs = s->lzma.is_match[0];
	for (i = 0; i < PROBS_TOTAL; ++i)
		probs[i] = RC_BIT_MODEL_TOTAL / 2;

	rc_reset(&s->rc);
}

/*
 * Decode and validate LZMA properties (lc/lp/pb) and calculate the bit masks
 * from the decoded lp and pb values. On success, the LZMA decoder state is
 * reset and true is returned.
 */
static bool lzma_props(struct xz_dec_lzma2 *s, uint8_t props)
{
	if (props > (4 * 5 + 4) * 9 + 8)
		return false;

	s->lzma.pos_mask = 0;
	while (props >= 9 * 5) {
		props -= 9 * 5;
		++s->lzma.pos_mask;
	}

	s->lzma.pos_mask = (1 << s->lzma.pos_mask) - 1;

	s->lzma.literal_pos_mask = 0;
	while (props >= 9) {
		props -= 9;
		++s->lzma.literal_pos_mask;
	}

	s->lzma.lc = props;

	if (s->lzma.lc + s->lzma.literal_pos_mask > 4)
		return false;

	s->lzma.literal_pos_mask = (1 << s->lzma.literal_pos_mask) - 1;

	lzma_reset(s);

	return true;
}

/*********
 * LZMA2 *
 *********/

/*
 * The LZMA decoder assumes that if the input limit (s->rc.in_limit) hasn't
 * been exceeded, it is safe to read up to LZMA_IN_REQUIRED bytes. This
 * wrapper function takes care of making the LZMA decoder's assumption safe.
 *
 * As long as there is plenty of input left to be decoded in the current LZMA
 * chunk, we decode directly from the caller-supplied input buffer until
 * there's LZMA_IN_REQUIRED bytes left. Those remaining bytes are copied into
 * s->temp.buf, which (hopefully) gets filled on the next call to this
 * function. We decode a few bytes from the temporary buffer so that we can
 * continue decoding from the caller-supplied input buffer again.
 */
static bool lzma2_lzma(struct xz_dec_lzma2 *s, struct xz_buf *b)
{
	size_t in_avail;
	uint32_t tmp;

	in_avail = b->in_size - b->in_pos;
	if (s->temp.size > 0 || s->lzma2.compressed == 0) {
		tmp = 2 * LZMA_IN_REQUIRED - s->temp.size;
		if (tmp > s->lzma2.compressed - s->temp.size)
			tmp = s->lzma2.compressed - s->temp.size;
		if (tmp > in_avail)
			tmp = in_avail;

		memcpy(s->temp.buf + s->temp.size, b->in + b->in_pos, tmp);

		if (s->temp.size + tmp == s->lzma2.compressed) {
			memzero(s->temp.buf + s->temp.size + tmp,
					sizeof(s->temp.buf)
						- s->temp.size - tmp);
			s->rc.in_limit = s->temp.size + tmp;
		} else if (s->temp.size + tmp < LZMA_IN_REQUIRED) {
			s->temp.size += tmp;
			b->in_pos += tmp;
			return true;
		} else {
			s->rc.in_limit = s->temp.size + tmp - LZMA_IN_REQUIRED;
		}

		s->rc.in = s->temp.buf;
		s->rc.in_pos = 0;

		if (!lzma_main(s) || s->rc.in_pos > s->temp.size + tmp)
			return false;

		s->lzma2.compressed -= s->rc.in_pos;

		if (s->rc.in_pos < s->temp.size) {
			s->temp.size -= s->rc.in_pos;
			memmove(s->temp.buf, s->temp.buf + s->rc.in_pos,
					s->temp.size);
			return true;
		}

		b->in_pos += s->rc.in_pos - s->temp.size;
		s->temp.size = 0;
	}

	in_avail = b->in_size - b->in_pos;
	if (in_avail >= LZMA_IN_REQUIRED) {
		s->rc.in = b->in;
		s->rc.in_pos = b->in_pos;

		if (in_avail >= s->lzma2.compressed + LZMA_IN_REQUIRED)
			s->rc.in_limit = b->in_pos + s->lzma2.compressed;
		else
			s->rc.in_limit = b->in_size - LZMA_IN_REQUIRED;

		if (!lzma_main(s))
			return false;

		in_avail = s->rc.in_pos - b->in_pos;
		if (in_avail > s->lzma2.compressed)
			return false;

		s->lzma2.compressed -= in_avail;
		b->in_pos = s->rc.in_pos;
	}

	in_avail = b->in_size - b->in_pos;
	if (in_avail < LZMA_IN_REQUIRED) {
		if (in_avail > s->lzma2.compressed)
			in_avail = s->lzma2.compressed;

		memcpy(s->temp.buf, b->in + b->in_pos, in_avail);
		s->temp.size = in_avail;
		b->in_pos += in_avail;
	}

	return true;
}

/*
 * Take care of the LZMA2 control layer, and forward the job of actual LZMA
 * decoding or copying of uncompressed chunks to other functions.
 */
XZ_EXTERN enum xz_ret xz_dec_lzma2_run(struct xz_dec_lzma2 *s,
				       struct xz_buf *b)
{
	uint32_t tmp;

	while (b->in_pos < b->in_size || s->lzma2.sequence == SEQ_LZMA_RUN) {
		switch (s->lzma2.sequence) {
		case SEQ_CONTROL:
			/*
			 * LZMA2 control byte
			 *
			 * Exact values:
			 *   0x00   End marker
			 *   0x01   Dictionary reset followed by
			 *          an uncompressed chunk
			 *   0x02   Uncompressed chunk (no dictionary reset)
			 *
			 * Highest three bits (s->control & 0xE0):
			 *   0xE0   Dictionary reset, new properties and state
			 *          reset, followed by LZMA compressed chunk
			 *   0xC0   New properties and state reset, followed
			 *          by LZMA compressed chunk (no dictionary
			 *          reset)
			 *   0xA0   State reset using old properties,
			 *          followed by LZMA compressed chunk (no
			 *          dictionary reset)
			 *   0x80   LZMA chunk (no dictionary or state reset)
			 *
			 * For LZMA compressed chunks, the lowest five bits
			 * (s->control & 1F) are the highest bits of the
			 * uncompressed size (bits 16-20).
			 *
			 * A new LZMA2 stream must begin with a dictionary
			 * reset. The first LZMA chunk must set new
			 * properties and reset the LZMA state.
			 *
			 * Values that don't match anything described above
			 * are invalid and we return XZ_DATA_ERROR.
			 */
			tmp = b->in[b->in_pos++];

			if (tmp == 0x00)
				return XZ_STREAM_END;

			if (tmp >= 0xE0 || tmp == 0x01) {
				s->lzma2.need_props = true;
				s->lzma2.need_dict_reset = false;
				dict_reset(&s->dict, b);
			} else if (s->lzma2.need_dict_reset) {
				return XZ_DATA_ERROR;
			}

			if (tmp >= 0x80) {
				s->lzma2.uncompressed = (tmp & 0x1F) << 16;
				s->lzma2.sequence = SEQ_UNCOMPRESSED_1;

				if (tmp >= 0xC0) {
					/*
					 * When there are new properties,
					 * state reset is done at
					 * SEQ_PROPERTIES.
					 */
					s->lzma2.need_props = false;
					s->lzma2.next_sequence
							= SEQ_PROPERTIES;

				} else if (s->lzma2.need_props) {
					return XZ_DATA_ERROR;

				} else {
					s->lzma2.next_sequence
							= SEQ_LZMA_PREPARE;
					if (tmp >= 0xA0)
						lzma_reset(s);
				}
			} else {
				if (tmp > 0x02)
					return XZ_DATA_ERROR;

				s->lzma2.sequence = SEQ_COMPRESSED_0;
				s->lzma2.next_sequence = SEQ_COPY;
			}

			break;

		case SEQ_UNCOMPRESSED_1:
			s->lzma2.uncompressed
					+= (uint32_t)b->in[b->in_pos++] << 8;
			s->lzma2.sequence = SEQ_UNCOMPRESSED_2;
			break;

		case SEQ_UNCOMPRESSED_2:
			s->lzma2.uncompressed
					+= (uint32_t)b->in[b->in_pos++] + 1;
			s->lzma2.sequence = SEQ_COMPRESSED_0;
			break;

		case SEQ_COMPRESSED_0:
			s->lzma2.compressed
					= (uint32_t)b->in[b->in_pos++] << 8;
			s->lzma2.sequence = SEQ_COMPRESSED_1;
			break;

		case SEQ_COMPRESSED_1:
			s->lzma2.compressed
					+= (uint32_t)b->in[b->in_pos++] + 1;
			s->lzma2.sequence = s->lzma2.next_sequence;
			break;

		case SEQ_PROPERTIES:
			if (!lzma_props(s, b->in[b->in_pos++]))
				return XZ_DATA_ERROR;

			s->lzma2.sequence = SEQ_LZMA_PREPARE;

			fallthrough;

		case SEQ_LZMA_PREPARE:
			if (s->lzma2.compressed < RC_INIT_BYTES)
				return XZ_DATA_ERROR;

			if (!rc_read_init(&s->rc, b))
				return XZ_OK;

			s->lzma2.compressed -= RC_INIT_BYTES;
			s->lzma2.sequence = SEQ_LZMA_RUN;

			fallthrough;

		case SEQ_LZMA_RUN:
			/*
			 * Set dictionary limit to indicate how much we want
			 * to be encoded at maximum. Decode new data into the
			 * dictionary. Flush the new data from dictionary to
			 * b->out. Check if we finished decoding this chunk.
			 * In case the dictionary got full but we didn't fill
			 * the output buffer yet, we may run this loop
			 * multiple times without changing s->lzma2.sequence.
			 */
			dict_limit(&s->dict, min_t(size_t,
					b->out_size - b->out_pos,
					s->lzma2.uncompressed));
			if (!lzma2_lzma(s, b))
				return XZ_DATA_ERROR;

			s->lzma2.uncompressed -= dict_flush(&s->dict, b);

			if (s->lzma2.uncompressed == 0) {
				if (s->lzma2.compressed > 0 || s->lzma.len > 0
						|| !rc_is_finished(&s->rc))
					return XZ_DATA_ERROR;

				rc_reset(&s->rc);
				s->lzma2.sequence = SEQ_CONTROL;

			} else if (b->out_pos == b->out_size
					|| (b->in_pos == b->in_size
						&& s->temp.size
						< s->lzma2.compressed)) {
				return XZ_OK;
			}

			break;

		case SEQ_COPY:
			dict_uncompressed(&s->dict, b, &s->lzma2.compressed);
			if (s->lzma2.compressed > 0)
				return XZ_OK;

			s->lzma2.sequence = SEQ_CONTROL;
			break;
		}
	}

	return XZ_OK;
}

XZ_EXTERN struct xz_dec_lzma2 *xz_dec_lzma2_create(enum xz_mode mode,
						   uint32_t dict_max)
{
	struct xz_dec_lzma2 *s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (s == NULL)
		return NULL;

	s->dict.mode = mode;
	s->dict.size_max = dict_max;

	if (DEC_IS_PREALLOC(mode)) {
		s->dict.buf = vmalloc(dict_max);
		if (s->dict.buf == NULL) {
			kfree(s);
			return NULL;
		}
	} else if (DEC_IS_DYNALLOC(mode)) {
		s->dict.buf = NULL;
		s->dict.allocated = 0;
	}

	return s;
}

XZ_EXTERN enum xz_ret xz_dec_lzma2_reset(struct xz_dec_lzma2 *s, uint8_t props)
{
	/* This limits dictionary size to 3 GiB to keep parsing simpler. */
	if (props > 39)
		return XZ_OPTIONS_ERROR;

	s->dict.size = 2 + (props & 1);
	s->dict.size <<= (props >> 1) + 11;

	if (DEC_IS_MULTI(s->dict.mode)) {
		if (s->dict.size > s->dict.size_max)
			return XZ_MEMLIMIT_ERROR;

		s->dict.end = s->dict.size;

		if (DEC_IS_DYNALLOC(s->dict.mode)) {
			if (s->dict.allocated < s->dict.size) {
				s->dict.allocated = s->dict.size;
				vfree(s->dict.buf);
				s->dict.buf = vmalloc(s->dict.size);
				if (s->dict.buf == NULL) {
					s->dict.allocated = 0;
					return XZ_MEM_ERROR;
				}
			}
		}
	}

	s->lzma2.sequence = SEQ_CONTROL;
	s->lzma2.need_dict_reset = true;

	s->temp.size = 0;

	return XZ_OK;
}

XZ_EXTERN void xz_dec_lzma2_end(struct xz_dec_lzma2 *s)
{
	if (DEC_IS_MULTI(s->dict.mode))
		vfree(s->dict.buf);

	kfree(s);
}

#ifdef XZ_DEC_MICROLZMA
/* This is a wrapper struct to have a nice struct name in the public API. */
struct xz_dec_microlzma {
	struct xz_dec_lzma2 s;
};

XZ_EXTERN enum xz_ret xz_dec_microlzma_run(struct xz_dec_microlzma *s_ptr,
					   struct xz_buf *b)
{
	struct xz_dec_lzma2 *s = &s_ptr->s;

	/*
	 * sequence is SEQ_PROPERTIES before the first input byte,
	 * SEQ_LZMA_PREPARE until a total of five bytes have been read,
	 * and SEQ_LZMA_RUN for the rest of the input stream.
	 */
	if (s->lzma2.sequence != SEQ_LZMA_RUN) {
		if (s->lzma2.sequence == SEQ_PROPERTIES) {
			/* One byte is needed for the props. */
			if (b->in_pos >= b->in_size)
				return XZ_OK;

			/*
			 * Don't increment b->in_pos here. The same byte is
			 * also passed to rc_read_init() which will ignore it.
			 */
			if (!lzma_props(s, ~b->in[b->in_pos]))
				return XZ_DATA_ERROR;

			s->lzma2.sequence = SEQ_LZMA_PREPARE;
		}

		/*
		 * xz_dec_microlzma_reset() doesn't validate the compressed
		 * size so we do it here. We have to limit the maximum size
		 * to avoid integer overflows in lzma2_lzma(). 3 GiB is a nice
		 * round number and much more than users of this code should
		 * ever need.
		 */
		if (s->lzma2.compressed < RC_INIT_BYTES
				|| s->lzma2.compressed > (3U << 30))
			return XZ_DATA_ERROR;

		if (!rc_read_init(&s->rc, b))
			return XZ_OK;

		s->lzma2.compressed -= RC_INIT_BYTES;
		s->lzma2.sequence = SEQ_LZMA_RUN;

		dict_reset(&s->dict, b);
	}

	/* This is to allow increasing b->out_size between calls. */
	if (DEC_IS_SINGLE(s->dict.mode))
		s->dict.end = b->out_size - b->out_pos;

	while (true) {
		dict_limit(&s->dict, min_t(size_t, b->out_size - b->out_pos,
					   s->lzma2.uncompressed));

		if (!lzma2_lzma(s, b))
			return XZ_DATA_ERROR;

		s->lzma2.uncompressed -= dict_flush(&s->dict, b);

		if (s->lzma2.uncompressed == 0) {
			if (s->lzma2.pedantic_microlzma) {
				if (s->lzma2.compressed > 0 || s->lzma.len > 0
						|| !rc_is_finished(&s->rc))
					return XZ_DATA_ERROR;
			}

			return XZ_STREAM_END;
		}

		if (b->out_pos == b->out_size)
			return XZ_OK;

		if (b->in_pos == b->in_size
				&& s->temp.size < s->lzma2.compressed)
			return XZ_OK;
	}
}

XZ_EXTERN struct xz_dec_microlzma *xz_dec_microlzma_alloc(enum xz_mode mode,
							  uint32_t dict_size)
{
	struct xz_dec_microlzma *s;

	/* Restrict dict_size to the same range as in the LZMA2 code. */
	if (dict_size < 4096 || dict_size > (3U << 30))
		return NULL;

	s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (s == NULL)
		return NULL;

	s->s.dict.mode = mode;
	s->s.dict.size = dict_size;

	if (DEC_IS_MULTI(mode)) {
		s->s.dict.end = dict_size;

		s->s.dict.buf = vmalloc(dict_size);
		if (s->s.dict.buf == NULL) {
			kfree(s);
			return NULL;
		}
	}

	return s;
}

XZ_EXTERN void xz_dec_microlzma_reset(struct xz_dec_microlzma *s,
				      uint32_t comp_size,
				      uint32_t uncomp_size,
				      int uncomp_size_is_exact)
{
	/*
	 * comp_size is validated in xz_dec_microlzma_run().
	 * uncomp_size can safely be anything.
	 */
	s->s.lzma2.compressed = comp_size;
	s->s.lzma2.uncompressed = uncomp_size;
	s->s.lzma2.pedantic_microlzma = uncomp_size_is_exact;

	s->s.lzma2.sequence = SEQ_PROPERTIES;
	s->s.temp.size = 0;
}

XZ_EXTERN void xz_dec_microlzma_end(struct xz_dec_microlzma *s)
{
	if (DEC_IS_MULTI(s->s.dict.mode))
		vfree(s->s.dict.buf);

	kfree(s);
}
#endif
/* END xz_dec_lzma2.c */
/* BEGIN xz_dec_stream.c */
// SPDX-License-Identifier: 0BSD

/*
 * .xz Stream decoder
 *
 * Author: Lasse Collin <lasse.collin@tukaani.org>
 */


#ifdef XZ_USE_CRC64
#	define IS_CRC64(check_type) ((check_type) == XZ_CHECK_CRC64)
#else
#	define IS_CRC64(check_type) false
#endif

#ifdef XZ_USE_SHA256
#	define IS_SHA256(check_type) ((check_type) == XZ_CHECK_SHA256)
#else
#	define IS_SHA256(check_type) false
#endif

/* Hash used to validate the Index field */
struct xz_dec_hash {
	vli_type unpadded;
	vli_type uncompressed;
	uint32_t crc32;
};

struct xz_dec {
	/* Position in dec_main() */
	enum {
		SEQ_STREAM_HEADER,
		SEQ_BLOCK_START,
		SEQ_BLOCK_HEADER,
		SEQ_BLOCK_UNCOMPRESS,
		SEQ_BLOCK_PADDING,
		SEQ_BLOCK_CHECK,
		SEQ_INDEX,
		SEQ_INDEX_PADDING,
		SEQ_INDEX_CRC32,
		SEQ_STREAM_FOOTER,
		SEQ_STREAM_PADDING
	} sequence;

	/* Position in variable-length integers and Check fields */
	uint32_t pos;

	/* Variable-length integer decoded by dec_vli() */
	vli_type vli;

	/* Saved in_pos and out_pos */
	size_t in_start;
	size_t out_start;

#ifdef XZ_USE_CRC64
	/* CRC32 or CRC64 value in Block or CRC32 value in Index */
	uint64_t crc;
#else
	/* CRC32 value in Block or Index */
	uint32_t crc;
#endif

	/* Type of the integrity check calculated from uncompressed data */
	enum xz_check check_type;

	/* Operation mode */
	enum xz_mode mode;

	/*
	 * True if the next call to xz_dec_run() is allowed to return
	 * XZ_BUF_ERROR.
	 */
	bool allow_buf_error;

	/* Information stored in Block Header */
	struct {
		/*
		 * Value stored in the Compressed Size field, or
		 * VLI_UNKNOWN if Compressed Size is not present.
		 */
		vli_type compressed;

		/*
		 * Value stored in the Uncompressed Size field, or
		 * VLI_UNKNOWN if Uncompressed Size is not present.
		 */
		vli_type uncompressed;

		/* Size of the Block Header field */
		uint32_t size;
	} block_header;

	/* Information collected when decoding Blocks */
	struct {
		/* Observed compressed size of the current Block */
		vli_type compressed;

		/* Observed uncompressed size of the current Block */
		vli_type uncompressed;

		/* Number of Blocks decoded so far */
		vli_type count;

		/*
		 * Hash calculated from the Block sizes. This is used to
		 * validate the Index field.
		 */
		struct xz_dec_hash hash;
	} block;

	/* Variables needed when verifying the Index field */
	struct {
		/* Position in dec_index() */
		enum {
			SEQ_INDEX_COUNT,
			SEQ_INDEX_UNPADDED,
			SEQ_INDEX_UNCOMPRESSED
		} sequence;

		/* Size of the Index in bytes */
		vli_type size;

		/* Number of Records (matches block.count in valid files) */
		vli_type count;

		/*
		 * Hash calculated from the Records (matches block.hash in
		 * valid files).
		 */
		struct xz_dec_hash hash;
	} index;

	/*
	 * Temporary buffer needed to hold Stream Header, Block Header,
	 * and Stream Footer. The Block Header is the biggest (1 KiB)
	 * so we reserve space according to that. buf[] has to be aligned
	 * to a multiple of four bytes; the size_t variables before it
	 * should guarantee this.
	 */
	struct {
		size_t pos;
		size_t size;
		uint8_t buf[1024];
	} temp;

	struct xz_dec_lzma2 *lzma2;

#ifdef XZ_DEC_BCJ
	struct xz_dec_bcj *bcj;
	bool bcj_active;
#endif

#ifdef XZ_USE_SHA256
	/*
	 * SHA-256 value in Block
	 *
	 * struct xz_sha256 is over a hundred bytes and it's only accessed
	 * from a few places. By putting the SHA-256 state near the end
	 * of struct xz_dec (somewhere after the "index" member) reduces
	 * code size at least on x86 and RISC-V. It's because the first bytes
	 * of the struct can be accessed with smaller instructions; the
	 * members that are accessed from many places should be at the top.
	 */
	struct xz_sha256 sha256;
#endif
};

#if defined(XZ_DEC_ANY_CHECK) || defined(XZ_USE_SHA256)
/* Sizes of the Check field with different Check IDs */
static const uint8_t check_sizes[16] = {
	0,
	4, 4, 4,
	8, 8, 8,
	16, 16, 16,
	32, 32, 32,
	64, 64, 64
};
#endif

/*
 * Fill s->temp by copying data starting from b->in[b->in_pos]. Caller
 * must have set s->temp.pos and s->temp.size to indicate how much data
 * we are supposed to copy into s->temp.buf. Return true once s->temp.pos
 * has reached s->temp.size.
 */
static bool fill_temp(struct xz_dec *s, struct xz_buf *b)
{
	size_t copy_size = min_t(size_t,
			b->in_size - b->in_pos, s->temp.size - s->temp.pos);

	memcpy(s->temp.buf + s->temp.pos, b->in + b->in_pos, copy_size);
	b->in_pos += copy_size;
	s->temp.pos += copy_size;

	if (s->temp.pos == s->temp.size) {
		s->temp.pos = 0;
		return true;
	}

	return false;
}

/* Decode a variable-length integer (little-endian base-128 encoding) */
static enum xz_ret dec_vli(struct xz_dec *s, const uint8_t *in,
			   size_t *in_pos, size_t in_size)
{
	uint8_t byte;

	if (s->pos == 0)
		s->vli = 0;

	while (*in_pos < in_size) {
		byte = in[*in_pos];
		++*in_pos;

		s->vli |= (vli_type)(byte & 0x7F) << s->pos;

		if ((byte & 0x80) == 0) {
			/* Don't allow non-minimal encodings. */
			if (byte == 0 && s->pos != 0)
				return XZ_DATA_ERROR;

			s->pos = 0;
			return XZ_STREAM_END;
		}

		s->pos += 7;
		if (s->pos == 7 * VLI_BYTES_MAX)
			return XZ_DATA_ERROR;
	}

	return XZ_OK;
}

/*
 * Decode the Compressed Data field from a Block. Update and validate
 * the observed compressed and uncompressed sizes of the Block so that
 * they don't exceed the values possibly stored in the Block Header
 * (validation assumes that no integer overflow occurs, since vli_type
 * is normally uint64_t). Update the CRC32 or CRC64 value if presence of
 * the CRC32 or CRC64 field was indicated in Stream Header.
 *
 * Once the decoding is finished, validate that the observed sizes match
 * the sizes possibly stored in the Block Header. Update the hash and
 * Block count, which are later used to validate the Index field.
 */
static enum xz_ret dec_block(struct xz_dec *s, struct xz_buf *b)
{
	enum xz_ret ret;

	s->in_start = b->in_pos;
	s->out_start = b->out_pos;

#ifdef XZ_DEC_BCJ
	if (s->bcj_active)
		ret = xz_dec_bcj_run(s->bcj, s->lzma2, b);
	else
#endif
		ret = xz_dec_lzma2_run(s->lzma2, b);

	s->block.compressed += b->in_pos - s->in_start;
	s->block.uncompressed += b->out_pos - s->out_start;

	/*
	 * There is no need to separately check for VLI_UNKNOWN, since
	 * the observed sizes are always smaller than VLI_UNKNOWN.
	 */
	if (s->block.compressed > s->block_header.compressed
			|| s->block.uncompressed
				> s->block_header.uncompressed)
		return XZ_DATA_ERROR;

	if (s->check_type == XZ_CHECK_CRC32)
		s->crc = xz_crc32(b->out + s->out_start,
				b->out_pos - s->out_start, s->crc);
#ifdef XZ_USE_CRC64
	else if (s->check_type == XZ_CHECK_CRC64)
		s->crc = xz_crc64(b->out + s->out_start,
				b->out_pos - s->out_start, s->crc);
#endif
#ifdef XZ_USE_SHA256
	else if (s->check_type == XZ_CHECK_SHA256)
		xz_sha256_update(b->out + s->out_start,
				b->out_pos - s->out_start, &s->sha256);
#endif

	if (ret == XZ_STREAM_END) {
		if (s->block_header.compressed != VLI_UNKNOWN
				&& s->block_header.compressed
					!= s->block.compressed)
			return XZ_DATA_ERROR;

		if (s->block_header.uncompressed != VLI_UNKNOWN
				&& s->block_header.uncompressed
					!= s->block.uncompressed)
			return XZ_DATA_ERROR;

		s->block.hash.unpadded += s->block_header.size
				+ s->block.compressed;

#if defined(XZ_DEC_ANY_CHECK) || defined(XZ_USE_SHA256)
		s->block.hash.unpadded += check_sizes[s->check_type];
#else
		if (s->check_type == XZ_CHECK_CRC32)
			s->block.hash.unpadded += 4;
		else if (IS_CRC64(s->check_type))
			s->block.hash.unpadded += 8;
#endif

		s->block.hash.uncompressed += s->block.uncompressed;
		s->block.hash.crc32 = xz_crc32(
				(const uint8_t *)&s->block.hash,
				sizeof(s->block.hash), s->block.hash.crc32);

		++s->block.count;
	}

	return ret;
}

/* Update the Index size and the CRC32 value. */
static void index_update(struct xz_dec *s, const struct xz_buf *b)
{
	size_t in_used = b->in_pos - s->in_start;
	s->index.size += in_used;
	s->crc = xz_crc32(b->in + s->in_start, in_used, s->crc);
}

/*
 * Decode the Number of Records, Unpadded Size, and Uncompressed Size
 * fields from the Index field. That is, Index Padding and CRC32 are not
 * decoded by this function.
 *
 * This can return XZ_OK (more input needed), XZ_STREAM_END (everything
 * successfully decoded), or XZ_DATA_ERROR (input is corrupt).
 */
static enum xz_ret dec_index(struct xz_dec *s, struct xz_buf *b)
{
	enum xz_ret ret;

	do {
		ret = dec_vli(s, b->in, &b->in_pos, b->in_size);
		if (ret != XZ_STREAM_END) {
			index_update(s, b);
			return ret;
		}

		switch (s->index.sequence) {
		case SEQ_INDEX_COUNT:
			s->index.count = s->vli;

			/*
			 * Validate that the Number of Records field
			 * indicates the same number of Records as
			 * there were Blocks in the Stream.
			 */
			if (s->index.count != s->block.count)
				return XZ_DATA_ERROR;

			s->index.sequence = SEQ_INDEX_UNPADDED;
			break;

		case SEQ_INDEX_UNPADDED:
			s->index.hash.unpadded += s->vli;
			s->index.sequence = SEQ_INDEX_UNCOMPRESSED;
			break;

		case SEQ_INDEX_UNCOMPRESSED:
			s->index.hash.uncompressed += s->vli;
			s->index.hash.crc32 = xz_crc32(
					(const uint8_t *)&s->index.hash,
					sizeof(s->index.hash),
					s->index.hash.crc32);
			--s->index.count;
			s->index.sequence = SEQ_INDEX_UNPADDED;
			break;
		}
	} while (s->index.count > 0);

	return XZ_STREAM_END;
}

/*
 * Validate that the next four or eight input bytes match the value
 * of s->crc. s->pos must be zero when starting to validate the first byte.
 * The "bits" argument allows using the same code for both CRC32 and CRC64.
 */
static enum xz_ret crc_validate(struct xz_dec *s, struct xz_buf *b,
				uint32_t bits)
{
	do {
		if (b->in_pos == b->in_size)
			return XZ_OK;

		if (((s->crc >> s->pos) & 0xFF) != b->in[b->in_pos++])
			return XZ_DATA_ERROR;

		s->pos += 8;

	} while (s->pos < bits);

	s->crc = 0;
	s->pos = 0;

	return XZ_STREAM_END;
}

#ifdef XZ_DEC_ANY_CHECK
/*
 * Skip over the Check field when the Check ID is not supported.
 * Returns true once the whole Check field has been skipped over.
 */
static bool check_skip(struct xz_dec *s, struct xz_buf *b)
{
	while (s->pos < check_sizes[s->check_type]) {
		if (b->in_pos == b->in_size)
			return false;

		++b->in_pos;
		++s->pos;
	}

	s->pos = 0;

	return true;
}
#endif

/* Decode the Stream Header field (the first 12 bytes of the .xz Stream). */
static enum xz_ret dec_stream_header(struct xz_dec *s)
{
	if (!memeq(s->temp.buf, HEADER_MAGIC, HEADER_MAGIC_SIZE))
		return XZ_FORMAT_ERROR;

	if (xz_crc32(s->temp.buf + HEADER_MAGIC_SIZE, 2, 0)
			!= get_le32(s->temp.buf + HEADER_MAGIC_SIZE + 2))
		return XZ_DATA_ERROR;

	if (s->temp.buf[HEADER_MAGIC_SIZE] != 0)
		return XZ_OPTIONS_ERROR;

	/*
	 * Of integrity checks, we support none (Check ID = 0),
	 * CRC32 (Check ID = 1), and optionally CRC64 (Check ID = 4).
	 * However, if XZ_DEC_ANY_CHECK is defined, we will accept other
	 * check types too, but then the check won't be verified and
	 * a warning (XZ_UNSUPPORTED_CHECK) will be given.
	 */
	if (s->temp.buf[HEADER_MAGIC_SIZE + 1] > XZ_CHECK_MAX)
		return XZ_OPTIONS_ERROR;

	s->check_type = s->temp.buf[HEADER_MAGIC_SIZE + 1];

	if (s->check_type > XZ_CHECK_CRC32 && !IS_CRC64(s->check_type)
			&& !IS_SHA256(s->check_type)) {
#ifdef XZ_DEC_ANY_CHECK
		return XZ_UNSUPPORTED_CHECK;
#else
		return XZ_OPTIONS_ERROR;
#endif
	}

	return XZ_OK;
}

/* Decode the Stream Footer field (the last 12 bytes of the .xz Stream) */
static enum xz_ret dec_stream_footer(struct xz_dec *s)
{
	if (!memeq(s->temp.buf + 10, FOOTER_MAGIC, FOOTER_MAGIC_SIZE))
		return XZ_DATA_ERROR;

	if (xz_crc32(s->temp.buf + 4, 6, 0) != get_le32(s->temp.buf))
		return XZ_DATA_ERROR;

	/*
	 * Validate Backward Size. Note that we never added the size of the
	 * Index CRC32 field to s->index.size, thus we use s->index.size / 4
	 * instead of s->index.size / 4 - 1.
	 */
	if ((s->index.size >> 2) != get_le32(s->temp.buf + 4))
		return XZ_DATA_ERROR;

	if (s->temp.buf[8] != 0 || s->temp.buf[9] != s->check_type)
		return XZ_DATA_ERROR;

	/*
	 * Use XZ_STREAM_END instead of XZ_OK to be more convenient
	 * for the caller.
	 */
	return XZ_STREAM_END;
}

/* Decode the Block Header and initialize the filter chain. */
static enum xz_ret dec_block_header(struct xz_dec *s)
{
	enum xz_ret ret;

	/*
	 * Validate the CRC32. We know that the temp buffer is at least
	 * eight bytes so this is safe.
	 */
	s->temp.size -= 4;
	if (xz_crc32(s->temp.buf, s->temp.size, 0)
			!= get_le32(s->temp.buf + s->temp.size))
		return XZ_DATA_ERROR;

	s->temp.pos = 2;

	/*
	 * Catch unsupported Block Flags. We support only one or two filters
	 * in the chain, so we catch that with the same test.
	 */
#ifdef XZ_DEC_BCJ
	if (s->temp.buf[1] & 0x3E)
#else
	if (s->temp.buf[1] & 0x3F)
#endif
		return XZ_OPTIONS_ERROR;

	/* Compressed Size */
	if (s->temp.buf[1] & 0x40) {
		if (dec_vli(s, s->temp.buf, &s->temp.pos, s->temp.size)
					!= XZ_STREAM_END)
			return XZ_DATA_ERROR;

		s->block_header.compressed = s->vli;
	} else {
		s->block_header.compressed = VLI_UNKNOWN;
	}

	/* Uncompressed Size */
	if (s->temp.buf[1] & 0x80) {
		if (dec_vli(s, s->temp.buf, &s->temp.pos, s->temp.size)
				!= XZ_STREAM_END)
			return XZ_DATA_ERROR;

		s->block_header.uncompressed = s->vli;
	} else {
		s->block_header.uncompressed = VLI_UNKNOWN;
	}

#ifdef XZ_DEC_BCJ
	/* If there are two filters, the first one must be a BCJ filter. */
	s->bcj_active = s->temp.buf[1] & 0x01;
	if (s->bcj_active) {
		if (s->temp.size - s->temp.pos < 2)
			return XZ_OPTIONS_ERROR;

		ret = xz_dec_bcj_reset(s->bcj, s->temp.buf[s->temp.pos++]);
		if (ret != XZ_OK)
			return ret;

		/*
		 * We don't support custom start offset,
		 * so Size of Properties must be zero.
		 */
		if (s->temp.buf[s->temp.pos++] != 0x00)
			return XZ_OPTIONS_ERROR;
	}
#endif

	/* Valid Filter Flags always take at least two bytes. */
	if (s->temp.size - s->temp.pos < 2)
		return XZ_DATA_ERROR;

	/* Filter ID = LZMA2 */
	if (s->temp.buf[s->temp.pos++] != 0x21)
		return XZ_OPTIONS_ERROR;

	/* Size of Properties = 1-byte Filter Properties */
	if (s->temp.buf[s->temp.pos++] != 0x01)
		return XZ_OPTIONS_ERROR;

	/* Filter Properties contains LZMA2 dictionary size. */
	if (s->temp.size - s->temp.pos < 1)
		return XZ_DATA_ERROR;

	ret = xz_dec_lzma2_reset(s->lzma2, s->temp.buf[s->temp.pos++]);
	if (ret != XZ_OK)
		return ret;

	/* The rest must be Header Padding. */
	while (s->temp.pos < s->temp.size)
		if (s->temp.buf[s->temp.pos++] != 0x00)
			return XZ_OPTIONS_ERROR;

	s->temp.pos = 0;
	s->block.compressed = 0;
	s->block.uncompressed = 0;

	return XZ_OK;
}

static enum xz_ret dec_main(struct xz_dec *s, struct xz_buf *b)
{
	enum xz_ret ret;

	/*
	 * Store the start position for the case when we are in the middle
	 * of the Index field.
	 */
	s->in_start = b->in_pos;

	while (true) {
		switch (s->sequence) {
		case SEQ_STREAM_HEADER:
			/*
			 * Stream Header is copied to s->temp, and then
			 * decoded from there. This way if the caller
			 * gives us only little input at a time, we can
			 * still keep the Stream Header decoding code
			 * simple. Similar approach is used in many places
			 * in this file.
			 */
			if (!fill_temp(s, b))
				return XZ_OK;

			/*
			 * If dec_stream_header() returns
			 * XZ_UNSUPPORTED_CHECK, it is still possible
			 * to continue decoding if working in multi-call
			 * mode. Thus, update s->sequence before calling
			 * dec_stream_header().
			 */
			s->sequence = SEQ_BLOCK_START;

			ret = dec_stream_header(s);
			if (ret != XZ_OK)
				return ret;

			fallthrough;

		case SEQ_BLOCK_START:
			/* We need one byte of input to continue. */
			if (b->in_pos == b->in_size)
				return XZ_OK;

			/* See if this is the beginning of the Index field. */
			if (b->in[b->in_pos] == 0) {
				s->in_start = b->in_pos++;
				s->sequence = SEQ_INDEX;
				break;
			}

			/*
			 * Calculate the size of the Block Header and
			 * prepare to decode it.
			 */
			s->block_header.size
				= ((uint32_t)b->in[b->in_pos] + 1) * 4;

			s->temp.size = s->block_header.size;
			s->temp.pos = 0;
			s->sequence = SEQ_BLOCK_HEADER;

			fallthrough;

		case SEQ_BLOCK_HEADER:
			if (!fill_temp(s, b))
				return XZ_OK;

			ret = dec_block_header(s);
			if (ret != XZ_OK)
				return ret;

#ifdef XZ_USE_SHA256
			if (s->check_type == XZ_CHECK_SHA256)
				xz_sha256_reset(&s->sha256);
#endif

			s->sequence = SEQ_BLOCK_UNCOMPRESS;

			fallthrough;

		case SEQ_BLOCK_UNCOMPRESS:
			ret = dec_block(s, b);
			if (ret != XZ_STREAM_END)
				return ret;

			s->sequence = SEQ_BLOCK_PADDING;

			fallthrough;

		case SEQ_BLOCK_PADDING:
			/*
			 * Size of Compressed Data + Block Padding
			 * must be a multiple of four. We don't need
			 * s->block.compressed for anything else
			 * anymore, so we use it here to test the size
			 * of the Block Padding field.
			 */
			while (s->block.compressed & 3) {
				if (b->in_pos == b->in_size)
					return XZ_OK;

				if (b->in[b->in_pos++] != 0)
					return XZ_DATA_ERROR;

				++s->block.compressed;
			}

			s->sequence = SEQ_BLOCK_CHECK;

			fallthrough;

		case SEQ_BLOCK_CHECK:
			if (s->check_type == XZ_CHECK_CRC32) {
				ret = crc_validate(s, b, 32);
				if (ret != XZ_STREAM_END)
					return ret;
			}
			else if (IS_CRC64(s->check_type)) {
				ret = crc_validate(s, b, 64);
				if (ret != XZ_STREAM_END)
					return ret;
			}
#ifdef XZ_USE_SHA256
			else if (s->check_type == XZ_CHECK_SHA256) {
				s->temp.size = 32;
				if (!fill_temp(s, b))
					return XZ_OK;

				if (!xz_sha256_validate(s->temp.buf,
							&s->sha256))
					return XZ_DATA_ERROR;

				s->pos = 0;
			}
#endif
#ifdef XZ_DEC_ANY_CHECK
			else if (!check_skip(s, b)) {
				return XZ_OK;
			}
#endif

			s->sequence = SEQ_BLOCK_START;
			break;

		case SEQ_INDEX:
			ret = dec_index(s, b);
			if (ret != XZ_STREAM_END)
				return ret;

			s->sequence = SEQ_INDEX_PADDING;

			fallthrough;

		case SEQ_INDEX_PADDING:
			while ((s->index.size + (b->in_pos - s->in_start))
					& 3) {
				if (b->in_pos == b->in_size) {
					index_update(s, b);
					return XZ_OK;
				}

				if (b->in[b->in_pos++] != 0)
					return XZ_DATA_ERROR;
			}

			/* Finish the CRC32 value and Index size. */
			index_update(s, b);

			/* Compare the hashes to validate the Index field. */
			if (!memeq(&s->block.hash, &s->index.hash,
					sizeof(s->block.hash)))
				return XZ_DATA_ERROR;

			s->sequence = SEQ_INDEX_CRC32;

			fallthrough;

		case SEQ_INDEX_CRC32:
			ret = crc_validate(s, b, 32);
			if (ret != XZ_STREAM_END)
				return ret;

			s->temp.size = STREAM_HEADER_SIZE;
			s->sequence = SEQ_STREAM_FOOTER;

			fallthrough;

		case SEQ_STREAM_FOOTER:
			if (!fill_temp(s, b))
				return XZ_OK;

			return dec_stream_footer(s);

		case SEQ_STREAM_PADDING:
			/* Never reached, only silencing a warning */
			break;
		}
	}

	/* Never reached */
}

/*
 * xz_dec_run() is a wrapper for dec_main() to handle some special cases in
 * multi-call and single-call decoding.
 *
 * In multi-call mode, we must return XZ_BUF_ERROR when it seems clear that we
 * are not going to make any progress anymore. This is to prevent the caller
 * from calling us infinitely when the input file is truncated or otherwise
 * corrupt. Since zlib-style API allows that the caller fills the input buffer
 * only when the decoder doesn't produce any new output, we have to be careful
 * to avoid returning XZ_BUF_ERROR too easily: XZ_BUF_ERROR is returned only
 * after the second consecutive call to xz_dec_run() that makes no progress.
 *
 * In single-call mode, if we couldn't decode everything and no error
 * occurred, either the input is truncated or the output buffer is too small.
 * Since we know that the last input byte never produces any output, we know
 * that if all the input was consumed and decoding wasn't finished, the file
 * must be corrupt. Otherwise the output buffer has to be too small or the
 * file is corrupt in a way that decoding it produces too big output.
 *
 * If single-call decoding fails, we reset b->in_pos and b->out_pos back to
 * their original values. This is because with some filter chains there won't
 * be any valid uncompressed data in the output buffer unless the decoding
 * actually succeeds (that's the price to pay of using the output buffer as
 * the workspace).
 */
XZ_EXTERN enum xz_ret xz_dec_run(struct xz_dec *s, struct xz_buf *b)
{
	size_t in_start;
	size_t out_start;
	enum xz_ret ret;

	if (DEC_IS_SINGLE(s->mode))
		xz_dec_reset(s);

	in_start = b->in_pos;
	out_start = b->out_pos;
	ret = dec_main(s, b);

	if (DEC_IS_SINGLE(s->mode)) {
		if (ret == XZ_OK)
			ret = b->in_pos == b->in_size
					? XZ_DATA_ERROR : XZ_BUF_ERROR;

		if (ret != XZ_STREAM_END) {
			b->in_pos = in_start;
			b->out_pos = out_start;
		}

	} else if (ret == XZ_OK && in_start == b->in_pos
			&& out_start == b->out_pos) {
		if (s->allow_buf_error)
			ret = XZ_BUF_ERROR;

		s->allow_buf_error = true;
	} else {
		s->allow_buf_error = false;
	}

	return ret;
}

#ifdef XZ_DEC_CONCATENATED
XZ_EXTERN enum xz_ret xz_dec_catrun(struct xz_dec *s, struct xz_buf *b,
				    int finish)
{
	enum xz_ret ret;

	if (DEC_IS_SINGLE(s->mode)) {
		xz_dec_reset(s);
		finish = true;
	}

	while (true) {
		if (s->sequence == SEQ_STREAM_PADDING) {
			/*
			 * Skip Stream Padding. Its size must be a multiple
			 * of four bytes which is tracked with s->pos.
			 */
			while (true) {
				if (b->in_pos == b->in_size) {
					/*
					 * Note that if we are repeatedly
					 * given no input and finish is false,
					 * we will keep returning XZ_OK even
					 * though no progress is being made.
					 * The lack of XZ_BUF_ERROR support
					 * isn't a problem here because a
					 * reasonable caller will eventually
					 * provide more input or set finish
					 * to true.
					 */
					if (!finish)
						return XZ_OK;

					if (s->pos != 0)
						return XZ_DATA_ERROR;

					return XZ_STREAM_END;
				}

				if (b->in[b->in_pos] != 0x00) {
					if (s->pos != 0)
						return XZ_DATA_ERROR;

					break;
				}

				++b->in_pos;
				s->pos = (s->pos + 1) & 3;
			}

			/*
			 * More input remains. It should be a new Stream.
			 *
			 * In single-call mode xz_dec_run() will always call
			 * xz_dec_reset(). Thus, we need to do it here only
			 * in multi-call mode.
			 */
			if (DEC_IS_MULTI(s->mode))
				xz_dec_reset(s);
		}

		ret = xz_dec_run(s, b);

		if (ret != XZ_STREAM_END)
			break;

		s->sequence = SEQ_STREAM_PADDING;
	}

	return ret;
}
#endif

XZ_EXTERN struct xz_dec *xz_dec_init(enum xz_mode mode, uint32_t dict_max)
{
	struct xz_dec *s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (s == NULL)
		return NULL;

	s->mode = mode;

#ifdef XZ_DEC_BCJ
	s->bcj = xz_dec_bcj_create(DEC_IS_SINGLE(mode));
	if (s->bcj == NULL)
		goto error_bcj;
#endif

	s->lzma2 = xz_dec_lzma2_create(mode, dict_max);
	if (s->lzma2 == NULL)
		goto error_lzma2;

	xz_dec_reset(s);
	return s;

error_lzma2:
#ifdef XZ_DEC_BCJ
	xz_dec_bcj_end(s->bcj);
error_bcj:
#endif
	kfree(s);
	return NULL;
}

XZ_EXTERN void xz_dec_reset(struct xz_dec *s)
{
	s->sequence = SEQ_STREAM_HEADER;
	s->allow_buf_error = false;
	s->pos = 0;
	s->crc = 0;
	memzero(&s->block, sizeof(s->block));
	memzero(&s->index, sizeof(s->index));
	s->temp.pos = 0;
	s->temp.size = STREAM_HEADER_SIZE;
}

XZ_EXTERN void xz_dec_end(struct xz_dec *s)
{
	if (s != NULL) {
		xz_dec_lzma2_end(s->lzma2);
#ifdef XZ_DEC_BCJ
		xz_dec_bcj_end(s->bcj);
#endif
		kfree(s);
	}
}
/* END xz_dec_stream.c */
/* BEGIN xz_dec_bcj.c */
// SPDX-License-Identifier: 0BSD

/*
 * Branch/Call/Jump (BCJ) filter decoders
 *
 * Authors: Lasse Collin <lasse.collin@tukaani.org>
 *          Igor Pavlov <https://7-zip.org/>
 */


/*
 * The rest of the file is inside this ifdef. It makes things a little more
 * convenient when building without support for any BCJ filters.
 */
#ifdef XZ_DEC_BCJ

struct xz_dec_bcj {
	/* Type of the BCJ filter being used */
	enum {
		BCJ_X86 = 4,        /* x86 or x86-64 */
		BCJ_POWERPC = 5,    /* Big endian only */
		BCJ_IA64 = 6,       /* Big or little endian */
		BCJ_ARM = 7,        /* Little endian only */
		BCJ_ARMTHUMB = 8,   /* Little endian only */
		BCJ_SPARC = 9,      /* Big or little endian */
		BCJ_ARM64 = 10,     /* AArch64 */
		BCJ_RISCV = 11      /* RV32GQC_Zfh, RV64GQC_Zfh */
	} type;

	/*
	 * Return value of the next filter in the chain. We need to preserve
	 * this information across calls, because we must not call the next
	 * filter anymore once it has returned XZ_STREAM_END.
	 */
	enum xz_ret ret;

	/* True if we are operating in single-call mode. */
	bool single_call;

	/*
	 * Absolute position relative to the beginning of the uncompressed
	 * data (in a single .xz Block). We care only about the lowest 32
	 * bits so this doesn't need to be uint64_t even with big files.
	 */
	uint32_t pos;

	/* x86 filter state */
	uint32_t x86_prev_mask;

	/* Temporary space to hold the variables from struct xz_buf */
	uint8_t *out;
	size_t out_pos;
	size_t out_size;

	struct {
		/* Amount of already filtered data in the beginning of buf */
		size_t filtered;

		/* Total amount of data currently stored in buf  */
		size_t size;

		/*
		 * Buffer to hold a mix of filtered and unfiltered data. This
		 * needs to be big enough to hold Alignment + 2 * Look-ahead:
		 *
		 * Type         Alignment   Look-ahead
		 * x86              1           4
		 * PowerPC          4           0
		 * IA-64           16           0
		 * ARM              4           0
		 * ARM-Thumb        2           2
		 * SPARC            4           0
		 */
		uint8_t buf[16];
	} temp;
};

#ifdef XZ_DEC_X86
/*
 * This is used to test the most significant byte of a memory address
 * in an x86 instruction.
 */
static inline int bcj_x86_test_msbyte(uint8_t b)
{
	return b == 0x00 || b == 0xFF;
}

static size_t bcj_x86(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	static const bool mask_to_allowed_status[8]
		= { true, true, true, false, true, false, false, false };

	static const uint8_t mask_to_bit_num[8] = { 0, 1, 2, 2, 3, 3, 3, 3 };

	size_t i;
	size_t prev_pos = (size_t)-1;
	uint32_t prev_mask = s->x86_prev_mask;
	uint32_t src;
	uint32_t dest;
	uint32_t j;
	uint8_t b;

	if (size <= 4)
		return 0;

	size -= 4;
	for (i = 0; i < size; ++i) {
		if ((buf[i] & 0xFE) != 0xE8)
			continue;

		prev_pos = i - prev_pos;
		if (prev_pos > 3) {
			prev_mask = 0;
		} else {
			prev_mask = (prev_mask << (prev_pos - 1)) & 7;
			if (prev_mask != 0) {
				b = buf[i + 4 - mask_to_bit_num[prev_mask]];
				if (!mask_to_allowed_status[prev_mask]
						|| bcj_x86_test_msbyte(b)) {
					prev_pos = i;
					prev_mask = (prev_mask << 1) | 1;
					continue;
				}
			}
		}

		prev_pos = i;

		if (bcj_x86_test_msbyte(buf[i + 4])) {
			src = get_unaligned_le32(buf + i + 1);
			while (true) {
				dest = src - (s->pos + (uint32_t)i + 5);
				if (prev_mask == 0)
					break;

				j = mask_to_bit_num[prev_mask] * 8;
				b = (uint8_t)(dest >> (24 - j));
				if (!bcj_x86_test_msbyte(b))
					break;

				src = dest ^ (((uint32_t)1 << (32 - j)) - 1);
			}

			dest &= 0x01FFFFFF;
			dest |= (uint32_t)0 - (dest & 0x01000000);
			put_unaligned_le32(dest, buf + i + 1);
			i += 4;
		} else {
			prev_mask = (prev_mask << 1) | 1;
		}
	}

	prev_pos = i - prev_pos;
	s->x86_prev_mask = prev_pos > 3 ? 0 : prev_mask << (prev_pos - 1);
	return i;
}
#endif

#ifdef XZ_DEC_POWERPC
static size_t bcj_powerpc(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t instr;

	size &= ~(size_t)3;

	for (i = 0; i < size; i += 4) {
		instr = get_unaligned_be32(buf + i);
		if ((instr & 0xFC000003) == 0x48000001) {
			instr &= 0x03FFFFFC;
			instr -= s->pos + (uint32_t)i;
			instr &= 0x03FFFFFC;
			instr |= 0x48000001;
			put_unaligned_be32(instr, buf + i);
		}
	}

	return i;
}
#endif

#ifdef XZ_DEC_IA64
static size_t bcj_ia64(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	static const uint8_t branch_table[32] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		4, 4, 6, 6, 0, 0, 7, 7,
		4, 4, 0, 0, 4, 4, 0, 0
	};

	/*
	 * The local variables take a little bit stack space, but it's less
	 * than what LZMA2 decoder takes, so it doesn't make sense to reduce
	 * stack usage here without doing that for the LZMA2 decoder too.
	 */

	/* Loop counters */
	size_t i;
	size_t j;

	/* Instruction slot (0, 1, or 2) in the 128-bit instruction word */
	uint32_t slot;

	/* Bitwise offset of the instruction indicated by slot */
	uint32_t bit_pos;

	/* bit_pos split into byte and bit parts */
	uint32_t byte_pos;
	uint32_t bit_res;

	/* Address part of an instruction */
	uint32_t addr;

	/* Mask used to detect which instructions to convert */
	uint32_t mask;

	/* 41-bit instruction stored somewhere in the lowest 48 bits */
	uint64_t instr;

	/* Instruction normalized with bit_res for easier manipulation */
	uint64_t norm;

	size &= ~(size_t)15;

	for (i = 0; i < size; i += 16) {
		mask = branch_table[buf[i] & 0x1F];
		for (slot = 0, bit_pos = 5; slot < 3; ++slot, bit_pos += 41) {
			if (((mask >> slot) & 1) == 0)
				continue;

			byte_pos = bit_pos >> 3;
			bit_res = bit_pos & 7;
			instr = 0;
			for (j = 0; j < 6; ++j)
				instr |= (uint64_t)(buf[i + j + byte_pos])
						<< (8 * j);

			norm = instr >> bit_res;

			if (((norm >> 37) & 0x0F) == 0x05
					&& ((norm >> 9) & 0x07) == 0) {
				addr = (norm >> 13) & 0x0FFFFF;
				addr |= ((uint32_t)(norm >> 36) & 1) << 20;
				addr <<= 4;
				addr -= s->pos + (uint32_t)i;
				addr >>= 4;

				norm &= ~((uint64_t)0x8FFFFF << 13);
				norm |= (uint64_t)(addr & 0x0FFFFF) << 13;
				norm |= (uint64_t)(addr & 0x100000)
						<< (36 - 20);

				instr &= (1 << bit_res) - 1;
				instr |= norm << bit_res;

				for (j = 0; j < 6; j++)
					buf[i + j + byte_pos]
						= (uint8_t)(instr >> (8 * j));
			}
		}
	}

	return i;
}
#endif

#ifdef XZ_DEC_ARM
static size_t bcj_arm(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t addr;

	size &= ~(size_t)3;

	for (i = 0; i < size; i += 4) {
		if (buf[i + 3] == 0xEB) {
			addr = (uint32_t)buf[i] | ((uint32_t)buf[i + 1] << 8)
					| ((uint32_t)buf[i + 2] << 16);
			addr <<= 2;
			addr -= s->pos + (uint32_t)i + 8;
			addr >>= 2;
			buf[i] = (uint8_t)addr;
			buf[i + 1] = (uint8_t)(addr >> 8);
			buf[i + 2] = (uint8_t)(addr >> 16);
		}
	}

	return i;
}
#endif

#ifdef XZ_DEC_ARMTHUMB
static size_t bcj_armthumb(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t addr;

	if (size < 4)
		return 0;

	size -= 4;

	for (i = 0; i <= size; i += 2) {
		if ((buf[i + 1] & 0xF8) == 0xF0
				&& (buf[i + 3] & 0xF8) == 0xF8) {
			addr = (((uint32_t)buf[i + 1] & 0x07) << 19)
					| ((uint32_t)buf[i] << 11)
					| (((uint32_t)buf[i + 3] & 0x07) << 8)
					| (uint32_t)buf[i + 2];
			addr <<= 1;
			addr -= s->pos + (uint32_t)i + 4;
			addr >>= 1;
			buf[i + 1] = (uint8_t)(0xF0 | ((addr >> 19) & 0x07));
			buf[i] = (uint8_t)(addr >> 11);
			buf[i + 3] = (uint8_t)(0xF8 | ((addr >> 8) & 0x07));
			buf[i + 2] = (uint8_t)addr;
			i += 2;
		}
	}

	return i;
}
#endif

#ifdef XZ_DEC_SPARC
static size_t bcj_sparc(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t instr;

	size &= ~(size_t)3;

	for (i = 0; i < size; i += 4) {
		instr = get_unaligned_be32(buf + i);
		if ((instr >> 22) == 0x100 || (instr >> 22) == 0x1FF) {
			instr <<= 2;
			instr -= s->pos + (uint32_t)i;
			instr >>= 2;
			instr = ((uint32_t)0x40000000 - (instr & 0x400000))
					| 0x40000000 | (instr & 0x3FFFFF);
			put_unaligned_be32(instr, buf + i);
		}
	}

	return i;
}
#endif

#ifdef XZ_DEC_ARM64
static size_t bcj_arm64(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t instr;
	uint32_t addr;

	size &= ~(size_t)3;

	for (i = 0; i < size; i += 4) {
		instr = get_unaligned_le32(buf + i);

		if ((instr >> 26) == 0x25) {
			/* BL instruction */
			addr = instr - ((s->pos + (uint32_t)i) >> 2);
			instr = 0x94000000 | (addr & 0x03FFFFFF);
			put_unaligned_le32(instr, buf + i);

		} else if ((instr & 0x9F000000) == 0x90000000) {
			/* ADRP instruction */
			addr = ((instr >> 29) & 3) | ((instr >> 3) & 0x1FFFFC);

			/* Only convert values in the range +/-512 MiB. */
			if ((addr + 0x020000) & 0x1C0000)
				continue;

			addr -= (s->pos + (uint32_t)i) >> 12;

			instr &= 0x9000001F;
			instr |= (addr & 3) << 29;
			instr |= (addr & 0x03FFFC) << 3;
			instr |= (0U - (addr & 0x020000)) & 0xE00000;

			put_unaligned_le32(instr, buf + i);
		}
	}

	return i;
}
#endif

#ifdef XZ_DEC_RISCV
static size_t bcj_riscv(struct xz_dec_bcj *s, uint8_t *buf, size_t size)
{
	size_t i;
	uint32_t b1;
	uint32_t b2;
	uint32_t b3;
	uint32_t instr;
	uint32_t instr2;
	uint32_t instr2_rs1;
	uint32_t addr;

	if (size < 8)
		return 0;

	size -= 8;

	for (i = 0; i <= size; i += 2) {
		instr = buf[i];

		if (instr == 0xEF) {
			/* JAL */
			b1 = buf[i + 1];
			if ((b1 & 0x0D) != 0)
				continue;

			b2 = buf[i + 2];
			b3 = buf[i + 3];

			addr = ((b1 & 0xF0) << 13) | (b2 << 9) | (b3 << 1);
			addr -= s->pos + (uint32_t)i;

			buf[i + 1] = (uint8_t)((b1 & 0x0F)
					| ((addr >> 8) & 0xF0));

			buf[i + 2] = (uint8_t)(((addr >> 16) & 0x0F)
					| ((addr >> 7) & 0x10)
					| ((addr << 4) & 0xE0));

			buf[i + 3] = (uint8_t)(((addr >> 4) & 0x7F)
					| ((addr >> 13) & 0x80));

			i += 4 - 2;

		} else if ((instr & 0x7F) == 0x17) {
			/* AUIPC */
			instr |= (uint32_t)buf[i + 1] << 8;
			instr |= (uint32_t)buf[i + 2] << 16;
			instr |= (uint32_t)buf[i + 3] << 24;

			if (instr & 0xE80) {
				/* AUIPC's rd doesn't equal x0 or x2. */
				instr2 = get_unaligned_le32(buf + i + 4);

				if (((instr << 8) ^ (instr2 - 3)) & 0xF8003) {
					i += 6 - 2;
					continue;
				}

				addr = (instr & 0xFFFFF000) + (instr2 >> 20);

				instr = 0x17 | (2 << 7) | (instr2 << 12);
				instr2 = addr;
			} else {
				/* AUIPC's rd equals x0 or x2. */
				instr2_rs1 = instr >> 27;

				if ((uint32_t)((instr - 0x3117) << 18)
						>= (instr2_rs1 & 0x1D)) {
					i += 4 - 2;
					continue;
				}

				addr = get_unaligned_be32(buf + i + 4);
				addr -= s->pos + (uint32_t)i;

				instr2 = (instr >> 12) | (addr << 20);

				instr = 0x17 | (instr2_rs1 << 7)
					| ((addr + 0x800) & 0xFFFFF000);
			}

			put_unaligned_le32(instr, buf + i);
			put_unaligned_le32(instr2, buf + i + 4);

			i += 8 - 2;
		}
	}

	return i;
}
#endif

/*
 * Apply the selected BCJ filter. Update *pos and s->pos to match the amount
 * of data that got filtered.
 *
 * NOTE: This is implemented as a switch statement to avoid using function
 * pointers, which could be problematic in the kernel boot code, which must
 * avoid pointers to static data (at least on x86).
 */
static void bcj_apply(struct xz_dec_bcj *s,
		      uint8_t *buf, size_t *pos, size_t size)
{
	size_t filtered;

	buf += *pos;
	size -= *pos;

	switch (s->type) {
#ifdef XZ_DEC_X86
	case BCJ_X86:
		filtered = bcj_x86(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_POWERPC
	case BCJ_POWERPC:
		filtered = bcj_powerpc(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_IA64
	case BCJ_IA64:
		filtered = bcj_ia64(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_ARM
	case BCJ_ARM:
		filtered = bcj_arm(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_ARMTHUMB
	case BCJ_ARMTHUMB:
		filtered = bcj_armthumb(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_SPARC
	case BCJ_SPARC:
		filtered = bcj_sparc(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_ARM64
	case BCJ_ARM64:
		filtered = bcj_arm64(s, buf, size);
		break;
#endif
#ifdef XZ_DEC_RISCV
	case BCJ_RISCV:
		filtered = bcj_riscv(s, buf, size);
		break;
#endif
	default:
		/* Never reached but silence compiler warnings. */
		filtered = 0;
		break;
	}

	*pos += filtered;
	s->pos += filtered;
}

/*
 * Flush pending filtered data from temp to the output buffer.
 * Move the remaining mixture of possibly filtered and unfiltered
 * data to the beginning of temp.
 */
static void bcj_flush(struct xz_dec_bcj *s, struct xz_buf *b)
{
	size_t copy_size;

	copy_size = min_t(size_t, s->temp.filtered, b->out_size - b->out_pos);
	memcpy(b->out + b->out_pos, s->temp.buf, copy_size);
	b->out_pos += copy_size;

	s->temp.filtered -= copy_size;
	s->temp.size -= copy_size;
	memmove(s->temp.buf, s->temp.buf + copy_size, s->temp.size);
}

/*
 * The BCJ filter functions are primitive in sense that they process the
 * data in chunks of 1-16 bytes. To hide this issue, this function does
 * some buffering.
 */
XZ_EXTERN enum xz_ret xz_dec_bcj_run(struct xz_dec_bcj *s,
				     struct xz_dec_lzma2 *lzma2,
				     struct xz_buf *b)
{
	size_t out_start;

	/*
	 * Flush pending already filtered data to the output buffer. Return
	 * immediately if we couldn't flush everything, or if the next
	 * filter in the chain had already returned XZ_STREAM_END.
	 */
	if (s->temp.filtered > 0) {
		bcj_flush(s, b);
		if (s->temp.filtered > 0)
			return XZ_OK;

		if (s->ret == XZ_STREAM_END)
			return XZ_STREAM_END;
	}

	/*
	 * If we have more output space than what is currently pending in
	 * temp, copy the unfiltered data from temp to the output buffer
	 * and try to fill the output buffer by decoding more data from the
	 * next filter in the chain. Apply the BCJ filter on the new data
	 * in the output buffer. If everything cannot be filtered, copy it
	 * to temp and rewind the output buffer position accordingly.
	 *
	 * This needs to be always run when temp.size == 0 to handle a special
	 * case where the output buffer is full and the next filter has no
	 * more output coming but hasn't returned XZ_STREAM_END yet.
	 */
	if (s->temp.size < b->out_size - b->out_pos || s->temp.size == 0) {
		out_start = b->out_pos;
		memcpy(b->out + b->out_pos, s->temp.buf, s->temp.size);
		b->out_pos += s->temp.size;

		s->ret = xz_dec_lzma2_run(lzma2, b);
		if (s->ret != XZ_STREAM_END
				&& (s->ret != XZ_OK || s->single_call))
			return s->ret;

		bcj_apply(s, b->out, &out_start, b->out_pos);

		/*
		 * As an exception, if the next filter returned XZ_STREAM_END,
		 * we can do that too, since the last few bytes that remain
		 * unfiltered are meant to remain unfiltered.
		 */
		if (s->ret == XZ_STREAM_END)
			return XZ_STREAM_END;

		s->temp.size = b->out_pos - out_start;
		b->out_pos -= s->temp.size;
		memcpy(s->temp.buf, b->out + b->out_pos, s->temp.size);

		/*
		 * If there wasn't enough input to the next filter to fill
		 * the output buffer with unfiltered data, there's no point
		 * to try decoding more data to temp.
		 */
		if (b->out_pos + s->temp.size < b->out_size)
			return XZ_OK;
	}

	/*
	 * We have unfiltered data in temp. If the output buffer isn't full
	 * yet, try to fill the temp buffer by decoding more data from the
	 * next filter. Apply the BCJ filter on temp. Then we hopefully can
	 * fill the actual output buffer by copying filtered data from temp.
	 * A mix of filtered and unfiltered data may be left in temp; it will
	 * be taken care on the next call to this function.
	 */
	if (b->out_pos < b->out_size) {
		/* Make b->out{,_pos,_size} temporarily point to s->temp. */
		s->out = b->out;
		s->out_pos = b->out_pos;
		s->out_size = b->out_size;
		b->out = s->temp.buf;
		b->out_pos = s->temp.size;
		b->out_size = sizeof(s->temp.buf);

		s->ret = xz_dec_lzma2_run(lzma2, b);

		s->temp.size = b->out_pos;
		b->out = s->out;
		b->out_pos = s->out_pos;
		b->out_size = s->out_size;

		if (s->ret != XZ_OK && s->ret != XZ_STREAM_END)
			return s->ret;

		bcj_apply(s, s->temp.buf, &s->temp.filtered, s->temp.size);

		/*
		 * If the next filter returned XZ_STREAM_END, we mark that
		 * everything is filtered, since the last unfiltered bytes
		 * of the stream are meant to be left as is.
		 */
		if (s->ret == XZ_STREAM_END)
			s->temp.filtered = s->temp.size;

		bcj_flush(s, b);
		if (s->temp.filtered > 0)
			return XZ_OK;
	}

	return s->ret;
}

XZ_EXTERN struct xz_dec_bcj *xz_dec_bcj_create(bool single_call)
{
	struct xz_dec_bcj *s = kmalloc(sizeof(*s), GFP_KERNEL);
	if (s != NULL)
		s->single_call = single_call;

	return s;
}

XZ_EXTERN enum xz_ret xz_dec_bcj_reset(struct xz_dec_bcj *s, uint8_t id)
{
	switch (id) {
#ifdef XZ_DEC_X86
	case BCJ_X86:
#endif
#ifdef XZ_DEC_POWERPC
	case BCJ_POWERPC:
#endif
#ifdef XZ_DEC_IA64
	case BCJ_IA64:
#endif
#ifdef XZ_DEC_ARM
	case BCJ_ARM:
#endif
#ifdef XZ_DEC_ARMTHUMB
	case BCJ_ARMTHUMB:
#endif
#ifdef XZ_DEC_SPARC
	case BCJ_SPARC:
#endif
#ifdef XZ_DEC_ARM64
	case BCJ_ARM64:
#endif
#ifdef XZ_DEC_RISCV
	case BCJ_RISCV:
#endif
		break;

	default:
		/* Unsupported Filter ID */
		return XZ_OPTIONS_ERROR;
	}

	s->type = id;
	s->ret = XZ_OK;
	s->pos = 0;
	s->x86_prev_mask = 0;
	s->temp.filtered = 0;
	s->temp.size = 0;

	return XZ_OK;
}

#endif
/* END xz_dec_bcj.c */

/* Convenience wrapper for ordinary userspace applications. */
int xz_embedded_decompress(const void *input, size_t input_size,
                           void *output, size_t output_capacity,
                           size_t *output_size)
{
    struct xz_buf b;
    struct xz_dec *s;
    enum xz_ret ret;

    if (input == NULL || output == NULL || output_size == NULL)
        return -1;

    *output_size = 0;
    xz_crc32_init();
    xz_crc64_init();
    s = xz_dec_init(XZ_DYNALLOC, 64U << 20);
    if (s == NULL)
        return -2;

    b.in = (const uint8_t *)input;
    b.in_pos = 0;
    b.in_size = input_size;
    b.out = (uint8_t *)output;
    b.out_pos = 0;
    b.out_size = output_capacity;

    ret = xz_dec_catrun(s, &b, true);
    *output_size = b.out_pos;
    xz_dec_end(s);

    if (ret == XZ_STREAM_END)
        return 0;
    if (ret == XZ_MEM_ERROR || ret == XZ_MEMLIMIT_ERROR)
        return -2;
    if (ret == XZ_BUF_ERROR && b.out_pos == output_capacity)
        return -3; /* output buffer too small */
    return -4; /* invalid, truncated, or corrupt XZ input */
}
