// © 2026 Unit Circle Inc.
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
#include "nrf_power.h"
#include "nrf_nvmc.h"

#include "release.h"
#include "log.h"
#include "noclear.h"
#include "fwu.h"
#include "sbl.h"
#include "cbor.h"

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

typedef enum app_state_e {
  APP_IDLE,
  APP_DO_REJECT,
  APP_DO_WATCHDOG_RESET,
  APP_DO_STOP,
} app_state_t;

static app_state_t app_state = APP_IDLE;
static uint32_t saved_reset_reason;
static sbl_rsp_t saved_sbl_rsp;

#define PAGE_SIZE (4096U / sizeof(uint32_t))
#define SLOT_SIZE (120 * PAGE_SIZE)
#define PAGE_START(index_) (index_ & (~PAGE_SIZE+1)) // Get start of "page"
extern uint32_t slot0__[SLOT_SIZE];
extern uint32_t slot1__[PAGE_SIZE+SLOT_SIZE];


static void accept_image(void) {
  LOG_INFO("accept image");
  // Erase slot1[page0] - i.e. the signature of the "old" image
  // Only issue erase command if page not already erased
  for (size_t index = 0; index < PAGE_SIZE; index++) {
    if (slot1__[index] != 0xffffffff) {
      nrf_nvmc_page_erase((uint32_t) &slot1__[PAGE_START(index)]);
      break;
    }
  }
}

static void app_process(void) {
  switch (app_state) {
    default:
    case APP_IDLE:
      break;
    case APP_DO_REJECT:
      LOG_INFO("rejecting image - resetting to bootloader");
      log_flush();
      sbl_run(SBL_CMD_NONE);
      break;
    case APP_DO_WATCHDOG_RESET:
      LOG_INFO("waiting for watchdog - resetting to bootloader");
      log_flush();
      __disable_irq();
      for(;;) __WFE();
      break;
    case APP_DO_STOP:
      LOG_INFO("stopping - will reset to bootloader on wakeup");
      log_flush();
      __disable_irq();
      __DSB();
      __ISB();
      NRF_POWER->SYSTEMOFF = POWER_SYSTEMOFF_SYSTEMOFF_Enter;
      for(;;) __WFE();
      break;
  }
  app_state = APP_IDLE;
}


static size_t respond_with(uint8_t* b, size_t n, const char* fmt, ...) {
  va_list args;
  cbor_stream_t s;
  cbor_init(&s, b, n);
  va_start(args, fmt);
  cbor_error_t e = cbor_vpack(&s, fmt, args);
  va_end(args);
  if (e != CBOR_ERROR_NONE) {
    LOG_ERROR("cbor_vpack: {enum:cbor_error_t}%d", e);
  }
  return cbor_read_avail(&s);
}


size_t app_cmd(const uint8_t* rx, size_t rx_n, uint8_t* tx, size_t tx_n) {
  char cmd[20];
  size_t cmd_n = sizeof(cmd);
  cbor_stream_t s;
  cbor_init(&s, (uint8_t*) rx, rx_n);
  cbor_error_t e = cbor_unpack(&s, "{.cmd:s}", cmd, &cmd_n);

  if (e != CBOR_ERROR_NONE) {
    return 0;
  }

  if (strncmp(cmd, "accept-image", cmd_n) == 0) {
    accept_image();
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }
  else if (strncmp(cmd, "reject-image", cmd_n) == 0) {
    LOG_INFO("scheduling rejecting image");
    app_state = APP_DO_REJECT;
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }
  else if (strncmp(cmd, "watch-dog", cmd_n) == 0) {
    LOG_INFO("scheduling watchdog reset");
    app_state = APP_DO_WATCHDOG_RESET;
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }
  else if (strncmp(cmd, "flash-write", cmd_n) == 0) {
    uint32_t addr;
    uint32_t val;
    e = cbor_unpack(&s, "{.addr:I,.val:I}", &addr, &val);
    if ((e != CBOR_ERROR_NONE) ||
        ((addr % 4) != 0)) {
      return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "invalid");
    }
    LOG_INFO("flash-write: %08lx %08lx", addr, val);
    // NOTE this can trigger a hard fault if ACL is enabled for addr
    nrf_nvmc_write_word(addr, val);
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }
  else if (strncmp(cmd, "flash-erase", cmd_n) == 0) {
    uint32_t addr;
    e = cbor_unpack(&s, "{.addr:I}", &addr);
    if ((e != CBOR_ERROR_NONE) ||
        ((addr % 4096) != 0)) {
      return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "invalid");
    }
    LOG_INFO("flash-erase: %08lx", addr);
    // NOTE this can trigger a hard fault if ACL is enabled for addr
    nrf_nvmc_page_erase(addr);
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }
  else if (strncmp(cmd, "info-app", cmd_n) == 0) {
    LOG_INFO("version");
    return respond_with(tx, tx_n,
        "{.rsp:s,.ver:s,.ts:Q,.sbl-rsp:I,.sbl-ver:s,.rr:I}",
        cmd,
        sbl_app_version((uintptr_t) slot0__),
        sbl_app_timestamp((uintptr_t) slot0__),
        (uint32_t) saved_sbl_rsp,
        sbl_version(),
        saved_reset_reason);
  }
  else if (strncmp(cmd, "stop", cmd_n) == 0) {
#if defined(WAKEUP_PIN_NUMBER)
    nrf_gpio_cfg_sense_input(WAKEUP_PIN_NUMBER,
                             NRF_GPIO_PIN_PULLUP,
                             NRF_GPIO_PIN_SENSE_LOW);
    NRF_GPIO->DETECTMODE = 0;
    LOG_INFO("scheduling stop");
    app_state = APP_DO_STOP;
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
#else
    return respond_with(tx, tx_n,
        "{.rsp:s,.error:s}", cmd, "wakeup pin not configured");
#endif
  }
  return 0;
}

static size_t cmd(const uint8_t* rx, size_t rx_n, uint8_t* tx, size_t tx_n) {
  size_t n;

  n = fwu_cmd(rx, rx_n, tx, tx_n);
  if (n > 0) return n;

  n = app_cmd(rx, rx_n, tx, tx_n);
  if (n > 0) return n;

 return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "unknown cmd");
}

int main(void) {
  SCB->VTOR = (uint32_t) &__isr_vector;

  wdt_feed();

  log_pre_init();

  saved_reset_reason = reset_reason();
  LOG_INFO("reset_reason: %08lx", saved_reset_reason);
  reset_reason_clear(saved_reset_reason);

  saved_sbl_rsp = sbl_rsp();
  LOG_INFO("sbl response: {enum:sbl_rsp_e}%d", saved_sbl_rsp);

  LOG_INFO("sbl version: %s", sbl_version());
  LOG_INFO("app version: %s", sbl_app_version((uintptr_t) slot0__));
  LOG_MEM_INFO("app hash: ", sbl_app_hash(), 64U);

  fwu_init();
  log_notify(0, cmd);
  log_init(DEBUG_UART);

  systick_init();

  uint32_t start = systick_get();
  while (true) {
    log_process();
    fwu_process();
    app_process();
    if (systick_get() - start >= 1000ul) {
      LOG_INFO("%s tick %llu",
          "@" "(#)" BUILD_VER ", " BUILD_DATE ", " BUILD_TYPE,
          systick_get());
      start += 1000ul;
      wdt_feed();
    }
  }
}
