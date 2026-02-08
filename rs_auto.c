/*
 * rs_auto.c — Runtime SIMD dispatch for nanors
 *
 * Compiles rs.c multiple times, once per x86_64 ISA tier (SSSE3,
 * AVX2, AVX-512BW) plus a scalar/autodetected default, with each
 * set of symbols given a unique suffix.  reed_solomon_init() then
 * probes the CPU at runtime and sets function pointers to the
 * best available variant.
 *
 * On ARM64 the NEON path is selected at compile time via
 * autoshim.h — no runtime dispatch overhead.
 *
 * If the caller pre-defines any OBLAS_* flag (e.g. -DOBLAS_AVX2)
 * or OBLAS_TINY, dispatch is skipped entirely and rs.c is compiled
 * once with that flag, preserving the original compile-time
 * behaviour.
 *
 */

/* ── Prevent incompatibilities with target multiversioning ──── */

/* GCC's _FORTIFY_SOURCE can attempt to inline memset() with the
 * host machine's ISA, which conflicts when we compile a region
 * under a different #pragma GCC target.                         */
#undef _FORTIFY_SOURCE

#include <stdio.h> /* fprintf / stderr for ISA log */

/* ───────────────────────────────────────────────────────────── */
/*  Token-pasting helper                                        */
/* ───────────────────────────────────────────────────────────── */

#define RS_PASTE2(a, b) a##b
#define RS_PASTE(a, b) RS_PASTE2(a, b)

/* ───────────────────────────────────────────────────────────── */
/*  Symbol renaming                                             */
/*                                                              */
/*  Before each #include "rs.c" we #define every public and     */
/*  internal symbol to a suffixed variant, then #undef them     */
/*  afterwards, so multiple copies coexist in one TU.           */
/*                                                              */
/*  The full list of renamed symbols (18 total):                */
/*    reed_solomon_init  reed_solomon_new                       */
/*    reed_solomon_new_static  reed_solomon_release             */
/*    reed_solomon_encode  reed_solomon_decode                  */
/*    axpy  scal  gemm  invert_mat                              */
/*    obl_axpy  obl_scal  obl_swap  obl_axpyb32                */
/*    obl_axpyb32_ref  obl_axpy_ref  obl_scal_ref              */
/*    gf2_8_mul                                                 */
/*                                                              */
/*  Plus these rs.c / oblas_lite.c internal macros:             */
/*    HAVE_VLA  STACK_ARRAY  stack_alloc  OBL_NOOP              */
/* ───────────────────────────────────────────────────────────── */

/* ───────────────────────────────────────────────────────────── */
/*  Compilation paths                                           */
/* ───────────────────────────────────────────────────────────── */

#if defined(OBLAS_AVX512) || defined(OBLAS_AVX2) || defined(OBLAS_SSE3) || defined(OBLAS_NEON) || defined(OBLAS_TINY)
/* ─── Compile-time override: single ISA, no dispatch ───────── */

/* Rename the no-op reed_solomon_init() from rs.c so it doesn't
 * conflict with our version that wires up function pointers.   */
#define reed_solomon_init reed_solomon_init_noop_
#include "rs.c"
#undef reed_solomon_init

#include "rs_auto.h"

/* Undo the macro redirections from rs_auto.h so we can take
 * the addresses of the real functions compiled from rs.c.      */
#undef reed_solomon_new
#undef reed_solomon_new_static
#undef reed_solomon_release
#undef reed_solomon_encode
#undef reed_solomon_decode

rs_new_fn_t rs_new_fp = 0;
rs_new_static_fn_t rs_new_static_fp = 0;
rs_release_fn_t rs_release_fp = 0;
rs_encode_fn_t rs_encode_fp = 0;
rs_decode_fn_t rs_decode_fp = 0;

void reed_solomon_init(void)
{
    rs_new_fp = reed_solomon_new;
    rs_new_static_fp = reed_solomon_new_static;
    rs_release_fp = reed_solomon_release;
    rs_encode_fp = reed_solomon_encode;
    rs_decode_fp = reed_solomon_decode;

#if defined(OBLAS_AVX512)
    fprintf(stderr, "nanors: using AVX-512 (compile-time)\n");
#elif defined(OBLAS_AVX2)
    fprintf(stderr, "nanors: using AVX2 (compile-time)\n");
#elif defined(OBLAS_SSE3)
    fprintf(stderr, "nanors: using SSSE3 (compile-time)\n");
#elif defined(OBLAS_NEON)
    fprintf(stderr, "nanors: using NEON (compile-time)\n");
#elif defined(OBLAS_TINY)
    fprintf(stderr, "nanors: using scalar/tiny (compile-time)\n");
#else
    fprintf(stderr, "nanors: using scalar (compile-time)\n");
#endif
}

