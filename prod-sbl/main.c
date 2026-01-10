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

// "Design"
// The boot loader "state" records its progress while it completes the
// stateful changes to the system during FW updates (and/or restores).
// The recorded state is sufficently detailed to allow for recovery/restarting
// any in progress actions when a reset occurs.  Actions include:
// * Flash page erase
// * Copying page for FW update and/or restore
// * ...
//
// Reset causes include:
// * battery failure
// * SW bug
// * fault injection
// * ...
//
// All state transitions proceed sequentially from  BL_STATE_IDLE  through
// BL_STATE_TO_IDLE and then back again to BL_STATE_IDLE.  The only exception
// is BL_STATE_RESTORE_1 may be conditionall skipped, proceeding directly to
// BL_STATE_TO_IDLE.
//
// All states except BL_STATE_TO_IDLE are encoded by writing the number of 0
// words equal to theinteger value of the state enum to the first page of
// the bl_state FLASH area.  BL_STATE_TO_IDLE is encoded separately by writing
// a 0 value to the first word of the second page of the bl_state FLASH area.
// This encoding pairs well with the sequential nature of state transitions
// and the nature of FLASH write and erase operations,minimizing the potential
// for resets resuting in missed state transitions or eroneous encodings.
// The main concern in incomplete writes or erases to FLASH.  This can
// result in inconsistent values being read from FLASH, leading to possible
// invalid state encodings.  The mitigations for the vraious reset causes are:
// * battery failure - application ensures that there is sufficent battery to
//   complete a full update, run to point just for accept, restore.
// * SW bug in boot loader - aside for watch dog, once an operation has started
//   a SW bug cannot casue a reset.   For watch dog timeout set much longer
//   than any FLASH operation plus worst case watch dog feeding interval.
// * fault injection - this idicates active tampering with the device, so
//   ok to fail.
//
// The application requests FW updates by setting the GPREGRET register to
// SBL_CMD_INSTALL_APP and issuing a SW reset.   The boot laoder will then
// "swap" the the currently active app version N in slot0 FLASH area with
// the new app verision N+1 in slot__ FLASH area.  This is done one FLASH
// page at a time, using the one extra FLASH page of slot1 to allow the
// swapping to proceed wihtout error even if a reset occurs.  Once the swap
// is complete the app verison N+1 is run with the boot loader in state
// BL_STATE_RESTORE_0 to allow for possible restore (swap back) if the
// app resets before erasing the first page of slot1.
//

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_rng.h"
#include "nrf_gpio.h"
#include "nrf_wdt.h"
#include "nrf_rtc.h"
#include "nrf_clock.h"


#include "log.h"
#include "release.h"
#include "apputils.h"
#include "sbl.h"
#include "flash.h"

// Define debug interface
#if defined(TX_PIN_NUMBER) && defined(RX_PIN_NUMBER) && defined(TARGET_DEV)

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

static uint32_t reset_reason(void) {
  return NRF_POWER->RESETREAS;
}

static void reset_reason_clear(uint32_t mask) {
  NRF_POWER->RESETREAS = mask;
}

static void rng_fill(size_t n, uint8_t buf[n]) {
  nrf_rng_task_trigger(NRF_RNG_TASK_START);
  for (size_t i = 0; i < n; i++) {
    nrf_rng_event_clear(NRF_RNG_EVENT_VALRDY);
    while (!nrf_rng_event_get(NRF_RNG_EVENT_VALRDY)) __NOP();
    buf[i] = nrf_rng_random_value_get();
  }
  nrf_rng_task_trigger(NRF_RNG_TASK_STOP);
}

static uint32_t rng_uint32(void) {
  uint8_t b[4];
  rng_fill(sizeof(4), b);
  uint32_t r = b[0];
  r =  (r << 8) | b[1];
  r =  (r << 8) | b[2];
  r =  (r << 8) | b[3];
  return r;
}

