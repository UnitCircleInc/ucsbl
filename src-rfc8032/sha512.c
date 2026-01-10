// © 2023 Unit Circle Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ARM thumb m4
// .text is ~ 2000 bytes
// .rodata is ~ 704 bytes
// stack space is ~ 750 bytes (need to confirm - might be less)
// context size is ~ 212 bytes (align 64)

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "sha512.h"

// By making this a weak global function compiler cannot optimize away
// nor can it make any assumption abuot what happens to pointer.
// This prevents it from optimizing memset away in memzero.
__attribute__((weak)) void dummy_memzero(void *const  p, const size_t n);
__attribute__((weak)) void dummy_memzero(void *const  p, const size_t n) {
  (void) p;
  (void) n;
}

static void memzero(void * const p, const size_t n) {
#if 1
  if (n > 0U) {
    memset(p, 0, n);
    dummy_memzero(p, n);
  }
#else
  volatile unsigned char *volatile p_ =
      (volatile unsigned char *volatile) p;
  size_t i = (size_t) 0U;

  while (i < n) {
    p_[i++] = 0U;
  }
#endif
}

#define UINT128_INC(c_, v_) do { \
  (c_)[1] += (v_); \
  if ((c_)[1] < (v_)) (c_)[0] += 1; \
} while (0)

// Convert list of uint64 to uint8 - BE - n is number of uint64 inputs
static void uint64_to_uint8(uint8_t* out, const uint64_t* in, size_t n) {
  while (n-- > 0) {
    uint64_t v = *in++;
    out[0] = (v >> 56) & 0xff; out[1] = (v >> 48) & 0xff;
    out[2] = (v >> 40) & 0xff; out[3] = (v >> 32) & 0xff;
    out[4] = (v >> 24) & 0xff; out[5] = (v >> 16) & 0xff;
    out[6] = (v >>  8) & 0xff; out[7] = (v >>  0) & 0xff;
    out = &out[8];
  }
}

// Convert list of uint8 to uint64 - BE - n is number of uint64 outputs
#define UINT64(x_)  ((uint64_t) (x_))
static void uint8_to_uint64(uint64_t* out, const uint8_t* in, size_t n) {
  while (n-- > 0) {
    *out = (UINT64(in[0]) << 56) | (UINT64(in[1]) << 48)
         | (UINT64(in[2]) << 40) | (UINT64(in[3]) << 32)
         | (UINT64(in[4]) << 24) | (UINT64(in[5]) << 16)
         | (UINT64(in[6]) <<  8) | (UINT64(in[7]) <<  0);
    out++;
    in = &in[8];
  }
}

// FIPS 180-4:5.1.2 - max fill is 896 bits (including the 1 bit) = 112 bytes
// Let pn = number of bytes in last partial block [0, 127]
// If pn in [  0-111] then fill n = 112 - pn       (range [  1 - 112])
// If pn in [112-127] then fill n = 128 - pn + 112 (range [113 - 128])
static const uint8_t fillbuf[128] = { [0] = 0x80 /* , 0, 0, ...  */ };

