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

#include <string.h>

#include "nrf.h"
#include "nrf_nvmc.h"

#include "cb.h"
#include "log.h"
#include "cbor.h"
#include "fwu.h"
#include "sbl.h"

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

typedef enum fwu_state_e {
  FWU_WAIT_START,
  FWU_WAIT_BLOCK,
  FWU_WAIT_SCHEDULE,
  FWU_UPDATE,
} fwu_state_t;

static struct {
  uint64_t timestamp;
  uint32_t offset;
  fwu_state_t state;
} fwu_state = {
  .state = FWU_WAIT_START
};

#define PAGE_SIZE (4096U / sizeof(uint32_t))
#define SLOT_SIZE (120 * PAGE_SIZE)
#define PAGE_START(index_) (index_ & (~PAGE_SIZE+1)) // Get start of "page"
extern uint32_t slot0__[SLOT_SIZE];
extern uint32_t slot1__[PAGE_SIZE+SLOT_SIZE];

static void erase_slot1(void) {
  LOG_INFO("erasing slot1__");
  for (size_t i = 0; i < PAGE_SIZE+SLOT_SIZE; i++) {
    if (slot1__[i] != 0xffffffffU) {
      LOG_INFO("erase page %zu", i);
      log_flush();
      nrf_nvmc_page_erase((uint32_t) &slot1__[PAGE_START(i)]);
    }
  }
}

// Best to not become an oracle for the length of the image
// So keep accepting and programming data until
// the slot is full - then fake flash writing.
static void program_data(size_t n, uint32_t data[n]) {
  LOG_INFO("programming slot1__[%lu:%lu]",
      fwu_state.offset*sizeof(uint32_t),
      (fwu_state.offset + n)*sizeof(uint32_t));

  for (size_t i = 0; i < n; i++) {
    if (fwu_state.offset < PAGE_SIZE+SLOT_SIZE) {
      nrf_nvmc_write_word((uint32_t) &slot1__[fwu_state.offset], data[i]);
      fwu_state.offset += 1;
    }
    else {
      // TODO Add a delay that is similar to flash programming
    }
  }
}

size_t fwu_cmd(const uint8_t* rx, size_t rx_n, uint8_t* tx, size_t tx_n) {
  static uint32_t data[1024/sizeof(uint32_t)];
  char cmd[20];
  size_t cmd_n = sizeof(cmd);
  cbor_stream_t s;
  cbor_init(&s, (uint8_t*) rx, rx_n);
  cbor_error_t e = cbor_unpack(&s, "{.cmd:s}", cmd, &cmd_n);

  if (e != CBOR_ERROR_NONE) return 0;

  if (strncmp(cmd, "fw-start", cmd_n) == 0) {
    // TODO Cancel timer that would run boot loader as we are starting
    // NOTE: This command can take up to 1024*1024/2/4096*87.5ms or 11.2s
    // The sender needs to allow that long for the response.

    LOG_INFO("sbl ver: %s", sbl_version());

    fwu_state.offset = PAGE_SIZE;
    fwu_state.state = FWU_WAIT_BLOCK;
    erase_slot1();
    return respond_with(tx, tx_n, "{.rsp:s,.block-size:I}", cmd, sizeof(data));
  }
  else if (strncmp(cmd, "fw-block", cmd_n) == 0) {
    size_t data_n = sizeof(data);
    e = cbor_unpack(&s, "{.data:b}", data, &data_n);
    if ((e != CBOR_ERROR_NONE) ||
        ((data_n % 4) != 0) ||
        (fwu_state.state != FWU_WAIT_BLOCK)) {
      fwu_state.state = FWU_WAIT_START;
      return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "invalid");
    }
    if (data_n != sizeof(data)) {
      fwu_state.state = FWU_WAIT_SCHEDULE;
    }

    program_data(data_n / 4U, data);
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }
  else if (strncmp(cmd, "fw-schedule", cmd_n) == 0) {
    uint64_t timestamp;
    e = cbor_unpack(&s, "{.timestamp:Q}", &timestamp);

    if (e != CBOR_ERROR_NONE) {
      fwu_state.state = FWU_WAIT_START;
      return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "invalid");
    }
    if ((fwu_state.state != FWU_WAIT_BLOCK) &&
        (fwu_state.state != FWU_WAIT_SCHEDULE)) {
      fwu_state.state = FWU_WAIT_START;
      return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "invalid");
    }

#if 0
    // Check timestamp is in future ...
    if (timestamp < now) {
      fwu_state.state = FWU_WAIT_START;
      return respond_with(tx, tx_n, "{.rsp:s,.error:s}", cmd, "invalid");
    }
#endif
    // TODO Set timer to run boot loader - instead just run now
    LOG_INFO("upgrading to: %s", sbl_app_version((uintptr_t)&slot1__[PAGE_SIZE]));
    LOG_INFO("scheduling boot laoder to run at: %llu", timestamp);
    fwu_state.state = FWU_UPDATE;
    return respond_with(tx, tx_n, "{.rsp:s}", cmd);
  }

  return 0;
}

void fwu_process(void) {
  if (fwu_state.state == FWU_UPDATE) {
    LOG_INFO("calling bootloader(SBL_CMD_INSTALL_APP)");
    log_flush();
    sbl_run(SBL_CMD_INSTALL_APP);
  }
}

void fwu_init(void) {
  fwu_state.state = FWU_WAIT_START;
}
