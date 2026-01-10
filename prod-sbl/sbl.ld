/*
 * © 2024 Unit Circle Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#define FLASH BL_CODE
#define RAM   BL_RAM
#define LOGDATA_BASE 0x00100000
#define LOGDATA_LEN  0x00010000

#include "config/nrf52840.ld"
#include "src-uc/log.ld"
