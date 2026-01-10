// © 2022 Unit Circle Inc.
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

#include <nrf.h>

void dwt_init(void);

__STATIC_INLINE void dwt_start(void) {
  DWT->CTRL |=  DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0;
}

__STATIC_INLINE void dwt_stop(void) {
  DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
}

__STATIC_INLINE uint32_t dwt_count(void) {
  return DWT->CYCCNT;
}