typedef enum app_class_e {
  APP_CLASS_MFI,
  APP_CLASS_AFI,
  APP_CLASS_EFI,
} app_class_t;


#define ACL_ACL_PERM_Write_Disable (ACL_ACL_PERM_WRITE_Msk)
#define ACL_ACL_PERM_Read_Disable  (ACL_ACL_PERM_READ_Msk)

// Use ACL slot 0 to lock down boot loader and MFI data.
static void lockdown_bl(void) {
#if defined(TARGET_DEV)
  // Assumes that a full part erase has occured so that
  // NRF_UICR->APPROTECT == 0xffffffff
  // NRF_UICR->DEBUGCTRL == 0xffffffff
  if (NRF_UICR->APPROTECT != UICR_APPROTECT_PALL_HwDisabled) {
    flash_write_uint32(&NRF_UICR->APPROTECT, UICR_APPROTECT_PALL_HwDisabled);
    NVIC_SystemReset();
  }
  NRF_APPROTECT->DISABLE = APPROTECT_DISABLE_DISABLE_SwDisable;
#else
  // Ensure that Access Port Protect is enabled
  if (NRF_UICR->APPROTECT != UICR_APPROTECT_PALL_Enabled) {
    flash_write_uint32(&NRF_UICR->APPROTECT, UICR_APPROTECT_PALL_Enabled);
    NVIC_SystemReset();
  }
  NRF_APPROTECT->FORCEPROTECT = APPROTECT_FORCEPROTECT_FORCEPROTECT_Force;

  // Ensure ITM/ETM and FLASH Patch and Breakpoint (FPB) units disabled.
  if (NRF_UICR->DEBUGCTRL != 0x0) {
    flash_write_uint32(&NRF_UICR->DEBUGCTRL, 0x0);
    NVIC_SystemReset();
  }
#endif

  // disable write for boot loader
  const app_mem_config_t* mem = app_mem_config();
  NRF_ACL->ACL[0].ADDR = 0U;
  NRF_ACL->ACL[0].SIZE = mem->bl_len;
  NRF_ACL->ACL[0].PERM = ACL_ACL_PERM_Write_Disable;
}

static void lockdown_bl_state(void) {
  const app_mem_config_t* mem = app_mem_config();
  NRF_ACL->ACL[1].ADDR = (uint32_t) mem->bl_state[0];
  NRF_ACL->ACL[1].SIZE = mem->bl_state_len;
  NRF_ACL->ACL[1].PERM = ACL_ACL_PERM_Write_Disable | ACL_ACL_PERM_Read_Disable;
}

static void lockdown_mfidata(void) {
  const app_mem_config_t* mem = app_mem_config();
  NRF_ACL->ACL[2].ADDR = (uint32_t) mem->manu_data;
  NRF_ACL->ACL[2].SIZE = mem->manu_data_len;
  NRF_ACL->ACL[2].PERM = ACL_ACL_PERM_Write_Disable;
}

static void lockdown_app(void) {
  const app_mem_config_t* mem = app_mem_config();
  NRF_ACL->ACL[3].ADDR = (uint32_t) mem->slot0;
  NRF_ACL->ACL[3].SIZE = (uint32_t) mem->slot_len;
  NRF_ACL->ACL[3].PERM = ACL_ACL_PERM_Write_Disable;
}

#define WDT_TIMEOUT_MS (5000U)
#define WDT_TIMEOUT_TICKS ((uint32_t) ((WDT_TIMEOUT_MS * 32768ULL) / 1000U))

static void wdt_init(void) {
  NRF_WDT->INTENCLR = 1U;
#if defined(TARGET_DEV)
  NRF_WDT->CONFIG = NRF_WDT_BEHAVIOUR_RUN_SLEEP;
#else
  NRF_WDT->CONFIG = NRF_WDT_BEHAVIOUR_RUN_SLEEP_HALT;
#endif
  NRF_WDT->CRV = WDT_TIMEOUT_TICKS == 0U ? 0U : WDT_TIMEOUT_TICKS - 1U;
  NRF_WDT->RREN = 0xffU;
  NRF_WDT->TASKS_START = 1U;
}

