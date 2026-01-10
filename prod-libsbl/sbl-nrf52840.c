// © 2024 Unit Circle Inc.
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


#include "nrf.h"
#include "sbl.h"

void sbl_run(sbl_cmd_t cmd) {
  NRF_POWER->GPREGRET = cmd;
  __DSB();
  NVIC_SystemReset();
}

sbl_rsp_t sbl_rsp(void) {
  sbl_rsp_t r = NRF_POWER->GPREGRET2 & 0xffU;
  NRF_POWER->GPREGRET2 =  SBL_RSP_NORMAL;
  __DSB();
  return r;
}