// FIPS 180-4:4.2.3
static const uint64_t K[80] = {
  UINT64_C(0x428a2f98d728ae22), UINT64_C(0x7137449123ef65cd),
  UINT64_C(0xb5c0fbcfec4d3b2f), UINT64_C(0xe9b5dba58189dbbc),
  UINT64_C(0x3956c25bf348b538), UINT64_C(0x59f111f1b605d019),
  UINT64_C(0x923f82a4af194f9b), UINT64_C(0xab1c5ed5da6d8118),
  UINT64_C(0xd807aa98a3030242), UINT64_C(0x12835b0145706fbe),
  UINT64_C(0x243185be4ee4b28c), UINT64_C(0x550c7dc3d5ffb4e2),
  UINT64_C(0x72be5d74f27b896f), UINT64_C(0x80deb1fe3b1696b1),
  UINT64_C(0x9bdc06a725c71235), UINT64_C(0xc19bf174cf692694),
  UINT64_C(0xe49b69c19ef14ad2), UINT64_C(0xefbe4786384f25e3),
  UINT64_C(0x0fc19dc68b8cd5b5), UINT64_C(0x240ca1cc77ac9c65),
  UINT64_C(0x2de92c6f592b0275), UINT64_C(0x4a7484aa6ea6e483),
  UINT64_C(0x5cb0a9dcbd41fbd4), UINT64_C(0x76f988da831153b5),
  UINT64_C(0x983e5152ee66dfab), UINT64_C(0xa831c66d2db43210),
  UINT64_C(0xb00327c898fb213f), UINT64_C(0xbf597fc7beef0ee4),
  UINT64_C(0xc6e00bf33da88fc2), UINT64_C(0xd5a79147930aa725),
  UINT64_C(0x06ca6351e003826f), UINT64_C(0x142929670a0e6e70),
  UINT64_C(0x27b70a8546d22ffc), UINT64_C(0x2e1b21385c26c926),
  UINT64_C(0x4d2c6dfc5ac42aed), UINT64_C(0x53380d139d95b3df),
  UINT64_C(0x650a73548baf63de), UINT64_C(0x766a0abb3c77b2a8),
  UINT64_C(0x81c2c92e47edaee6), UINT64_C(0x92722c851482353b),
  UINT64_C(0xa2bfe8a14cf10364), UINT64_C(0xa81a664bbc423001),
  UINT64_C(0xc24b8b70d0f89791), UINT64_C(0xc76c51a30654be30),
  UINT64_C(0xd192e819d6ef5218), UINT64_C(0xd69906245565a910),
  UINT64_C(0xf40e35855771202a), UINT64_C(0x106aa07032bbd1b8),
  UINT64_C(0x19a4c116b8d2d0c8), UINT64_C(0x1e376c085141ab53),
  UINT64_C(0x2748774cdf8eeb99), UINT64_C(0x34b0bcb5e19b48a8),
  UINT64_C(0x391c0cb3c5c95a63), UINT64_C(0x4ed8aa4ae3418acb),
  UINT64_C(0x5b9cca4f7763e373), UINT64_C(0x682e6ff3d6b2b8a3),
  UINT64_C(0x748f82ee5defb2fc), UINT64_C(0x78a5636f43172f60),
  UINT64_C(0x84c87814a1f0ab72), UINT64_C(0x8cc702081a6439ec),
  UINT64_C(0x90befffa23631e28), UINT64_C(0xa4506cebde82bde9),
  UINT64_C(0xbef9a3f7b2c67915), UINT64_C(0xc67178f2e372532b),
  UINT64_C(0xca273eceea26619c), UINT64_C(0xd186b8c721c0c207),
  UINT64_C(0xeada7dd6cde0eb1e), UINT64_C(0xf57d4f7fee6ed178),
  UINT64_C(0x06f067aa72176fba), UINT64_C(0x0a637dc5a2c898a6),
  UINT64_C(0x113f9804bef90dae), UINT64_C(0x1b710b35131c471b),
  UINT64_C(0x28db77f523047d84), UINT64_C(0x32caab7b40c72493),
  UINT64_C(0x3c9ebe0a15c9bebc), UINT64_C(0x431d67c49c100d4c),
  UINT64_C(0x4cc5d4becb3e42b6), UINT64_C(0x597f299cfc657e2a),
  UINT64_C(0x5fcb6fab3ad6faec), UINT64_C(0x6c44198c4a475817)
};

// FIPS 180-4:4.1.3
#define CYCLIC(w_, s_)  (((w_) >> (s_)) | ((w_) << (64 - (s_))))
#define Ch(x_, y_, z_)  (((x_) & (y_)) ^ ((~(x_)) & (z_)))
#define Maj(x_, y_, z_) (((x_) & (y_)) ^ ((x_) & (z_)) ^ ((y_) & (z_)))
#define S0(x) (CYCLIC (x, 28) ^ CYCLIC (x, 34) ^ CYCLIC (x, 39))
#define S1(x) (CYCLIC (x, 14) ^ CYCLIC (x, 18) ^ CYCLIC (x, 41))
#define R0(x) (CYCLIC (x,  1) ^ CYCLIC (x,  8) ^ ((x) >> 7))
#define R1(x) (CYCLIC (x, 19) ^ CYCLIC (x, 61) ^ ((x) >> 6))

