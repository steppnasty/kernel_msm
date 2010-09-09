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

#ifndef _BCM_XDR_H
#define _BCM_XDR_H

/*
 * bcm_xdr_buf_t
 * Structure used for bookkeeping of a buffer being packed or unpacked.
 * Keeps a current read/write pointer and size as well as
 * the original buffer pointer and size.
 *
 */
typedef struct {
	uint8 *buf;		/* pointer to current position in origbuf */
	uint size;		/* current (residual) size in bytes */
	uint8 *origbuf;		/* unmodified pointer to orignal buffer */
	uint origsize;		/* unmodified orignal buffer size in bytes */
} bcm_xdr_buf_t;

void bcm_xdr_buf_init(bcm_xdr_buf_t * b, void *buf, size_t len);

int bcm_xdr_pack_uint32(bcm_xdr_buf_t * b, uint32 val);
int bcm_xdr_unpack_uint32(bcm_xdr_buf_t * b, uint32 * pval);
int bcm_xdr_pack_int32(bcm_xdr_buf_t * b, int32 val);
int bcm_xdr_unpack_int32(bcm_xdr_buf_t * b, int32 * pval);
int bcm_xdr_pack_int8(bcm_xdr_buf_t * b, int8 val);
int bcm_xdr_unpack_int8(bcm_xdr_buf_t * b, int8 * pval);
int bcm_xdr_pack_opaque(bcm_xdr_buf_t * b, uint len, void *data);
int bcm_xdr_unpack_opaque(bcm_xdr_buf_t * b, uint len, void **pdata);
int bcm_xdr_unpack_opaque_cpy(bcm_xdr_buf_t * b, uint len, void *data);
int bcm_xdr_pack_opaque_varlen(bcm_xdr_buf_t * b, uint len, void *data);
int bcm_xdr_unpack_opaque_varlen(bcm_xdr_buf_t * b, uint * plen, void **pdata);
int bcm_xdr_pack_string(bcm_xdr_buf_t * b, char *str);
int bcm_xdr_unpack_string(bcm_xdr_buf_t * b, uint * plen, char **pstr);

int bcm_xdr_pack_uint8_vec(bcm_xdr_buf_t *, uint8 * vec, uint32 elems);
int bcm_xdr_unpack_uint8_vec(bcm_xdr_buf_t *, uint8 * vec, uint32 elems);
int bcm_xdr_pack_uint16_vec(bcm_xdr_buf_t * b, uint len, void *vec);
int bcm_xdr_unpack_uint16_vec(bcm_xdr_buf_t * b, uint len, void *vec);
int bcm_xdr_pack_uint32_vec(bcm_xdr_buf_t * b, uint len, void *vec);
int bcm_xdr_unpack_uint32_vec(bcm_xdr_buf_t * b, uint len, void *vec);

int bcm_xdr_pack_opaque_raw(bcm_xdr_buf_t * b, uint len, void *data);
int bcm_xdr_pack_opaque_pad(bcm_xdr_buf_t * b);

#endif				/* _BCM_XDR_H */
