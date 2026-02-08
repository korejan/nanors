/*
 * rs.h — Public API for nanors
 *
 * This header defines the public API for the nanors Reed-Solomon library.
 * It is implemented by rs.c and rs_auto.c.  The latter provides runtime
 * dispatch to multiple implementations based on the host CPU's capabilities.
 *
 * See rs_auto.h for a drop-in replacement that automatically selects the
 * best available implementation at runtime.  Include that instead of this
 * header to get automatic ISA dispatch.
 */
#pragma once

#ifndef __RS_H_
#define __RS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_SHARDS_MAX 255

typedef struct _reed_solomon {
    int ds;
    int ps;
    int ts;
    uint8_t p[];
} reed_solomon;

#define reed_solomon_bufsize(ds, ps) (sizeof(reed_solomon) + 2 * (ps) * (ds))
#define reed_solomon_reconstruct reed_solomon_decode

void reed_solomon_init(void);
reed_solomon *reed_solomon_new_static(void *buf, size_t len, int ds, int ps);
reed_solomon *reed_solomon_new(int data_shards, int parity_shards);
void reed_solomon_release(reed_solomon *rs);

int reed_solomon_encode(reed_solomon *rs, uint8_t **shards, int nr_shards, int bs);
int reed_solomon_decode(reed_solomon *rs, uint8_t **shards, uint8_t *marks, int nr_shards, int bs);

#ifdef __cplusplus
}
#endif

#endif