static void wdt_feed(void) {
  for (size_t i = 0U; i < 8U; i++) {
    NRF_WDT->RR[i] = NRF_WDT_RR_VALUE;
  }
}

static void mitigate_random_delay(void) {
  nrf_delay_us(rng_uint32()/1000000u); // Random delay of 0-4.3ms
}

static sbl_cmd_t bl_get_cmd(void) {
  return NRF_POWER->GPREGRET & 0xffu;
}
static void bl_set_cmd(sbl_cmd_t cmd) {
  NRF_POWER->GPREGRET = cmd;
  __DSB(); // TODO - should this be ISB?
}

static void bl_set_rsp(sbl_rsp_t rsp) {
  NRF_POWER->GPREGRET2 = rsp;
  __DSB(); // TODO - should this be ISB?
}

static sbl_rsp_t bl_get_rsp(void) {
  return NRF_POWER->GPREGRET2 & 0xffu;
}

void nrfx_rtc_0_irq_handler(void);
void nrfx_rtc_0_irq_handler(void) {
  nrf_rtc_event_clear(NRF_RTC0, NRF_RTC_EVENT_TICK);
}

static __attribute__((noreturn)) void shutdown(void) {
  __disable_irq();
#if defined(LED_PIN_NUMBER)
  nrf_gpio_cfg_output(LED_PIN_NUMBER);
  nrf_gpio_pin_write(LED_PIN_NUMBER, 1);
#endif

  // Start LF RC oscilator
  nrf_clock_lf_src_set(NRF_CLOCK_LFCLK_RC);
  nrf_clock_task_trigger(NRF_CLOCK_TASK_LFCLKSTART);
  while (!nrf_clock_lf_is_running()) {
    wdt_feed();
  }

  // Start RTC
  nrf_rtc_prescaler_set(NRF_RTC0, 4096U - 1U); // Ticks every 125ms
  nrf_rtc_event_clear(NRF_RTC0, NRF_RTC_EVENT_TICK);
  nrf_rtc_int_enable(NRF_RTC0, NRF_RTC_INT_TICK_MASK);
  nrf_rtc_task_trigger(NRF_RTC0, NRF_RTC_TASK_START);
  NRFX_IRQ_PRIORITY_SET(RTC0_IRQn, 6);
  NRFX_IRQ_ENABLE(RTC0_IRQn);
  __enable_irq();
  for (;;) {
    __WFI();
    wdt_feed();
#if defined(LED_PIN_NUMBER)
    nrf_gpio_pin_toggle(LED_PIN_NUMBER);
#endif
  }
}

static __attribute__((noreturn)) void restart(void) {
  log_flush();
  NVIC_SystemReset();
}

// nRF52840 has 1MB of FLASH and page size of 4096
// Application size is 512 kB or 128 FLASH pages (assuming no boot loader).
// ENHANCEMENT: This should end up being less nRF52 specific.
//
// For each page, there are 4 swap states + 4 additional states = 516 states.
// Each state takes 4 bytes to record - 2068  bytes - well within 1 page.
// A second page is used to record BL_STATE_TO_IDLE so that when
// resetting the 2 pages that keep track of the state we can
// erase the first and still know we are trying to get to IDLE allowing the
// second to be erased if for some reason the device resets.
//
// BL_STATE encoding allows for encode IDLE as taking no memory which is
// why the above analysis uses 4 additional states:
//    INSTALL, RUN, RESTORE_0, RESTORE_1.
// and not 5 as IDLE does not need to be included.

typedef enum bl_swap_state_e {
  BL_SWAP_STATE_ERASE1,
  BL_SWAP_STATE_COPY0,
  BL_SWAP_STATE_ERASE0,
  BL_SWAP_STATE_COPY1,
  BL_SWAP_STATES
} bl_swap_state_t;

