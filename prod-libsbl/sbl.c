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

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdalign.h>

#include "sbl.h"
#include "sha512.h"
#include "signature.h"

#define SBL_SIZE (32768)

#define SIG __attribute__((__aligned__(512), __section__(".signature")))
static const SIG app_signature_t sig = {0};

static bool isprintable(const char* s) {
  while (*s != '\0') {
    if ((*s < ' ') || (*s > '~')) return false;
    s++;
  }
  return true;
}

const char* sbl_version(void) {
  // what string needs to be at least @(#)<xxx>\0 with <xxx> being
  // at least 0.0.0-g0000000 or a total of 4 + 14 + 1 = 19
  //
  // Assummes boot loader starts at address 8 and ends at max SBL size.
  // Which should be true for all ARM targets.
  // ARM reset vector and initial SP are in 0-7, so earliest location of
  // what string is 8.
  const char* start = (const char*) 0x00000008U;
  const char* end   = (const char*) SBL_SIZE;

  for (; start + 19U < end; start++) {
    if ((start[0U] == '@') && (start[1U] == '(') &&
        (start[2U] == '#') && (start[3U] == ')')) {
      start += 4U;
      size_t n = APP_MAX_WHAT;
      if (start + n > end) n = end - start;
      if (memchr(start, '\0', n) != NULL)  {
        if (isprintable(start)) return start;
      }
    }
  }
  return "<unknown>";
}

const char* sbl_app_version(uintptr_t p) {
  if (p != (uintptr_t) NULL) {
    if (p % alignof(app_signature_t) == 0U) {
      const app_signature_t* sig = (app_signature_t*) p;
      if (memchr(sig->what, '\0', sizeof(sig->what)) != NULL) {
        if (isprintable((const char*) sig->what)) {
          return (const char*) sig->what;
        }
      }
    }
  }
  return "<invalid>";
}

static uint64_t as_uint64(const uint8_t b[8]) {
  uint64_t v = b[7];
  v = v * 256U + b[6];
  v = v * 256U + b[5];
  v = v * 256U + b[4];
  v = v * 256U + b[3];
  v = v * 256U + b[2];
  v = v * 256U + b[1];
  return  v * 256U + b[0];
}

uint64_t sbl_app_timestamp(uintptr_t p) {
  if (p != (uintptr_t) NULL) {
    if (p % alignof(app_signature_t) == 0U) {
      const app_signature_t* sig = (app_signature_t*) p;
      return as_uint64(sig->date);
    }
  }
  return 0;
}

const uint8_t* sbl_app_hash(void) {
  return sig.hash;
}


static_assert(sizeof(sbl_sha512_t)>=sizeof(sha512_t));

void sbl_sha512_init(sbl_sha512_t* ctx) {
  sha512_init((sha512_t*) ctx);
}

void sbl_sha512_update(sbl_sha512_t* ctx, const void *buffer, size_t len) {
  sha512_update((sha512_t*) ctx, buffer, len);
}

void sbl_sha512_final(sbl_sha512_t* ctx, void* hash) {
  sha512_final((sha512_t*) ctx, hash);
}
