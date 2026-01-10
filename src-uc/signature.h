// © 2025 Unit Circle Inc.
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

#define SIG_BYTES        (64U)
#define PK_BYTES         (32U)
#define HASH_BYTES       (64U)
#define SIG_BLOCK_BYTES (512U)

#define APP_MAX_WHAT (163U)

// 64+8+32 = 104
typedef struct {
  uint8_t sig[SIG_BYTES];
  uint8_t date[8];
  uint8_t key[32];
} app_cert_t;

// 512-2*104-64-4-8-64-1 = 163
typedef struct {
  uint8_t sig[SIG_BYTES];
  uint8_t length[4];
  uint8_t date[8];
  uint8_t hash[HASH_BYTES];
  uint8_t type;
  uint8_t what[APP_MAX_WHAT];
  app_cert_t certs[2];
} app_signature_t;

static_assert(sizeof(app_signature_t) == SIG_BLOCK_BYTES);

