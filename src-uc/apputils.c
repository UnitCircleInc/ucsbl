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
#include "apputils.h"
#include "log.h"
#include "sha512.h"
#include "rfc8032.h"
#include "nrf.h"
#include "sbl.h"
#include "flash.h"
#include "signature.h"

// SBL configuration data includes:
// * The public portion of the root key.
// * Sizes of manufacturing data and slot0.
// * Size of slot1 is same as slot0 + 1 FLASH page.
//
// root_code_pk default is a random key that can be replaced by the user
// using `sbl.py config` to customize SBL for a specific root key.
// This needs to match value of def_pk in config fn in tools/sbl.py
// As all these items are in the same memory area, we use the root_code_pk
// to locate this sturct and replace with the selected configuration.
typedef struct {
  uint8_t root_code_pk[PK_BYTES];
  app_mem_config_t mem;
} app_config_t;

static const app_config_t config = {
  .mem = {
    .bl_len = 0,
    .bl_state = 0,
    .bl_state_len = 0,
    .manu_data = 0,
    .manu_data_len = 0,
    .slot0 = 0,
    .slot1 = 0,
    .slot_len = 0,
  },
  .root_code_pk = {
    0x73, 0xbe, 0xd9, 0x0c, 0xe4, 0xa9, 0x50, 0x5f,
    0xf8, 0x23, 0x5e, 0x51, 0xfe, 0xce, 0x9d, 0x4d,
    0xde, 0xb0, 0xfc, 0xd4, 0x4c, 0x48, 0xe4, 0x22,
    0xf2, 0x00, 0xc6, 0xb7, 0x8b, 0xd4, 0x81, 0xbf,
  },
};

// 1970-01-01T00:00:00Z
// Once the initial firmware is installed this value is no longer relavent
// as all new certificates will need to have dates later than the currently
// included slot 0 certificates.  Just makes code convenient.
#define ROOT_CODE_PK_DATE (0ULL)

const app_mem_config_t* __attribute__ ((noinline)) app_mem_config(void) {
  return &config.mem;
}

static uint8_t memcmp_safe(const void* b1_, const void* b2_, size_t n) {
  const volatile uint8_t *volatile b1 =  b1_;
  const volatile uint8_t *volatile b2 =  b2_;
  uint8_t cv = 0;
  while (n-- > 0) cv |= (*b1++) ^ (*b2++);
  return cv;
}