#elif defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64) || defined(_M_AMD64)
/* ─── x86_64: compile four variants, dispatch at runtime ───── */

/* ···· SSSE3 variant ·········································· */
#define reed_solomon_init RS_PASTE(reed_solomon_init, _ssse3)
#define reed_solomon_new RS_PASTE(reed_solomon_new, _ssse3)
#define reed_solomon_new_static RS_PASTE(reed_solomon_new_static, _ssse3)
#define reed_solomon_release RS_PASTE(reed_solomon_release, _ssse3)
#define reed_solomon_encode RS_PASTE(reed_solomon_encode, _ssse3)
#define reed_solomon_decode RS_PASTE(reed_solomon_decode, _ssse3)
#define axpy RS_PASTE(axpy, _ssse3)
#define scal RS_PASTE(scal, _ssse3)
#define gemm RS_PASTE(gemm, _ssse3)
#define invert_mat RS_PASTE(invert_mat, _ssse3)
#define obl_axpy RS_PASTE(obl_axpy, _ssse3)
#define obl_scal RS_PASTE(obl_scal, _ssse3)
#define obl_swap RS_PASTE(obl_swap, _ssse3)
#define obl_axpyb32 RS_PASTE(obl_axpyb32, _ssse3)
#define obl_axpyb32_ref RS_PASTE(obl_axpyb32_ref, _ssse3)
#define obl_axpy_ref RS_PASTE(obl_axpy_ref, _ssse3)
#define obl_scal_ref RS_PASTE(obl_scal_ref, _ssse3)
#define gf2_8_mul RS_PASTE(gf2_8_mul, _ssse3)

#define OBLAS_SSE3

#if defined(_MSC_VER)
/* MSVC: all intrinsics available regardless of /arch */
#elif defined(__clang__)
#pragma clang attribute push(__attribute__((target("ssse3"))), apply_to = function)
#else
#pragma GCC push_options
#pragma GCC target("ssse3")
#endif

#include "rs.c"

#if defined(_MSC_VER)
/* nothing to pop */
#elif defined(__clang__)
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif

#undef OBLAS_SSE3

#undef reed_solomon_init
#undef reed_solomon_new
#undef reed_solomon_new_static
#undef reed_solomon_release
#undef reed_solomon_encode
#undef reed_solomon_decode
#undef axpy
#undef scal
#undef gemm
#undef invert_mat
#undef obl_axpy
#undef obl_scal
#undef obl_swap
#undef obl_axpyb32
#undef obl_axpyb32_ref
#undef obl_axpy_ref
#undef obl_scal_ref
#undef gf2_8_mul
#undef HAVE_VLA
#undef STACK_ARRAY
#undef stack_alloc
#undef OBL_NOOP

/* ···· AVX2 variant ··········································· */
#define reed_solomon_init RS_PASTE(reed_solomon_init, _avx2)
#define reed_solomon_new RS_PASTE(reed_solomon_new, _avx2)
#define reed_solomon_new_static RS_PASTE(reed_solomon_new_static, _avx2)
#define reed_solomon_release RS_PASTE(reed_solomon_release, _avx2)
#define reed_solomon_encode RS_PASTE(reed_solomon_encode, _avx2)
#define reed_solomon_decode RS_PASTE(reed_solomon_decode, _avx2)
#define axpy RS_PASTE(axpy, _avx2)
#define scal RS_PASTE(scal, _avx2)
#define gemm RS_PASTE(gemm, _avx2)
#define invert_mat RS_PASTE(invert_mat, _avx2)
#define obl_axpy RS_PASTE(obl_axpy, _avx2)
#define obl_scal RS_PASTE(obl_scal, _avx2)
#define obl_swap RS_PASTE(obl_swap, _avx2)
#define obl_axpyb32 RS_PASTE(obl_axpyb32, _avx2)
#define obl_axpyb32_ref RS_PASTE(obl_axpyb32_ref, _avx2)
#define obl_axpy_ref RS_PASTE(obl_axpy_ref, _avx2)
#define obl_scal_ref RS_PASTE(obl_scal_ref, _avx2)
#define gf2_8_mul RS_PASTE(gf2_8_mul, _avx2)

#define OBLAS_AVX2

#if defined(_MSC_VER)
/* MSVC: all intrinsics available */
#elif defined(__clang__)
#pragma clang attribute push(__attribute__((target("avx2"))), apply_to = function)
#else
#pragma GCC push_options
#pragma GCC target("avx2")
#endif

#include "rs.c"

#if defined(_MSC_VER)
/* nothing to pop */
#elif defined(__clang__)
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif

#undef OBLAS_AVX2