#define PAGE_WORDS (FLASH_PAGE_SIZE / sizeof(uint32_t))

#define BL_MAX_SWAP_PAGES ((PAGE_WORDS - 4U) / BL_SWAP_STATES)

typedef enum bl_state_e {
  BL_STATE_IDLE,
  BL_STATE_INSTALL,
  BL_STATE_SWAP_START,
  // lots of states in between
  BL_STATE_RUN = BL_STATE_SWAP_START+BL_MAX_SWAP_PAGES*BL_SWAP_STATES,
  BL_STATE_RESTORE_0,
  BL_STATE_RESTORE_1,
  BL_STATE_TO_IDLE,

  // Not so much as a state more an indication of some sort of FLASH error
  BL_STATE_ERROR,
} bl_state_t;

// Return index first index with FLASH page value equal to 0xffffffff.
// If the remainder of the FLASH page is not all 0xffffffff then return
// -1 to indicate error.  Ensure that only values in FLASH page are
// 0x00000000 or 0xfffffff, otherwise return -1.
static int32_t bl_zero_split_idx(const uint32_t* page) {
  size_t i = 0U;
  // Search page until first non-zero value found
  while ((i < PAGE_WORDS) && (page[i] == 0x00000000U)) {
    i++;
  }
  if (i >= PAGE_WORDS) return i;

  // Ensure remainder is all FLASH erased value
  size_t j = i;
  while ((j < PAGE_WORDS) && (page[j] == 0xffffffffU)) {
    j++;
  }
  return j < PAGE_WORDS ? -1 : (int32_t) i;
}

static bl_state_t bl_get_state(void) {
  const app_mem_config_t* mem = app_mem_config();
  int32_t enc0 = bl_zero_split_idx(mem->bl_state[0U]);
  int32_t enc1 = bl_zero_split_idx(mem->bl_state[1U]);

  // If any encX values out of range then return error state
  if ((enc0 < 0) || (enc1 < 0) || (enc0 >= BL_STATE_TO_IDLE) || (enc1 >= 2)) {
    return BL_STATE_ERROR;
  }
  return enc1 == 0U ? (bl_state_t) enc0 : BL_STATE_TO_IDLE;
}

static void bl_set_state(bl_state_t state) {
  // Log transition
  const app_mem_config_t* mem = app_mem_config();
  bl_state_t cur = bl_get_state();
  if ((BL_STATE_SWAP_START <= cur) && (cur < BL_STATE_RUN) &&
      (state >= BL_STATE_RUN)) {
    uint32_t index = cur - BL_STATE_SWAP_START;
    uint32_t base = (index / BL_SWAP_STATES) * PAGE_WORDS;
    bl_swap_state_t swapstate = (bl_swap_state_t) (index % BL_SWAP_STATES);
    LOG_INFO("{enum:bl_swap_state_e}%d/%08lx -> {enum:bl_state_e}%d",
        swapstate, base, state);
  }
  else if ((cur < BL_STATE_SWAP_START) &&
           (BL_STATE_SWAP_START <= state) && (state < BL_STATE_RUN)) {
    uint32_t index = state - BL_STATE_SWAP_START;
    uint32_t base = (index / BL_SWAP_STATES) * PAGE_WORDS;
    bl_swap_state_t swapstate = (bl_swap_state_t) (index % BL_SWAP_STATES);
    LOG_INFO("{enum:bl_state_e}%d -> {enum:bl_swap_state_e}%d/%08lx",
        cur, swapstate, base);
  }
  else if ((BL_STATE_SWAP_START <= cur) && (cur < BL_STATE_RUN) &&
           (BL_STATE_SWAP_START <= state) && (state < BL_STATE_RUN)) {
    uint32_t indexc = cur - BL_STATE_SWAP_START;
    uint32_t basec = (indexc / BL_SWAP_STATES) * PAGE_WORDS;
    bl_swap_state_t swapstatec = (bl_swap_state_t) (indexc % BL_SWAP_STATES);
    uint32_t indexs = state - BL_STATE_SWAP_START;
    uint32_t bases = (indexs / BL_SWAP_STATES) * PAGE_WORDS;
    bl_swap_state_t swapstates = (bl_swap_state_t) (indexs % BL_SWAP_STATES);
    LOG_INFO("{enum:bl_swap_state_e}%d/%08lx -> {enum:bl_swap_state_e}%d/%08lx",
        swapstatec, basec, swapstates, bases);
  }
  else {
    LOG_INFO("{enum:bl_state_e}%d -> {enum:bl_state_e}%d", cur, state);
  }

  // Update FLASH with new state
  if (state == BL_STATE_TO_IDLE) {
    flash_write_uint32(&mem->bl_state[1][0], 0x00000000u);
  }
  else if (state != cur + 1) {
    LOG_FATAL("Can't {enum:bl_state_e}%d -> {enum:bl_state_e}%d", cur, state);
  }
  else {
    flash_write_uint32(&mem->bl_state[0][state-BL_STATE_INSTALL], 0x00000000u);
  }
}

