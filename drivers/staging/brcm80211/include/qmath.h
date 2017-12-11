/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __QMATH_H__
#define __QMATH_H__

#include <typedefs.h>

s16 qm_sat32(int32 op);

int32 qm_mul321616(s16 op1, s16 op2);

s16 qm_mul16(s16 op1, s16 op2);

int32 qm_muls321616(s16 op1, s16 op2);

u16 qm_mulu16(u16 op1, u16 op2);

s16 qm_muls16(s16 op1, s16 op2);

int32 qm_add32(int32 op1, int32 op2);

s16 qm_add16(s16 op1, s16 op2);

s16 qm_sub16(s16 op1, s16 op2);

int32 qm_sub32(int32 op1, int32 op2);

int32 qm_mac321616(int32 acc, s16 op1, s16 op2);

int32 qm_shl32(int32 op, int shift);

int32 qm_shr32(int32 op, int shift);

s16 qm_shl16(s16 op, int shift);

s16 qm_shr16(s16 op, int shift);

s16 qm_norm16(s16 op);

s16 qm_norm32(int32 op);

s16 qm_div_s(s16 num, s16 denom);

s16 qm_abs16(s16 op);

s16 qm_div16(s16 num, s16 denom, s16 *qQuotient);

int32 qm_abs32(int32 op);

s16 qm_div163232(int32 num, int32 denom, s16 *qquotient);

int32 qm_mul323216(int32 op1, s16 op2);

int32 qm_mulsu321616(s16 op1, u16 op2);

int32 qm_muls323216(int32 op1, s16 op2);

int32 qm_mul32(int32 a, int32 b);

int32 qm_muls32(int32 a, int32 b);

void qm_log10(int32 N, s16 qN, s16 *log10N, s16 *qLog10N);

void qm_1byN(int32 N, s16 qN, int32 *result, s16 *qResult);

#endif				/* #ifndef __QMATH_H__ */