#undef reed_solomon_init
#undef reed_solomon_new
#undef reed_solomon_new_static
#undef reed_solomon_release
#undef reed_solomon_encode
#undef reed_solomon_decode
#undef axpy
#undef scal
#undef gemm
#undef invert_mat
#undef obl_axpy
#undef obl_scal
#undef obl_swap
#undef obl_axpyb32
#undef obl_axpyb32_ref
#undef obl_axpy_ref
#undef obl_scal_ref
#undef gf2_8_mul
#undef HAVE_VLA
#undef STACK_ARRAY
#undef stack_alloc
#undef OBL_NOOP

/* ···· AVX-512 variant ········································ */
#define reed_solomon_init RS_PASTE(reed_solomon_init, _avx512)
#define reed_solomon_new RS_PASTE(reed_solomon_new, _avx512)
#define reed_solomon_new_static RS_PASTE(reed_solomon_new_static, _avx512)
#define reed_solomon_release RS_PASTE(reed_solomon_release, _avx512)
#define reed_solomon_encode RS_PASTE(reed_solomon_encode, _avx512)
#define reed_solomon_decode RS_PASTE(reed_solomon_decode, _avx512)
#define axpy RS_PASTE(axpy, _avx512)
#define scal RS_PASTE(scal, _avx512)
#define gemm RS_PASTE(gemm, _avx512)
#define invert_mat RS_PASTE(invert_mat, _avx512)
#define obl_axpy RS_PASTE(obl_axpy, _avx512)
#define obl_scal RS_PASTE(obl_scal, _avx512)
#define obl_swap RS_PASTE(obl_swap, _avx512)
#define obl_axpyb32 RS_PASTE(obl_axpyb32, _avx512)
#define obl_axpyb32_ref RS_PASTE(obl_axpyb32_ref, _avx512)
#define obl_axpy_ref RS_PASTE(obl_axpy_ref, _avx512)
#define obl_scal_ref RS_PASTE(obl_scal_ref, _avx512)
#define gf2_8_mul RS_PASTE(gf2_8_mul, _avx512)

#define OBLAS_AVX512

#if defined(_MSC_VER)
/* MSVC: all intrinsics available */
#elif defined(__clang__)
#pragma clang attribute push(__attribute__((target("avx512f,avx512bw"))), apply_to = function)
#else
#pragma GCC push_options
#pragma GCC target("avx512f,avx512bw")
#endif

#include "rs.c"

#if defined(_MSC_VER)
/* nothing to pop */
#elif defined(__clang__)
#pragma clang attribute pop
#else
#pragma GCC pop_options
#endif

#undef OBLAS_AVX512

#undef reed_solomon_init
#undef reed_solomon_new
#undef reed_solomon_new_static
#undef reed_solomon_release
#undef reed_solomon_encode
#undef reed_solomon_decode
#undef axpy
#undef scal
#undef gemm
#undef invert_mat
#undef obl_axpy
#undef obl_scal
#undef obl_swap
#undef obl_axpyb32
#undef obl_axpyb32_ref
#undef obl_axpy_ref
#undef obl_scal_ref
#undef gf2_8_mul
#undef HAVE_VLA
#undef STACK_ARRAY
#undef stack_alloc
#undef OBL_NOOP

/* ···· Scalar / default variant ······························· */
#define reed_solomon_init RS_PASTE(reed_solomon_init, _default)
#define reed_solomon_new RS_PASTE(reed_solomon_new, _default)
#define reed_solomon_new_static RS_PASTE(reed_solomon_new_static, _default)
#define reed_solomon_release RS_PASTE(reed_solomon_release, _default)
#define reed_solomon_encode RS_PASTE(reed_solomon_encode, _default)
#define reed_solomon_decode RS_PASTE(reed_solomon_decode, _default)
#define axpy RS_PASTE(axpy, _default)
#define scal RS_PASTE(scal, _default)
#define gemm RS_PASTE(gemm, _default)
#define invert_mat RS_PASTE(invert_mat, _default)
#define obl_axpy RS_PASTE(obl_axpy, _default)
#define obl_scal RS_PASTE(obl_scal, _default)
#define obl_swap RS_PASTE(obl_swap, _default)
#define obl_axpyb32 RS_PASTE(obl_axpyb32, _default)
#define obl_axpyb32_ref RS_PASTE(obl_axpyb32_ref, _default)
#define obl_axpy_ref RS_PASTE(obl_axpy_ref, _default)
#define obl_scal_ref RS_PASTE(obl_scal_ref, _default)
#define gf2_8_mul RS_PASTE(gf2_8_mul, _default)

#include "autoshim.h"
#include "rs.c"

