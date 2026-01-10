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

#pragma once
#include <stdlib.h>
#include <stdint.h>

#include "sha512.h"
#include "ed25519_ref10.h"

typedef struct {
  sha512_t   hs;
  ge25519_p3 A;
  ge25519_p3 expected_r;
  uint8_t    s[32];
  int        rc;
} ed25519_t;

void ed25519_verify_init(ed25519_t* ctx, const uint8_t* sig, const uint8_t* pk);
void ed25519_verify_update(ed25519_t* ctx, const uint8_t* b, size_t n);
int  ed25519_verify_final(ed25519_t* ctx);

int  ed25519_verify(const uint8_t* sig, const uint8_t* b, size_t n,
                   const uint8_t* pk);
