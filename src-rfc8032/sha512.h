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

typedef struct sha512_s {
  uint64_t H[8];
  uint64_t total[2]; // 128 bit length counter
  size_t  n;
  uint8_t buffer[128];
} sha512_t;

void sha512_init(sha512_t* ctx);
void sha512_update(sha512_t* ctx, const void *buffer, size_t len);
void sha512_final(sha512_t* ctx, void* hash);
