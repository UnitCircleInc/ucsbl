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

#include "flash.h"
#include "nrf_nvmc.h"

void flash_write_uint32(volatile uint32_t* addr, uint32_t value) {
  nrf_nvmc_write_word((uint32_t) addr, value); //-V2571
}

// Erase the entire page containing addr
void flash_erase_page(void* addr) {
  nrf_nvmc_page_erase(((uint32_t) addr) & (~FLASH_PAGE_SIZE+1));
}
