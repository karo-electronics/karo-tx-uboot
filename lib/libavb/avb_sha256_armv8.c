/* SHA-256 and SHA-512 implementation based on code by Oliver Gay
 * <olivier.gay@a3.epfl.ch> under a BSD-style license. See below.
 */

/*
 * FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 09/26/2017
 * Issue date:  04/30/2005
 *
 * Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 * Copyright (C) 2017 GlobalLogic.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "avb_sha.h"

#define UNPACK32(x, str)                 \
  {                                      \
    *((str) + 3) = (uint8_t)((x));       \
    *((str) + 2) = (uint8_t)((x) >> 8);  \
    *((str) + 1) = (uint8_t)((x) >> 16); \
    *((str) + 0) = (uint8_t)((x) >> 24); \
  }


static const uint32_t sha256_h0[8] = {0x6a09e667,
                                      0xbb67ae85,
                                      0x3c6ef372,
                                      0xa54ff53a,
                                      0x510e527f,
                                      0x9b05688c,
                                      0x1f83d9ab,
                                      0x5be0cd19};

/* SHA-256 implementation */
void avb_sha256_init(AvbSHA256Ctx* ctx) {
#ifndef UNROLL_LOOPS
  int i;
  for (i = 0; i < 8; i++) {
    ctx->h[i] = sha256_h0[i];
  }
#else
  ctx->h[0] = sha256_h0[0];
  ctx->h[1] = sha256_h0[1];
  ctx->h[2] = sha256_h0[2];
  ctx->h[3] = sha256_h0[3];
  ctx->h[4] = sha256_h0[4];
  ctx->h[5] = sha256_h0[5];
  ctx->h[6] = sha256_h0[6];
  ctx->h[7] = sha256_h0[7];
#endif /* !UNROLL_LOOPS */

  ctx->len = 0;
  ctx->tot_len = 0;
}

extern void sha256_ce_transform(uint32_t *ctx, const void *in, size_t num);

void avb_sha256_update(AvbSHA256Ctx* ctx, const uint8_t* data, size_t len) {
  unsigned int block_nb;
  unsigned int new_len, rem_len, tmp_len;
  const uint8_t* shifted_data;

  tmp_len = AVB_SHA256_BLOCK_SIZE - ctx->len;
  rem_len = len < tmp_len ? len : tmp_len;

  avb_memcpy(&ctx->block[ctx->len], data, rem_len);

  if (ctx->len + len < AVB_SHA256_BLOCK_SIZE) {
    ctx->len += len;
    return;
  }

  new_len = len - rem_len;
  block_nb = new_len / AVB_SHA256_BLOCK_SIZE;

  shifted_data = data + rem_len;

  sha256_ce_transform(ctx->h, ctx->block, 1);
  sha256_ce_transform(ctx->h, shifted_data, block_nb);

  rem_len = new_len % AVB_SHA256_BLOCK_SIZE;

  avb_memcpy(ctx->block, &shifted_data[block_nb << 6], rem_len);

  ctx->len = rem_len;
  ctx->tot_len += (block_nb + 1) << 6;
}

uint8_t* avb_sha256_final(AvbSHA256Ctx* ctx) {
  unsigned int block_nb;
  unsigned int pm_len;
  unsigned int len_b;
#ifndef UNROLL_LOOPS
  int i;
#endif

  block_nb =
      (1 + ((AVB_SHA256_BLOCK_SIZE - 9) < (ctx->len % AVB_SHA256_BLOCK_SIZE)));

  len_b = (ctx->tot_len + ctx->len) << 3;
  pm_len = block_nb << 6;

  avb_memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
  ctx->block[ctx->len] = 0x80;
  UNPACK32(len_b, ctx->block + pm_len - 4);

  sha256_ce_transform(ctx->h, ctx->block, block_nb);

#ifndef UNROLL_LOOPS
  for (i = 0; i < 8; i++) {
    UNPACK32(ctx->h[i], &ctx->buf[i << 2]);
  }
#else
  UNPACK32(ctx->h[0], &ctx->buf[0]);
  UNPACK32(ctx->h[1], &ctx->buf[4]);
  UNPACK32(ctx->h[2], &ctx->buf[8]);
  UNPACK32(ctx->h[3], &ctx->buf[12]);
  UNPACK32(ctx->h[4], &ctx->buf[16]);
  UNPACK32(ctx->h[5], &ctx->buf[20]);
  UNPACK32(ctx->h[6], &ctx->buf[24]);
  UNPACK32(ctx->h[7], &ctx->buf[28]);
#endif /* !UNROLL_LOOPS */

  return ctx->buf;
}