static uint32_t as_uint32(const uint8_t b[4]) {
  uint32_t v = b[3];
  v = v * 256U + b[2];
  v = v * 256U + b[1];
  return  v * 256U + b[0];
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

static size_t app_max_len(void) {
  const app_mem_config_t* mem = app_mem_config();
  return mem->slot_len - sizeof(app_signature_t);
}

static bool app_cert_valid(const app_cert_t* cert, size_t n, const uint8_t* pk, uint64_t ts) {
  uint8_t r = ed25519_verify(cert->sig, cert->date,
                n*sizeof(app_cert_t)-offsetof(app_cert_t, date), pk);
  if (r != 0U) {
    LOG_ERROR("cert ed25519_verify(...) = %d", r);
    return false;
  }
  if (as_uint64(cert->date) < ts) {
    LOG_ERROR("Invalid cert date %llu", as_uint64(cert->date));
    return false;
  }
  return true;
}

static inline bool is_aligned(const void *restrict p, size_t a) {
  return (uintptr_t)p % a == 0;
}

static bool app_verify_empty(const app_signature_t* sig, const uint8_t* code) {
  LOG_INFO("app_verify_empty(...) p: %p l: %lu", code, as_uint32(sig->length));
  if (as_uint32(sig->length) > app_max_len()) {
    return false;
  }
  code = &code[as_uint32(sig->length)];
  size_t n = app_max_len() - as_uint32(sig->length);

  LOG_INFO("app_verify_empty: p: %p n: %zu", code, n);

  while (n > 0U) {
    if (is_aligned(code, 4U) && (n >= 4U)) {
      const uint32_t*v = (const uint32_t*) code;
      if (*v != 0xffffffffU) {
        LOG_MEM_ERROR("p:", v, 4U);
        return false;
      }
      code = &code[4U]; n -= 4U;
    }
    else if (is_aligned(code, 2U) && (n >= 2U)) {
      const uint16_t*v = (const uint16_t*) code;
      if (*v != 0xffffU) {
        LOG_MEM_ERROR("p:", v, 2U);
        return false;
      }
      code = &code[2U]; n -= 2U;
    }
    else {
      if (*code != 0xffU) {
        LOG_MEM_ERROR("p:", code, 1U);
        return false;
      }
      code = &code[1U]; n -= 1U;
    }
  }
  return true;
}

static bool app_verify_hash(const app_signature_t* sig, const uint8_t* code) {
  sha512_t ctx;
  uint8_t hash[HASH_BYTES];

  sha512_init(&ctx);
  sha512_update(&ctx, code, as_uint32(sig->length));
  sha512_final(&ctx, hash);

  LOG_MEM_INFO("sig  hash:", sig->hash, HASH_BYTES);
  LOG_MEM_INFO("code hash:", hash, HASH_BYTES);

  return memcmp_safe(hash, sig->hash, sizeof(hash)) == 0U;
}

static const uint8_t* app_image_for_slot(app_slot_t slot) {
  const app_mem_config_t* mem = app_mem_config();
  switch (slot) {
    case APP_SLOT_0: return (const uint8_t*) mem->slot0;
    case APP_SLOT_1: return (const uint8_t*) &mem->slot1[FLASH_PAGE_SIZE/sizeof(uint32_t)];
    case APP_SLOT_1_RESTORE: return (const uint8_t*) mem->slot1;
    default: return NULL;
  }
}

static bool app_image_valid(app_slot_t slot) {
  const uint8_t* image = app_image_for_slot(slot);
  if (image == NULL) {
    return false;
  }

  const app_signature_t* sig = (const app_signature_t*) image;
  const uint8_t* code = (const uint8_t*) &image[sizeof(app_signature_t)];

  { // In a block so that we scope the declared variables
    size_t n = sizeof(app_signature_t);
    const uint8_t* p = (const uint8_t*) sig;
    while (n > 0U) {
      size_t nn = n;
      if (nn > 32U) nn = 32U;
      LOG_MEM_INFO("sig:", p, nn);
      n -= nn;
      p = &p[nn];
    }
  }

  // Check certificates and get Pk used to sign signature block
  // Both certs are required
  const uint8_t* pk = config.root_code_pk;
  uint64_t ts = ROOT_CODE_PK_DATE;
  if (!app_cert_valid(&sig->certs[1], 1U, pk, ts)) {
    return false;
  }
  pk = sig->certs[1].key;
  ts = as_uint64(sig->certs[1].date);
  if (!app_cert_valid(&sig->certs[0], 2U, pk, ts)) {
    return false;
  }
  pk = sig->certs[0].key;
  ts = as_uint64(sig->certs[0].date);
  LOG_MEM_INFO("Using pk:", pk, 32U);
  LOG_INFO("Using ts: %llu", ts);

  // Then verify everything else
  uint8_t r = ed25519_verify(sig->sig, sig->length,
      sizeof(app_signature_t)-offsetof(app_signature_t, length), pk);
  if (r != 0U) {
    LOG_ERROR("ed25519_verify(...) = %d", r);
    return false;
  }
  if (as_uint32(sig->length) > app_max_len()) {
    LOG_ERROR("Invalid length %lu", as_uint32(sig->length));
    return false;
  }
  if (!app_verify_empty(sig, code)) {
    LOG_ERROR("app_verify_empty(...) failed");
    return false;
  }
  if (!app_verify_hash(sig, code)) {
    LOG_ERROR("app_verify_hash(...) failed");
    return false;
  }
  if (memchr(sig->what, '\0', sizeof(sig->what)) == NULL) {
    LOG_MEM_ERROR("sig->what:", sig->what, sizeof(sig->what));
    LOG_ERROR("Invalid what string");
    return false;
  }
  if (sig->type >= APP_TYPE_ERROR) {
    LOG_ERROR("Invalid sig->type: %u", sig->type);
    return false;
  }
  if (as_uint64(sig->date) < ts) {
    LOG_ERROR("Invalid date %llu", as_uint64(sig->date));
    return false;
  }
  return true;
}

static const app_signature_t* app_sig_for_slot(app_slot_t slot) {
  return (const app_signature_t*) app_image_for_slot(slot);
}

static const char* app_what_for_slot(app_slot_t slot) {
  const app_signature_t* sig = app_sig_for_slot(slot);
  if (sig == NULL) {
    return "<unknown slot>";
  }
  if (memchr(sig->what, '\0', sizeof(sig->what)) == NULL) {
    return "<invalid>";
  }
  return (const char*) sig->what;
}

bool app_slot_valid(app_slot_t slot) {
  LOG_INFO("app_slot_valid({enum:app_slot_t}%d)", slot);
  switch (slot) {
    case APP_SLOT_0:
      return app_image_valid(APP_SLOT_0);
    case APP_SLOT_1: {
      bool iv = true;
      // Check that both images are valid
      // This checks that certificates within each image are valid
      // and that both primary and secondary certs are present.
      iv &= app_image_valid(APP_SLOT_0);
      iv &= app_image_valid(APP_SLOT_1);
#if defined(TARGET_DEV)
#else
      const app_signature_t* sig0 = app_sig_for_slot(APP_SLOT_0);
      const app_signature_t* sig1 = app_sig_for_slot(APP_SLOT_1);

      if ((sig0 != NULL) && (sig1 != NULL)) {
        // All fields already checked by app_image_valid
        // Prevent rollback from AFI -> MFI/EFI or MFI->EFI
        iv &= sig1->type >= sig0->type;

        // Check that certificate dates "ratchet"
        iv &= as_uint64(sig1->certs[1].date) >= as_uint64(sig0->certs[1].date);
        iv &= as_uint64(sig1->certs[0].date) >= as_uint64(sig0->certs[0].date);

        // Check that image signature dates "strictly ratchet"
        iv &= as_uint64(sig1->date) > as_uint64(sig0->date);
      }
      else {
        iv &= false;
      }
      // TODO Need to run mitigation - rand delay + perhaps redo tests
#endif
      return iv;
    }
    case APP_SLOT_1_RESTORE:
      return app_image_valid(APP_SLOT_1_RESTORE);
    default:
      LOG_ERROR("Invalid slot %d", slot);
      return false;
  }
}

app_type_t app_type(void) {
  const app_signature_t* sig = app_sig_for_slot(APP_SLOT_0);
  app_type_t t = APP_TYPE_ERROR;
  if ((sig != NULL) && (sig->type <= APP_TYPE_AFI)) {
    t = sig->type;
  }
  LOG_INFO("app_type() = {enum:app_type_t}%d", t);
  return t;
}

const char* app_app_what(void) {
  return app_what_for_slot(APP_SLOT_0);
}

void app_run_nocheck(void) {
  if (app_image_for_slot(APP_SLOT_0) == NULL) return;
  log_flush();

  const app_mem_config_t* mem = app_mem_config();
  uint32_t pc, sp;

  // Get sp and pc from app vector table
  sp = mem->slot0[sizeof(app_signature_t)/sizeof(uint32_t) + 0U];
  pc = mem->slot0[sizeof(app_signature_t)/sizeof(uint32_t) + 1U];
  __asm volatile("msr msp, %0": :"r" (sp));
  __asm volatile("msr psp, %0": :"r" (sp));
  __asm volatile("mov pc, %0": :"r" (pc) : "pc");
  __ISB();
  __NOP();
  __NOP();
  LOG_ERROR("We should never reach here");
}

void app_run(void) {
  if (!app_slot_valid(APP_SLOT_0)) return;
  app_run_nocheck();
}

// This is here to "fix" unused-function errors as these functions are
// defined as static in src-rfc8032/private/ed25519_ref10_fe_25_5.h
// which is a dependent import from src-rfc8032/rfc8032.h
// app_junk ends up being "garbage collected" by the linker so this
// code does not end up in the final sbl image.
bool app_junk(void);
bool app_junk(void) {
  fe25519 x;
  fe25519 y;
  fe25519_0(x);
  fe25519_sq2(y, x);
  fe25519_sq(y, y);
  fe25519_mul(y, y, y);
  fe25519_cswap(y, y, 0U);
  fe25519_cmov(y, y, 0U);
  fe25519_sub(y, y, y);
  return false;
}
