/*
 * rs_auto.h — Runtime SIMD dispatch for nanors
 *
 * Drop-in replacement for rs.h. Include this instead of rs.h to
 * get automatic runtime ISA selection. Call reed_solomon_init()
 * once before any other API function.
 */
#pragma once

#ifndef RS_AUTO_H
#define RS_AUTO_H

#include <stdint.h>

/* Pull in the reed_solomon struct, DATA_SHARDS_MAX,
 * reed_solomon_bufsize, and reed_solomon_reconstruct alias. */
#include "rs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Function pointer types for each public API entry ── */

typedef reed_solomon *(*rs_new_fn_t)(int data_shards, int parity_shards);
typedef reed_solomon *(*rs_new_static_fn_t)(void *buf, size_t len, int ds, int ps);
typedef void (*rs_release_fn_t)(reed_solomon *rs);
typedef int (*rs_encode_fn_t)(reed_solomon *rs, uint8_t **shards, int nr_shards, int bs);
typedef int (*rs_decode_fn_t)(reed_solomon *rs, uint8_t **shards, uint8_t *marks, int nr_shards, int bs);

/* ── Global function pointers (set by reed_solomon_init) ─── */

extern rs_new_fn_t rs_new_fp;
extern rs_new_static_fn_t rs_new_static_fp;
extern rs_release_fn_t rs_release_fp;
extern rs_encode_fn_t rs_encode_fp;
extern rs_decode_fn_t rs_decode_fp;

/* ── Redirect public API names through function pointers ── */

#undef reed_solomon_new
#define reed_solomon_new rs_new_fp

#undef reed_solomon_new_static
#define reed_solomon_new_static rs_new_static_fp

#undef reed_solomon_release
#define reed_solomon_release rs_release_fp

#undef reed_solomon_encode
#define reed_solomon_encode rs_encode_fp

#undef reed_solomon_decode
#define reed_solomon_decode rs_decode_fp

/* reed_solomon_reconstruct is defined as reed_solomon_decode in rs.h,
 * which now chains to rs_decode_fp — no extra work needed. */

/*
 * Detect the best SIMD instruction set available on the running
 * CPU and wire all function pointers above to the matching
 * implementation. Must be called once before any other API call.
 *
 * Logs the selected ISA to stderr (e.g. "nanors: using AVX2").
 */
void reed_solomon_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RS_AUTO_H */
