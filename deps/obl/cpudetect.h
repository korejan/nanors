/*
 * cpudetect.h — Portable, header-only CPU feature detection
 *
 * Returns a bitmask of supported SIMD instruction set extensions
 * for the running CPU.  Works on x86 (32- and 64-bit, GCC, Clang,
 * MSVC) and ARM64 (compile-time NEON detection).
 *
 * SPDX-License-Identifier: MIT
 *
 * Usage:
 *   #include "cpudetect.h"
 *   rs_cpu_feature_flags feat = rs_detect_cpu();
 *   if (feat & RS_CPU_FEATURE_AVX2_BIT) { ... }
 */
#pragma once

#ifndef OBL_CPUDETECT_H
#define OBL_CPUDETECT_H

#include <stdint.h>

/* ── Feature flag type and bit definitions ─────────────────── */

typedef unsigned rs_cpu_feature_flags;

enum rs_cpu_feature_flag_bits {
    RS_CPU_FEATURE_SSSE3_BIT = (1u << 0),
    RS_CPU_FEATURE_AVX2_BIT = (1u << 1),
    RS_CPU_FEATURE_AVX512_BIT = (1u << 2),
    RS_CPU_FEATURE_NEON_BIT = (1u << 3)
};

/*
 * ──────────────────────────────────────────────
 *  ARM64: NEON is always present, detected at
 *  compile time.  No runtime probing needed.
 * ──────────────────────────────────────────────
 */
#if defined(__aarch64__) || (defined(_MSC_VER) && defined(_M_ARM64))

static inline rs_cpu_feature_flags rs_detect_cpu(void)
{
    return RS_CPU_FEATURE_NEON_BIT;
}

/*
 * ──────────────────────────────────────────────
 *  x86 — MSVC  (32-bit and 64-bit)
 *  Uses __cpuid / __cpuidex and _xgetbv to
 *  query both CPU capability and OS XSAVE
 *  support for extended register sets.
 * ──────────────────────────────────────────────
 */
#elif defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_IX86))

#include <intrin.h>

static inline rs_cpu_feature_flags rs_detect_cpu(void)
{
    rs_cpu_feature_flags result = 0;
    int info[4];

    /* Check max supported CPUID leaf */
    __cpuid(info, 0);
    int max_leaf = info[0];
    if (max_leaf < 1)
        return result;

    /* CPUID leaf 1: basic feature flags */
    __cpuid(info, 1);
    int ecx1 = info[2];

    /* SSSE3: leaf 1, ECX bit 9 */
    if (ecx1 & (1 << 9))
        result |= RS_CPU_FEATURE_SSSE3_BIT;

    /* Check OSXSAVE (ECX bit 27) — needed before testing AVX/AVX-512 */
    if (!(ecx1 & (1 << 27)))
        return result;

    /* Verify OS has enabled XSAVE for YMM (XCR0 bits 1 and 2) */
    unsigned long long xcr0 = _xgetbv(0);
    int ymm_ok = ((xcr0 & 0x06) == 0x06);

    if (!ymm_ok)
        return result;

    if (max_leaf < 7)
        return result;

    /* CPUID leaf 7, sub-leaf 0: extended feature flags */
    __cpuidex(info, 7, 0);
    int ebx = info[1];

    /* AVX2: leaf 7, EBX bit 5 */
    if (ebx & (1 << 5))
        result |= RS_CPU_FEATURE_AVX2_BIT;

    /* AVX-512BW requires OS XSAVE for ZMM (XCR0 bits 5, 6, 7) */
    int zmm_ok = ((xcr0 & 0xE0) == 0xE0);
    if (zmm_ok && ((ebx >> 16) & 1) && ((ebx >> 30) & 1))
        result |= RS_CPU_FEATURE_AVX512_BIT;

    return result;
}

/*
 * ──────────────────────────────────────────────
 *  x86 — GCC / Clang  (32-bit and 64-bit)
 *
 *  64-bit: uses __builtin_cpu_supports() which
 *   handles both CPUID and OS XSAVE checks.
 *
 *  32-bit (i386): falls back to inline-asm
 *   CPUID + _xgetbv checks (same logic as the
 *   MSVC path) because __builtin_cpu_supports
 *   is not always available on older 32-bit
 *   toolchains.
 * ──────────────────────────────────────────────
 */
#elif defined(__x86_64__) || defined(__amd64__)

static inline rs_cpu_feature_flags rs_detect_cpu(void)
{
    rs_cpu_feature_flags result = 0;

    __builtin_cpu_init();

    if (__builtin_cpu_supports("ssse3"))
        result |= RS_CPU_FEATURE_SSSE3_BIT;

    if (__builtin_cpu_supports("avx2"))
        result |= RS_CPU_FEATURE_AVX2_BIT;

    if (__builtin_cpu_supports("avx512bw"))
        result |= RS_CPU_FEATURE_AVX512_BIT;

    return result;
}

#elif defined(__i386__) || defined(__i386)

/*
 * On i386 with -fPIC/-fPIE, GCC reserves EBX for the GOT pointer,
 * so we cannot use "=b" as an output constraint.  We save/restore
 * EBX around CPUID with xchg instead.
 */
static inline void rs_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ __volatile__("xchg %%ebx, %1\n\t"
                         "cpuid\n\t"
                         "xchg %%ebx, %1"
                         : "=a"(*eax), "=r"(*ebx), "=c"(*ecx), "=d"(*edx)
                         : "a"(leaf), "c"(subleaf));
}

static inline uint64_t rs_xgetbv(uint32_t xcr)
{
    uint32_t lo, hi;
    __asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(xcr));
    return ((uint64_t)hi << 32) | lo;
}

static inline rs_cpu_feature_flags rs_detect_cpu(void)
{
    rs_cpu_feature_flags result = 0;
    uint32_t eax, ebx, ecx, edx;

    /* Check max supported CPUID leaf */
    rs_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    if (max_leaf < 1)
        return result;

    /* CPUID leaf 1 */
    rs_cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    /* SSSE3: ECX bit 9 */
    if (ecx & (1u << 9))
        result |= RS_CPU_FEATURE_SSSE3_BIT;

    /* OSXSAVE: ECX bit 27 */
    if (!(ecx & (1u << 27)))
        return result;

    uint64_t xcr0 = rs_xgetbv(0);
    int ymm_ok = ((xcr0 & 0x06) == 0x06);
    if (!ymm_ok)
        return result;

    if (max_leaf < 7)
        return result;

    /* CPUID leaf 7 */
    rs_cpuid(7, 0, &eax, &ebx, &ecx, &edx);

    /* AVX2: EBX bit 5 */
    if (ebx & (1u << 5))
        result |= RS_CPU_FEATURE_AVX2_BIT;

    /* AVX-512F (EBX bit 16) + AVX-512BW (EBX bit 30) + OS ZMM */
    int zmm_ok = ((xcr0 & 0xE0) == 0xE0);
    if (zmm_ok && ((ebx >> 16) & 1) && ((ebx >> 30) & 1))
        result |= RS_CPU_FEATURE_AVX512_BIT;

    return result;
}

/*
 * ──────────────────────────────────────────────
 *  Unknown / unsupported architecture
 *  Returns 0 — caller should use scalar paths.
 * ──────────────────────────────────────────────
 */
#else

static inline rs_cpu_feature_flags rs_detect_cpu(void)
{
    return 0;
}

#endif

#endif /* OBL_CPUDETECT_H */