static bool bl_swap(bl_state_t state) {
  const app_mem_config_t* mem = app_mem_config();
  uint32_t index = state - BL_STATE_SWAP_START;
  uint32_t base = (index / BL_SWAP_STATES) * PAGE_WORDS;

  // If we are past the last page in the slot then keep bumping state
  // until it reaches the BL_STATE_RUN.
  if (base * sizeof(uint32_t) >= mem->slot_len) {
    bl_set_state(state + 1);
    return false;
  }

  bl_swap_state_t swapstate = (bl_swap_state_t) (index % BL_SWAP_STATES);
  //LOG_INFO("bl_swap {enum:bl_swap_state_e}%d/%lu", swapstate, base);
  switch (swapstate) {
    case BL_SWAP_STATE_ERASE1:
      // Ensure slot1[n] is erased
      for (size_t i = 0u; i < PAGE_WORDS; i++) {
        if (mem->slot1[base+i] != 0xffffffffu) {
          // Erase the page and restart
          //LOG_INFO("erasing slot1[%lu] %p", base, &mem->slot1[base]);
          flash_erase_page(&mem->slot1[base]);
          break;
        }
      }
      for (size_t i = 0u; i < PAGE_WORDS; i++) {
        if (mem->slot1[base+i] != 0xffffffffu) {
          return true;
        }
      }
      break;
    case BL_SWAP_STATE_COPY0:
      // Copy from slot0[n] to slot1[n]
      for (size_t i = 0u; i < PAGE_WORDS; i++) {
        if ((mem->slot1[base+i] == 0xffffffffu) &&
            (mem->slot0[base+i] != 0xffffffffu)) {
          flash_write_uint32(&mem->slot1[base+i], mem->slot0[base+i]);
        }
        else if (mem->slot1[base+i] == mem->slot0[base+i]) {
          // Already correct
        }
        else {
          // Erase the page and restart
          flash_erase_page(&mem->slot1[base]);
          return true;
        }
      }
      break;
    case BL_SWAP_STATE_ERASE0:
      // Ensure slot0[n] is erased
      for (size_t i = 0u; i < PAGE_WORDS; i++) {
        if (mem->slot0[base+i] != 0xffffffffu) {
          // Erase the page and restart
          //LOG_INFO("erasing slot0[%lu] %p", base, &mem->slot0[base]);
          flash_erase_page(&mem->slot0[base]);
          break;
        }
      }
      for (size_t i = 0u; i < PAGE_WORDS; i++) {
        if (mem->slot0[base+i] != 0xffffffffu) {
          return true;
        }
      }
      break;
    case BL_SWAP_STATE_COPY1:
      // Copy from slot1[n+1] to slot0[n]
      for (size_t i = 0u; i < PAGE_WORDS; i++) {
        if ((mem->slot0[base+i] == 0xffffffffu) &&
            (mem->slot1[base+PAGE_WORDS+i] != 0xffffffffu)) {
          flash_write_uint32(&mem->slot0[base+i],
                              mem->slot1[base+PAGE_WORDS+i]);
        }
        else if (mem->slot0[base+i] == mem->slot1[base+PAGE_WORDS+i]) {
          // Already correct
        }
        else {
          // Erase the page and restart
          flash_erase_page(&mem->slot0[base]);
          return true;
        }
      }
      break;
    default:
      LOG_FATAL("invalid swap state: %d", swapstate);
      break;
  }
  bl_set_state(state + 1);
  return false;
}

