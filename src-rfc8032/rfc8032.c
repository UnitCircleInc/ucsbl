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

// Strategies for limiting impact of active attacks.
// - Random delays between operations to make Vcc/EMP hard.
// - Don't do if tests instead do mov from addresses to get results
// - Ensure called functions are called/returned from by hooking
//   compiler genereted function entry/exit calls.
// - Do tests twice and only if results agree proceed
//
// x ? sha512_update(&hs, "T", 1) : sha512_update(&hs, "F", 1);
// !x ? sha512_update(&hs, "f", 1) : sha512_date(&hs, "t", 1);
// still doesn't protect against x being modified before it is stored.

#include <stdint.h>
#include <stddef.h>
#include "rfc8032.h"

// Don't want to bring in all of libsodium so reimplement
int sodium_is_zero(const uint8_t* b, size_t n) {
  volatile unsigned char d = 0u;

  for (size_t i = 0u; i < n; i++) {
    d |= b[i];
  }
  return 1 & ((d - 1) >> 8);
}


// A more complete ge25519 cononical test
static int x_ge25519_is_canonical(const uint8_t* p) {
  // These two points are non-canonical but pass the standard test
  static const uint8_t p1[32] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  };
  static const uint8_t p2[32] = {
    0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };
  if (memcmp(p, p1, 32) == 0) return 0;
  if (memcmp(p, p2, 32) == 0) return 0;
  return ge25519_is_canonical(p);
};


#if 0
int ed25519_verify(const uint8_t* sig,
                   const uint8_t* m,
                   size_t mlen,
                   const uint8_t* pk) {
  sha512_t   hs;
  uint8_t    h[64];
  ge25519_p3 check;
  ge25519_p3 expected_r;
  ge25519_p3 A;
  ge25519_p3 sb_ah;
  ge25519_p2 sb_ah_p2;
  int rc = 0x7f;

#if 1
  rc ^= (sig[63] & 240) != 0 && sc25519_is_canonical(sig + 32) == 0 ? 1 << 0 : 0;
  rc ^= ge25519_is_canonical(pk) == 0 ? 1 << 1 : 0;
  rc ^= ge25519_frombytes_negate_vartime(&A, pk) != 0 ? 1 << 2 : 0;
  rc ^= ge25519_has_small_order(&A) != 0 ? 1 << 3 : 0;
  rc ^= x_ge25519_is_canonical(sig) == 0 ? 1 << 4 : 0;
  rc ^= ge25519_frombytes(&expected_r, sig) != 0 ? 1 << 5 : 0;
#else
  if ((sig[63] & 240) != 0 &&
    sc25519_is_canonical(sig + 32) == 0) {
    rc = -1;
  }
  if (ge25519_is_canonical(pk) == 0) {
    // Combined with small order check below covers all cases
    rc = -1;
  }
  if (ge25519_frombytes_negate_vartime(&A, pk) != 0 ||
    ge25519_has_small_order(&A) != 0) {
    rc = -1;
  }


#if 1
  // See: https://eprint.iacr.org/2020/1244.pdf
  //   - Page 15 - Checking for non-canonical points
  //   - Page  8 - Table 1
  //   - Page  9 - Algorithm 2
  //   - Page 10 - Note at end of Reject Small order A
  // No requirement  in R to be low order
  // So need extended canonical test
  // This passes Test Vector #2 - which Algoithm 2 passed
  if (x_ge25519_is_canonical(sig) == 0 ||
      ge25519_frombytes(&expected_r, sig) != 0) {
#else
  // Or we can required R to be low order in which case we can use
  // the incomplete canonical test as the two missing points will
  // be caught by the low order test
  // This fails Test Vector #2 - which Algoithm 2 passed
  if (ge25519_is_canonical(sig) == 0 ||
      ge25519_frombytes(&expected_r, sig) != 0 ||
      ge25519_has_small_order(&expected_r) != 0) {
#endif
    rc = -1;
  }
#endif

  sha512_init(&hs);
  sha512_update(&hs, sig, 32);
  sha512_update(&hs, pk, 32);
  sha512_update(&hs, m, mlen);
  sha512_final(&hs, h);
  sc25519_reduce(h);

  ge25519_double_scalarmult_vartime(&sb_ah_p2, h, &A, sig + 32);
  ge25519_p2_to_p3(&sb_ah, &sb_ah_p2);
  ge25519_p3_sub(&check, &expected_r, &sb_ah);

#if 1
  rc ^= ge25519_has_small_order(&check) == 0 ? 1 << 6 : 0;
  return rc;
#else
  if (ge25519_has_small_order(&check) == 0) {
    rc = -1;
  }

  return rc;
#endif
}
#endif

void ed25519_verify_init(ed25519_t* ctx,
                         const uint8_t* sig, const uint8_t* pk) {
  ctx->rc = -1;
  if ((sig[63] & 240) != 0 &&
    sc25519_is_canonical(&sig[32]) == 0) {
    return;
  }
  if (ge25519_is_canonical(pk) == 0) {
    return;
  }

  if (ge25519_frombytes_negate_vartime(&ctx->A, pk) != 0 ||
    ge25519_has_small_order(&ctx->A) != 0) {
    return;
  }
  if (ge25519_frombytes(&ctx->expected_r, sig) != 0 ||
    ge25519_has_small_order(&ctx->expected_r) != 0) {
    return;
  }
  memmove(ctx->s, &sig[32], 32);

  sha512_init(&ctx->hs);
  sha512_update(&ctx->hs, sig, 32);
  sha512_update(&ctx->hs, pk, 32);

  ctx->rc = 0;
}

void ed25519_verify_update(ed25519_t* ctx, const uint8_t* b, size_t n) {
  if (ctx->rc != 0) return;
  sha512_update(&ctx->hs, b, n);
}

int ed25519_verify_final(ed25519_t* ctx) {
  uint8_t    h[64];
  ge25519_p3 check;
  ge25519_p3 sb_ah;
  ge25519_p2 sb_ah_p2;

  if (ctx->rc != 0) return ctx->rc;

  sha512_final(&ctx->hs, h);
  sc25519_reduce(h);

  ge25519_double_scalarmult_vartime(&sb_ah_p2, h, &ctx->A, ctx->s);
  ge25519_p2_to_p3(&sb_ah, &sb_ah_p2);
  ge25519_p3_sub(&check, &ctx->expected_r, &sb_ah);

  return ge25519_has_small_order(&check) - 1;
}



int ed25519_verify(const uint8_t* sig,
                   const uint8_t* m,
                   size_t mlen,
                   const uint8_t* pk) {
  ed25519_t ctx;
  ed25519_verify_init(&ctx, sig, pk);
  ed25519_verify_update(&ctx, m, mlen);
  return ed25519_verify_final(&ctx);
}

