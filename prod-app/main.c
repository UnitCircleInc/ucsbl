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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_wdt.h"

#include "release.h"
#include "log.h"
#include "noclear.h"
#include "fwu.h"
#include "sbl.h"

// Define debug interface
#if defined(TX_PIN_NUMBER) && defined(RX_PIN_NUMBER)

#include "cb.h"
#define DEBUG_UART (&debug)
static uint8_t rx_buf[1600];
static cb_t rx_cb = CB_INIT(rx_buf);
static uart_t debug = {
  .uart = NRF_UARTE0,
  .tx = TX_PIN_NUMBER,
  .rx = RX_PIN_NUMBER,
  .tx_cb = NULL, // Filled in by logging framework
  .rx_cb = &rx_cb,
};

void UARTE0_UART0_IRQHandler(void) {
  uart_handler(DEBUG_UART);
}

#else

#define DEBUG_UART (NULL)

#endif

extern uint32_t slot0__[];

#include "nrf_systick.h"
#define SYSTICKS_PER_MS (SystemCoreClock / 1000ul)

static uint64_t ticks = 0;
static uint32_t last;
static void systick_init(void) {
  nrf_systick_load_set(250 * SYSTICKS_PER_MS-1);
  nrf_systick_csr_set(
      NRF_SYSTICK_CSR_CLKSOURCE_CPU |
      NRF_SYSTICK_CSR_TICKINT_DISABLE |
      NRF_SYSTICK_CSR_ENABLE);
  last  = nrf_systick_val_get();
}

static uint64_t systick_get(void) {
  uint32_t new = nrf_systick_val_get();
  if (new > last) {
    last += 250 * SYSTICKS_PER_MS;
  }
  while (last > new + SYSTICKS_PER_MS) {
    last -= SYSTICKS_PER_MS;
    ticks += 1;
  }
  return ticks;
}

static uint32_t reset_reason(void) {
  return NRF_POWER->RESETREAS;
}

static void reset_reason_clear(uint32_t mask) {
  NRF_POWER->RESETREAS = mask;
}

static void wdt_feed(void) {
  for (size_t i = 0U; i < 8U; i++) {
    NRF_WDT->RR[i] = NRF_WDT_RR_VALUE;
  }
}

int main(void) {
  SCB->VTOR = (uint32_t) &__isr_vector;

  wdt_feed();

  log_pre_init();


  uint32_t rr = reset_reason();
  LOG_INFO("reset_reason: %08lx", rr);
  reset_reason_clear(rr);

  sbl_rsp_t r = sbl_rsp();
  LOG_INFO("sbl response: {enum:sbl_rsp_e}%d", r);

  LOG_INFO("sbl version: %s", sbl_version());
  LOG_INFO("app version: %s", sbl_app_version((uintptr_t) slot0__));
  LOG_MEM_INFO("app hash: ", sbl_app_hash(), 64U);

  log_notify(0, fwu_cmd);
  log_init(DEBUG_UART);
  fwu_init(rr, r);

  systick_init();

  uint32_t start = systick_get();
  while (true) {
    log_process();
    fwu_process();
    if (systick_get() - start >= 1000ul) {
      LOG_INFO("%s tick %llu",
          "@" "(#)" BUILD_VER ", " BUILD_DATE ", " BUILD_TYPE,
          systick_get());
      start += 1000ul;
      wdt_feed();
    }
  }
}