static void process_block(sha512_t* ctx, const uint8_t* b) {
  size_t t;
  uint64_t W[80];
  uint64_t S[8];
  uint64_t T[2];

  // Update total number of bits
  UINT128_INC(ctx->total, 128 * UINT64_C(8));

  // FIPS 180-4:6.4.2 step 1
  uint8_to_uint64(&W[0], b, 16);
  for (t = 16; t < 80; t++) {
    W[t] = R1(W[t - 2]) + W[t - 7] + R0(W[t - 15]) + W[t - 16];
  }

  // FIPS 180-4:6.4.2 step 2
  memmove(S, ctx->H, sizeof(S));

  // FIPS 180-4:6.4.2 step 3
  for (t = 0; t < 80; t++) {
    T[0] = S[7] + S1(S[4]) + Ch(S[4], S[5], S[6]) + K[t] + W[t];
    T[1] = S0(S[0]) + Maj(S[0], S[1], S[2]);
    S[7] = S[6]; S[6] = S[5]; S[5] = S[4]; S[4] = S[3] + T[0];
    S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = T[0] + T[1];
  }

  // FIPS 180-4:6.4.2 step 4
  for (t = 0; t < 8; t++) {
    ctx->H[t] += S[t];
  }

  // Defensive - remove state from stack variables
  memzero(W, sizeof(W));
  memzero(S, sizeof(S));
  memzero(T, sizeof(T));
}

// FIPS 180-4:5.3.5
static const uint64_t IH[8] = {
  UINT64_C(0x6a09e667f3bcc908), UINT64_C(0xbb67ae8584caa73b),
  UINT64_C(0x3c6ef372fe94f82b), UINT64_C(0xa54ff53a5f1d36f1),
  UINT64_C(0x510e527fade682d1), UINT64_C(0x9b05688c2b3e6c1f),
  UINT64_C(0x1f83d9abfb41bd6b), UINT64_C(0x5be0cd19137e2179)
};

void sha512_init(sha512_t* ctx) {
  memzero(ctx, sizeof(*ctx));
  memmove(ctx->H, IH, sizeof(IH));
}

void sha512_update(sha512_t* ctx, const void *b, size_t n) {
  // Fill b if incomplete
  if (ctx->n != 0) {
    size_t add = 128 - ctx->n > n ? n : 128 - ctx->n;

    memmove(&ctx->buffer[ctx->n], b, add);
    ctx->n += add;
    if (ctx->n < 128) return; // Not enough for one block
    // ctx->n == 128 - so process one block and see if there is more input

    b = (const uint8_t *) b + add; //-V2571 //-V2562
    n -= add;

    process_block(ctx, ctx->buffer);
    ctx->n = 0;
  }

  // Process blocks
  while (n >= 128) {
    process_block(ctx, b); //-V2571
    b = (const uint8_t *) b + 128;  //-V2571 //-V2562
    n -= 128;
  }

  // Save any remainder for next time. Note: n is < 128
  memmove(&ctx->buffer[0], b, n);
  ctx->n = n;
}

// Note: hash must be at least 64 bytes.
void sha512_final(sha512_t* ctx, void* hash) {
  // Update total number of user bits and save
  UINT128_INC(ctx->total, ctx->n * UINT64_C(8));
  uint8_t  total[16];
  uint64_to_uint8(total, ctx->total, 2);

  // Append pad bytes and ensure buffer is "full" to 112 leaving space for l
  size_t pad = ctx->n >= 112 ? 128 - ctx->n + 112 : 112 - ctx->n;
  sha512_update(ctx, fillbuf, pad);
  sha512_update(ctx, total, 16);

  // Extract the results
  uint64_to_uint8(hash, ctx->H, 8); //-V2571

  // Defensive - clear state
  memzero(ctx, sizeof(*ctx));
}