static void bl_run_app(void) {
  lockdown_bl_state();
  if (app_type() >= APP_TYPE_AFI) lockdown_mfidata();
  lockdown_app();

  mitigate_random_delay();

  lockdown_bl_state();
  if (app_type() >= APP_TYPE_AFI) lockdown_mfidata();
  lockdown_app();

  wdt_feed();
  log_flush();
  app_run();
}

// Returns true if boot loader is requesting a restart (SW reset)
// false otherwise.
//
// NOTE: Setting or cmd/rsp done before setting new state so that if
// reset occurs between the two we can recover.
static bool bl_process(sbl_cmd_t cmd) {
  const app_mem_config_t* mem = app_mem_config();
  bl_state_t state = bl_get_state();
  size_t slot_size = mem->slot_len / sizeof(uint32_t);
  LOG_INFO("bl_process: state: {enum:bl_state_t}%d", state);
  wdt_feed();

  // "Restore" to where we left off
  switch (state) {
    case BL_STATE_IDLE:
      if ((reset_reason() & POWER_RESETREAS_SREQ_Msk) &&
          (cmd == SBL_CMD_INSTALL_APP)) {
        // Record request
        LOG_INFO("got command: SBL_CMD_INSTALL_APP");
        bl_set_state(BL_STATE_INSTALL);
      }
      else {
        // Due to HW reset or SW with no cmd
        bl_set_rsp(SBL_RSP_NORMAL);
        bl_run_app();
        wdt_feed();

        // If run app return then app not valid - device is bricked - shutdown
        shutdown();
      }
      break;
    case BL_STATE_INSTALL:
      // Ignore request if app isn't valid
      if (app_slot_valid(APP_SLOT_1)) {
        bl_set_state(BL_STATE_SWAP_START);
      }
      else {
        bl_set_rsp(SBL_RSP_INSTALL_ERROR);
        bl_set_cmd(SBL_CMD_RUN_APP);
        bl_set_state(BL_STATE_TO_IDLE);
        return true;
      }
      break;
    case BL_STATE_RUN:
      // If run app returns then app not valid - try restoring original
      // If app resets and hasn't cleared state - then bad update - try restore
      bl_set_rsp(SBL_RSP_NEW_FW_FIRST_RUN);
      bl_set_cmd(SBL_CMD_RUN_APP);
      bl_set_state(BL_STATE_RESTORE_0);
      return true;
      break;
    case BL_STATE_RESTORE_0:
      if (cmd != SBL_CMD_NONE) {
        // App must be requesting a new install so chain through IDLE
        bl_set_cmd(cmd);
        bl_set_state(BL_STATE_TO_IDLE);
        return true;
      }
      if (!app_slot_valid(APP_SLOT_1_RESTORE)) {
        // We ue SBL_CMD_NONE to force SBL to run the nexxt state
        // vs running directly.
        LOG_INFO("New image was good - so we are not swapping back");
        bl_set_rsp(SBL_RSP_NORMAL);
        bl_set_cmd(SBL_CMD_NONE);
        bl_set_state(BL_STATE_TO_IDLE);
        return true;
      }
      wdt_feed();

      // NOTE: Use a simplified state machine for restore.
      // In the rare cases that things fail - the restarts will take slightly
      // longer.  Could switch to a 128*3 state machine that would work
      // similarly to SWAP (erase slot0, copy slot1->slot0, erase slot1) and
      // work page by page.  Doesn't seem warrented.
      //
      // NOTE: Could rely on Copy loop only - but this would result in
      // a lot of resets which would take longer - the assumption is that
      // images don't share much in common and copy failures for
      // functioning units that are not being attacked by fault-injection
      // rarely (if ever) fail the copy and reverification steps.
      //
      // Ensure that things are erased
      for (size_t index = 0; index < slot_size; index++) {
        if (mem->slot0[index] != 0xffffffffu) {
          flash_erase_page(&mem->slot0[index]);
          wdt_feed();
        }
        if ((index % 4096U) == 0U) {
          wdt_feed();
        }
      }

      // Recheck - if not erase then reset and try again
      // Above loop will attempt to "erase" any pages that failed.
      for (size_t index = 0; index < slot_size; index++) {
        if (mem->slot0[index] != 0xffffffffu) {
          return true;
        }
        if ((index % 4096U) == 0U) {
          wdt_feed();
        }
      }

      // Copy
      for (size_t index = 0; index < slot_size; index++) {
        if ((mem->slot0[index] == 0xffffffffu) &&
            (mem->slot1[index] != 0xffffffffu)) {
          flash_write_uint32(&mem->slot0[index], mem->slot1[index]);
        }
        else if (mem->slot0[index] == mem->slot1[index]) {
          // Skip as already correct
        }
        else {
          // Erase the page and restart
          flash_erase_page(&mem->slot0[index]);
          return true;
        }
        if ((index % 4096U) == 0U) {
          wdt_feed();
        }
      }

      // Recheck - if not equal then reset and try again
      // The erase page below will allow the above loop to attempt "writing"
      // any words that failed.
      for (size_t index = 0; index < slot_size; index++) {
        if (mem->slot0[index] != mem->slot1[index]) {
          flash_erase_page(&mem->slot0[index]);
          return true;
        }
        if ((index % 4096U) == 0U) {
          wdt_feed();
        }
      }

      bl_set_state(BL_STATE_RESTORE_1);
      break;
    case BL_STATE_RESTORE_1:
      // Ensure slot 1 erased
      for (size_t index = 0; index < slot_size; index++) {
        if (mem->slot1[index] != 0xffffffffu) {
          flash_erase_page(&mem->slot1[index]);
          wdt_feed();
        }
        if ((index % 4096U) == 0U) {
          wdt_feed();
        }
      }

      // Recheck - if not erase then reset and try again
      // Above loop will attempt to "erase" any pages that failed.
      for (size_t index = 0; index < slot_size; index++) {
        if (mem->slot1[index] != 0xffffffffu) {
          return true;
        }
        if ((index % 4096U) == 0U) {
          wdt_feed();
        }
      }
      bl_set_rsp(SBL_RSP_RESTORE_FIRST_RUN);
      bl_set_cmd(SBL_CMD_RUN_APP);
      bl_set_state(BL_STATE_TO_IDLE);
      return true;
      break;
    case BL_STATE_ERROR:
      // Assume that we can recover back to BL_STATE_IDLE.
      // NOTE:
      // We are mostly likely broken if we get to BL_STATE_ERROR from any
      // state other than BL_STATE_IDLE or BL_STATE_TO_IDLE.  That said
      // resets during execution of erases in BL_STATE_TO_IDLE are the most
      // likely sources (other than fault injection) to end up in
      // BL_STATE_ERROR (e.g. partial erases).  So retrying to goto idle
      // is best course of action.
      bl_set_rsp(SBL_RSP_INTERNAL_ERROR);
      bl_set_cmd(SBL_CMD_RUN_APP);

      // Reset bl_state pages to BL_STATE_IDLE
      flash_erase_page(mem->bl_state[0]);
      flash_erase_page(mem->bl_state[1]);
      return true;
      break;
    case BL_STATE_TO_IDLE:
      if (cmd == SBL_CMD_NONE) {
        bl_set_rsp(SBL_RSP_NORMAL);
        bl_set_cmd(SBL_CMD_RUN_APP);
      }
      else {
        // Chain command so we eventually execute
        bl_set_cmd(cmd);
      }

      // Reset bl_state pages to BL_STATE_IDLE
      flash_erase_page(mem->bl_state[0]);
      flash_erase_page(mem->bl_state[1]);
      return true;
      break;
    default:
      return bl_swap(state);
      break;
  }
  return false;
}