#undef reed_solomon_init
#undef reed_solomon_new
#undef reed_solomon_new_static
#undef reed_solomon_release
#undef reed_solomon_encode
#undef reed_solomon_decode
#undef axpy
#undef scal
#undef gemm
#undef invert_mat
#undef obl_axpy
#undef obl_scal
#undef obl_swap
#undef obl_axpyb32
#undef obl_axpyb32_ref
#undef obl_axpy_ref
#undef obl_scal_ref
#undef gf2_8_mul
#undef HAVE_VLA
#undef STACK_ARRAY
#undef stack_alloc
#undef OBL_NOOP

/* Clean up any OBLAS_ flags set by autoshim.h */
#undef OBLAS_AVX512
#undef OBLAS_AVX2
#undef OBLAS_SSE3
#undef OBLAS_NEON

/* ── Runtime dispatch: function pointers + init ────────────── */

#include "rs_auto.h"
#include "cpudetect.h"

/* Function pointer storage */
rs_new_fn_t rs_new_fp = 0;
rs_new_static_fn_t rs_new_static_fp = 0;
rs_release_fn_t rs_release_fp = 0;
rs_encode_fn_t rs_encode_fp = 0;
rs_decode_fn_t rs_decode_fp = 0;

/* Forward-declare each variant's public API */
#define DECLARE_VARIANT(sfx)                                                                                                       \
    reed_solomon *reed_solomon_new##sfx(int, int);                                                                                 \
    reed_solomon *reed_solomon_new_static##sfx(void *, size_t, int, int);                                                          \
    void reed_solomon_release##sfx(reed_solomon *);                                                                                \
    int reed_solomon_encode##sfx(reed_solomon *, uint8_t **, int, int);                                                            \
    int reed_solomon_decode##sfx(reed_solomon *, uint8_t **, uint8_t *, int, int);                                                 \
    void reed_solomon_init##sfx(void);

DECLARE_VARIANT(_ssse3)
DECLARE_VARIANT(_avx2)
DECLARE_VARIANT(_avx512)
DECLARE_VARIANT(_default)

#define SELECT_VARIANT(sfx)                                                                                                        \
    do {                                                                                                                           \
        rs_new_fp = reed_solomon_new##sfx;                                                                                         \
        rs_new_static_fp = reed_solomon_new_static##sfx;                                                                           \
        rs_release_fp = reed_solomon_release##sfx;                                                                                 \
        rs_encode_fp = reed_solomon_encode##sfx;                                                                                   \
        rs_decode_fp = reed_solomon_decode##sfx;                                                                                   \
        reed_solomon_init##sfx();                                                                                                  \
    } while (0)

void reed_solomon_init(void)
{
    const rs_cpu_feature_flags feat = rs_detect_cpu();

    if (feat & RS_CPU_FEATURE_AVX512_BIT) {
        SELECT_VARIANT(_avx512);
        fprintf(stderr, "nanors: using AVX-512\n");
    } else if (feat & RS_CPU_FEATURE_AVX2_BIT) {
        SELECT_VARIANT(_avx2);
        fprintf(stderr, "nanors: using AVX2\n");
    } else if (feat & RS_CPU_FEATURE_SSSE3_BIT) {
        SELECT_VARIANT(_ssse3);
        fprintf(stderr, "nanors: using SSSE3\n");
    } else {
        SELECT_VARIANT(_default);
        fprintf(stderr, "nanors: using scalar\n");
    }
}

#else
/* ─── Non-x86 (ARM64, etc.): compile-time selection only ───── */

#include "autoshim.h"
#define reed_solomon_init reed_solomon_init_noop_
#include "rs.c"
#undef reed_solomon_init
#include "rs_auto.h"

/* Undo the macro redirections from rs_auto.h so we can take
 * the addresses of the real functions compiled from rs.c.      */
#undef reed_solomon_new
#undef reed_solomon_new_static
#undef reed_solomon_release
#undef reed_solomon_encode
#undef reed_solomon_decode

rs_new_fn_t rs_new_fp = 0;
rs_new_static_fn_t rs_new_static_fp = 0;
rs_release_fn_t rs_release_fp = 0;
rs_encode_fn_t rs_encode_fp = 0;
rs_decode_fn_t rs_decode_fp = 0;

void reed_solomon_init(void)
{
    rs_new_fp = reed_solomon_new;
    rs_new_static_fp = reed_solomon_new_static;
    rs_release_fp = reed_solomon_release;
    rs_encode_fp = reed_solomon_encode;
    rs_decode_fp = reed_solomon_decode;

#if defined(__aarch64__) || (defined(_MSC_VER) && defined(_M_ARM64))
    fprintf(stderr, "nanors: using NEON (compile-time)\n");
#else
    fprintf(stderr, "nanors: using scalar (compile-time)\n");
#endif
}

#endif /* architecture dispatch */
