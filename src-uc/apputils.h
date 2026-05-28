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
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "flash.h"

typedef enum app_slot_e {
  APP_SLOT_0,
  APP_SLOT_1,
  APP_SLOT_1_RESTORE,
} app_slot_t;

// Order is important - can only upgrade to new image if new type >= old type
typedef enum app_type_e {
  APP_TYPE_EFI = 0, // Image type used during development/engineering
  APP_TYPE_MFI = 1, // Image type used during manufacturing
  APP_TYPE_AFI = 2, // Application image type
  APP_TYPE_ERROR = 3, // An error occured
} app_type_t;

// Memory configuration/layout used by SBL.
// This is configured using sbl.py configure command.
// The configuration needs to match the actual layout used by the application.
// Typically the layout is:
//   boot loader  // Must be at 0, bl_len - readonly immediately on boot
//   bl_state     // bl_state_len         - readonly before run slot0
//   manu_data    // manu_data_len        - readonly before run slot0 if afi
//   slot0        // slot_len             - readonly before run slot0
//   slot1        // slot_len + PAGE_SIZE - readwrite (idealy no excute)
//   <user defined> //                    - readwrite
#define PAGE_SIZE_WORDS (FLASH_PAGE_SIZE / sizeof(uint32_t))
typedef const struct {
  size_t    bl_len;        // Must be integral number of PAGE_SIZE bytes
  uint32_t(*bl_state)[PAGE_SIZE_WORDS]; //aligned to PAGE_SIZE
  size_t    bl_state_len;  // Must be integral number of PAGE_SIZE bytes
  uint32_t* manu_data;     // Must to be aligned to PAGE_SIZE
  size_t    manu_data_len; // Must be integral number of PAGE_SIZE bytes
  uint32_t* slot0;         // Must to be aligned to PAGE_SIZE
  uint32_t* slot1;         // This  slot_len + PAGE_SIZE aligned to PAGE_SIZE
  size_t    slot_len;      // Must be integral number of PAGE_SIZE bytes
} app_mem_config_t;

const app_mem_config_t* app_mem_config(void);
bool app_slot_valid(app_slot_t slot);
uint32_t app_slot_length(app_slot_t slot);
const char* app_app_what(void);
app_type_t app_type(void);
void app_run(void);
void app_run_nocheck(void);