#if defined(TARGET_DEV)
#define BUILD_TYPE "DEV"
#else
#define BUILD_TYPE "REL"
#endif

const char SBL_WHAT[] = "@" "(#)" BUILD_VER ", " BUILD_TYPE ", " BUILD_DATE ", SBL";
volatile const char* sbl_what;

int main(void);
int main(void) {
  // ENHANCEMENT Perhaps NRF_APP_PROTECT->DISABLE and lockdown() should be in
  // ResetHandler - so that they execute very early on

  // Lock chip down as much as possible
  mitigate_random_delay();
  lockdown_bl();
  wdt_init();

  // Do a second time to try and mitigate fault-injection atacks
  mitigate_random_delay();
  lockdown_bl();
  wdt_init();

  log_pre_init();

  LOG_INFO("%s", "release:  " BUILD_VER);
  LOG_INFO("%s", "branch:   " BUILD_BRANCH);
  LOG_INFO("%s", "machine:  " BUILD_MACHINE);
  LOG_INFO("%s", "user:     " BUILD_USER);
  LOG_INFO("app what: %s", app_app_what());
  LOG_INFO("sbl what: %s", SBL_WHAT);
  sbl_what = SBL_WHAT;  // To force SBL_WHAT to be included in output binary

  LOG_INFO("resetreas: %08lx", reset_reason());

  // Capture cmd - and then clear as we only process once
  sbl_cmd_t cmd = bl_get_cmd();
  bl_set_cmd(SBL_CMD_NONE);

  // See if there was a reqeust to run
  if ((reset_reason() & POWER_RESETREAS_SREQ_Msk) &&
      (cmd == SBL_CMD_RUN_APP)) {
    // rsp set by "caller" - i.e. restart requestor that set SBL_CMD_RUN_APP
    bl_run_app();
    // app was not valid so restart
    restart();
    mitigate_random_delay();
    restart();
  }

  // Process 1 action  - as we might need to restart to process new cmd
  // before initializing remaining drivers.
  if (bl_process(cmd)) {
    restart();
    mitigate_random_delay();
    restart();
  }

  // NOTE: Above code initializes:
  // * Watchdog Timer
  // * Internal LF clock (needed by watchdog timer)
  //
  // It makes use of RNG, but leaves in state that is essentially the same
  // as reset.
  //
  // NOTE: Boot loader state machine ensures that app is run before any of the
  // following code is executed, so free to make use of any HW beyond this
  // point - e.g. serial port using DMA and interupts, etc.

  // Init remaining drivers
  log_init(DEBUG_UART);

  while (true) {
    if (bl_process(SBL_CMD_NONE)) {
      restart();
      mitigate_random_delay();
      restart();
    }
    log_flush();
    wdt_feed();
  }
}

// Place a bunch of undefined instructions after all code so that any
// fault injections that attempt to bypass flow control end up executing
// undefined instructions that will cause an exception and eventual SW reset.
// 0xde00 is reserved as a permenantly undefined 16 bit thumb instruction
// area by ARM see:
// - Section 3.5.1 of ARM Architecture Reference Manual Thumb-2 Supplement
//
const __attribute__((__section__(".protect"))) uint16_t protect[] = {
  0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00,
  0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00,
  0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00,
  0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00, 0xde00,
};
