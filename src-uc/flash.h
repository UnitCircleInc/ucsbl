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
#include <stdint.h>

#define FLASH_PAGE_SIZE        (4096U)

// Write the value to addr - addr must be alligned to sizeof(uint32_t)
// which is implied by the addr being a poitner to uint32_t
void flash_write_uint32(volatile uint32_t* addr, uint32_t value);

// Erase the entire page containing addr
void flash_erase_page(void* addr);


