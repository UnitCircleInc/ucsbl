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

#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "nrf.h"

#include "log.h"
#include "cobs.h"
#include "cb.h"

#if LOG_SAVE_ENABLED
#include "noclear.h"
#else
#define NOCLEAR
#endif

#if LOG_MAX_PORTS > 0
#include "co.h"
#endif

#if LOG_USE_SCHEDULER
#include "scheduler.h"
#endif

typedef struct {
  uart_t* uart;
  uint8_t buf[COBS_ENC_SIZE(LOG_MAX_PACKET_SIZE)+3];
  cb_t    cb;
  bool    tx_enabled;
  bool    overrun;
#if LOG_MAX_PORTS > 0
  log_cb_t* handlers[LOG_MAX_PORTS];
  log_msg_t msg;
  uint8_t port;
#if !LOG_USE_SCHEDULER
  CO_STATE_T co_state;
#endif
#endif
} log_data_t;


static log_data_t log_data;

#if LOG_USE_SCHEDULER
static task_t log_task;
#endif

static size_t strnlen_s (const char* s, size_t n) {
  const char* found = memchr(s, '\0', n);
  return found ? (size_t)(found-s) : n;
}

void log_panic(void) {
  if (log_data.uart != NULL) {
    uart_tx_panic(log_data.uart);
  }
}

void log_flush(void) {
  if (log_data.uart != NULL) {
    uart_tx_flush(log_data.uart);
  }
}

static void tx_buffer(const uint8_t* b, size_t n);

void log_log1_(const char *prefix) {
  union {
    const void* p;
    uint8_t v[sizeof(const void*)];
  } v;
  uint8_t b[5+2];

  v.p = prefix;
  v.v[0] = (v.v[0] & 0xfc) | 0x00;
  memmove(b+2, v.v, 4);
  size_t n = cobs_enc(b+1, b+2, 4); // inplace
  b[0] = 0x00;
  b[1+n] = 0x00;
  tx_buffer(b, n+2);
  if (log_data.tx_enabled) uart_tx_schedule(log_data.uart);
}

void log_logn_(const char* fmt, const char *prefix,  ...) {
  union {
    unsigned int u;
    unsigned long long int ull;
    double d;
    long double ld;
    const void* p;
    uint8_t v[16];
  } v;

  uint8_t b[100]; // Limits total packet size - code below expect less than 253
  size_t n = sizeof(b)-1-2;
  uint8_t* bb = b+1+1;
  size_t sn;

  v.p = prefix;
  v.v[0] = (v.v[0] & 0xfc) | 0x00;
  memmove(bb, v.v, 4);
  bb += 4; n -= 4;

  va_list args;
  va_start(args, prefix);
  while (*fmt != '\0') {
    switch (*fmt++) {
      case '0':
        if (n < 4) goto done;
        v.u = va_arg(args, unsigned int);
        memmove(bb, v.v, 4);
        bb += 4; n -= 4;
        break;
      case '1':
        if (n < 8) goto done;
        v.ull = va_arg(args, unsigned long long int);
        memmove(bb, v.v, 8);
        bb += 8; n -= 8;
        break;
      case '2':
        if (n < 8) goto done;
        v.d = va_arg(args, double);
        memmove(bb, v.v, 8);
        bb += 8; n -= 8;
        break;
      case '3':
        if (n < 16) goto done;
        v.d = va_arg(args, long double);
        memmove(bb, v.v, 16);
        bb += 16; n -= 16;
        break;
      case '4':
        v.p = va_arg(args, char*);
        sn = strnlen_s(v.p, n-1);
        memmove(bb, v.p, sn);
        bb += sn;
        *bb++ = '\0';
        n -= sn + 1;
        break;
      case '5':
        if (n < 4) goto done;
        v.p = va_arg(args, void*);
        memmove(bb, v.v, 4);
        bb += 4; n -= 4;
        break;
      default:
        goto done;
    }
  }
done:
  va_end(args);
  n = cobs_enc(b+1, b+1+1, sizeof(b) -2 - 1 - n); // inplace
  b[0] = 0x00;
  b[1+n] = 0x00;
  tx_buffer(b, n+2);
  if (log_data.tx_enabled) uart_tx_schedule(log_data.uart);
}

void log_mem_(const char *prefix, const void* b, size_t n) {
  union {
    const void* p;
    uint8_t v[sizeof(const void*)];
  } v;
  uint8_t bb[100];

  if (n + 8 + 1 + 2> 100) n = 100 - 8 - 1 - 2;

  v.p = prefix;
  v.v[0] = (v.v[0] & 0xfc) | 0x01;
  memmove(bb+2, v.v, 4);
  v.p = b;
  memmove(bb+2+4, v.v, 4);
  memmove(bb+2+8, b, n);
  n = cobs_enc(bb+1, bb+2, 8+n); // inplace
  bb[0] = 0x00;
  bb[1+n] = 0x00;
  tx_buffer(bb, n+2);
  if (log_data.tx_enabled) uart_tx_schedule(log_data.uart);
}

void suspend_tx(void) {
  log_data.tx_enabled = false;
}

void resume_tx(void) {
  log_data.tx_enabled = true;
  if (log_data.uart != NULL) {
    uart_tx_schedule(log_data.uart);
  }
}

__attribute__((noreturn)) void log_fatal_(void) {
  // We switched ot log_panic so all data will have been flushed
  // If debugger connected then bkpt, otherwise reset
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0) {
    __BKPT(0);
  }
  NVIC_SystemReset();
}

#if LOG_MAX_PORTS > 0

void log_tx(uint8_t port, const uint8_t* data, size_t n) {
  static uint8_t b[256];
  if (n > sizeof(b) - 4) LOG_FATAL("tx message too long %zu", n);
  if ((port < 1) || (63 < port)) LOG_FATAL("invalid port %d", port);
  b[2] = (port << 2) | 3;
  memmove(b+3, data, n);
  n = cobs_enc(b+1, b+2, n + 1);
  b[0] = '\0';
  b[n+1] = '\0';
  tx_buffer(b, n+2);
  if (log_data.tx_enabled) uart_tx_schedule(log_data.uart);
}



void log_notify(uint8_t port, log_cb_t* task) {
  if (port >= LOG_MAX_PORTS) {
    LOG_FATAL("port out of range: %d", port);
  }
  log_data.handlers[port] = task;
}

#if LOG_USE_SCHEDULER
typedef enum {
  RX_DONE       = 1 << 0,
  TX_DONE       = 1 << 1,
  RX_INT        = 1 << 2,
} log_event_e;

static void log_poll(void* context) {
  log_data_t* data = context;
  // Poll as uart RX is configured for DMA
  // Could use timer to poll - but easier to just use polling function
  if ((uart_rx_avail(data->uart) > 0) ||
      (uart_error(data->uart) != UART_ERROR_NONE)) {
    scheduler_notify_task(&log_task, RX_DONE);
  }
  // Uart ISR could do notify - but since we are polling for Rx
  // it is simpler to poll Tx as well
  if (uart_tx_done(data->uart)) {
    scheduler_notify_task(&log_task, TX_DONE);
  }
}

//static int count = 0;
static CO_RET_T log_task_entry(task_t* task) {
  log_data_t* data = task->context;
  CO_BEGIN(task->co_state);
  LOG_INFO("log task %p", (void*) task->entry);
  cb_init(&data->cb, data->buf, sizeof(data->buf));

  // Save current tx_cb so that we get logging from startup
  cb_t save_tx_cb = *(data->uart->tx_cb);
  uart_init(data->uart);
  *(data->uart->tx_cb) = save_tx_cb;
  resume_tx();
  uart_on_wakeup_notify(data->uart, task, RX_INT);
  scheduler_set_stopok(false);
  scheduler_set_pollfn(log_poll);
run:
  while (true) {
    // Wait for a start of frame
    while (true) {
      // TODO Reduce to 1s
      CO_WAIT_EVENT(task->co_state, RX_DONE, 3000);
      if (task->timedout) goto pause;
      size_t n = uart_rx_avail(data->uart);
      if (n == 0) {
        // Must be an error - restart
        uart_restart_rx(data->uart);
        goto run;
      }

      const uint8_t*b = uart_rx_peek(data->uart);
      if (*b != '\0') break;
      uart_rx_skip(data->uart, 1);
    }

    // Process until end of frame
    cb_reset(&data->cb);
    data->overrun = false;
    while (true) {
      CO_WAIT_EVENT(task->co_state, RX_DONE, 100);
      if (task->timedout) goto pause;
      size_t n = uart_rx_avail(data->uart);
      if (n == 0) {
        // Must be an error - restart
        uart_restart_rx(data->uart);
        goto run;
      }

      const uint8_t* b = uart_rx_peek(data->uart);
      const uint8_t* e = memchr(b, '\0', n);
      if (e != NULL) {
        n = e - b;
        if (n > cb_write_avail(&data->cb)) {
          data->overrun = true;
          n = cb_write_avail(&data->cb);
        }
        cb_write(&data->cb, b, n);
        //LOG_MEM_INFO("Rx: ", data->buf,  cb_peek_avail(&data->cb));
        uart_rx_skip(data->uart, e-b); // Leave the 0x00 frame terminator
        ssize_t n = cobs_dec(data->buf, data->buf, cb_peek_avail(&data->cb));
        if ((n < 0) || data->overrun) {
          LOG_ERROR("COBS decode error: %d overrun: %d", (int) n, data->overrun);
        }
        else if (n > 0) {
          uint8_t type = data->buf[0] & 3;
          data->port = data->buf[0] >> 2;
          cb_skip(&data->cb, 1);
          if (type != 0x3) {
            LOG_ERROR("unexpected frame type: %d", type);
          }
          else if (data->port >= LOG_MAX_PORTS) {
            LOG_ERROR("invalid port: %d", data->port);
          }
          else if (data->handlers[data->port]) {
            data->msg.rx = data->buf + 1;
            data->msg.rx_n = n - 1;
            data->msg.tx = data->buf + sizeof(data->buf) - LOG_MAX_PACKET_SIZE;
            data->msg.tx_n = LOG_MAX_PACKET_SIZE;
            CO_SEND(task->co_state, data->handlers[data->port], 0, &data->msg);

            // Send response if present
            if (data->msg.tx_n > 0) {
              data->msg.tx -= 1;
              *data->msg.tx = (data->port << 2) | 0x3;
              data->msg.tx_n += 1;
              size_t n = cobs_enc(data->buf+1, data->msg.tx, data->msg.tx_n);
              data->buf[0] = '\0';
              data->buf[n+1] = '\0';

              uart_tx(data->uart, data->buf, n+2);
            }
          }
          else {
            LOG_ERROR("no handler for port: %d", data->port);
          }
        }
        else {
          LOG_INFO("empty frame");
          // Ignore empty frames
        }
        break;
      }
      else {
        size_t n2 = n;
        if (n > cb_write_avail(&data->cb)) {
          data->overrun = true;
          n = cb_write_avail(&data->cb);
        }
        cb_write(&data->cb, b, n);
        uart_rx_skip(data->uart, n2);
      }
    }
  }
pause:
  //LOG_INFO("log pause");
  suspend_tx();
  scheduler_clear_events(TX_DONE);
  CO_WAIT_EVENT(task->co_state, TX_DONE | RX_DONE, 0);
  if ((task->active & RX_DONE) == 0) {
    scheduler_set_pollfn(NULL);
    scheduler_set_stopok(true);
    if (uart_lp_enter(data->uart)) {
      CO_WAIT_EVENT(task->co_state, RX_INT, 0);
      uart_lp_exit(data->uart);
    }
    scheduler_set_stopok(false);
    scheduler_clear_events(RX_DONE);
    scheduler_set_pollfn(log_poll);
  }
  resume_tx();
  //LOG_INFO("log resume");
  goto run;

  CO_END(task->co_state);
}

#else

static CO_RET_T log_process_internal(log_data_t* data) {
  CO_BEGIN(data->co_state);
  cb_init(&data->cb, data->buf, sizeof(data->buf));

  while (true) {
    // Wait for a start of frame
    while (true) {
      CO_WAIT_UNTIL(data->co_state, uart_rx_avail(data->uart) > 0);
      const uint8_t*b = uart_rx_peek(data->uart);
      if (*b != '\0') break;
      uart_rx_skip(data->uart, 1);
    }

    // Process until end of frame
    cb_reset(&data->cb);
    data->overrun = false;
    while (true) {
      CO_WAIT_UNTIL(data->co_state, uart_rx_avail(data->uart) > 0);
      size_t n = uart_rx_avail(data->uart);
      const uint8_t* b = uart_rx_peek(data->uart);
      const uint8_t* e = memchr(b, '\0', n);
      if (e != NULL) {
        n = e - b;
        if (n > cb_write_avail(&data->cb)) {
          data->overrun = true;
          n = cb_write_avail(&data->cb);
        }
        cb_write(&data->cb, b, n);
        //LOG_MEM_INFO("Rx: ", data->buf,  cb_peek_avail(&data->cb));
        uart_rx_skip(data->uart, e-b); // Leave the 0x00 frame terminator
        ssize_t n = cobs_dec(data->buf, data->buf, cb_peek_avail(&data->cb));
        if ((n < 0) || data->overrun) {
          LOG_ERROR("COBS decode error: %d overrun: %d", (int) n, data->overrun);
        }
        else if (n > 0) {
          uint8_t type = data->buf[0] & 3;
          data->port = data->buf[0] >> 2;
          cb_skip(&data->cb, 1);
          if (type != 0x3) {
            LOG_ERROR("unexpected frame type: %d", type);
          }
          else if (data->port >= LOG_MAX_PORTS) {
            LOG_ERROR("invalid port: %d", data->port);
          }
          else if (data->handlers[data->port]) {
            data->msg.rx = data->buf + 1;
            data->msg.rx_n = n - 1;
            data->msg.tx = data->buf + sizeof(data->buf) - LOG_MAX_PACKET_SIZE;
            data->msg.tx_n = LOG_MAX_PACKET_SIZE;
            data->msg.tx_n = data->handlers[data->port](
                data->msg.rx, data->msg.rx_n, data->msg.tx, data->msg.tx_n);

            // Send response if present
            if (data->msg.tx_n > 0) {
              data->msg.tx -= 1;
              *data->msg.tx = (data->port << 2) | 0x3;
              data->msg.tx_n += 1;
              size_t n = cobs_enc(data->buf+1, data->msg.tx, data->msg.tx_n);
              data->buf[0] = '\0';
              data->buf[n+1] = '\0';

              uart_tx(data->uart, data->buf, n+2);
            }
          }
          else {
            LOG_ERROR("no handler for port: %d", data->port);
          }
        }
        else {
          LOG_INFO("empty frame");
          // Ignore empty frames
        }
        break;
      }
      else {
        size_t n2 = n;
        if (n > cb_write_avail(&data->cb)) {
          data->overrun = true;
          n = cb_write_avail(&data->cb);
        }
        cb_write(&data->cb, b, n);
        uart_rx_skip(data->uart, n2);
      }
    }
  }
  CO_END(data->co_state);
}

void log_process(void) {
  if (log_data.uart != NULL) {
    log_process_internal(&log_data);
  }
}

#endif
#endif

#if !defined(LOG_SIZE)
#define LOG_SIZE (4096)
#endif

extern uint8_t         app_hash__[32];
static NOCLEAR cb_t    tx_cb;
static NOCLEAR uint8_t tx_buf[LOG_SIZE];

static void tx_buffer(const uint8_t* b, size_t n) {
  cb_write(&tx_cb, b, n);
}

#if LOG_SAVE_ENABLED
static NOCLEAR uint8_t app_hash[32];

static uint8_t saved_app_hash[32];
static uint8_t saved_log[LOG_SIZE];
static size_t saved_log_n;

const uint8_t* log_saved_log(size_t* n) {
  *n = saved_log_n;
  return saved_log;
}

const uint8_t* log_saved_app_hash(void) {
  return saved_app_hash;
}

// app_hash__ and app_hash will only be different on a code change.
// We don't want previous log details for code changes.
static bool log_valid(void) {
  return (tx_cb.write < tx_cb.n)     && (tx_cb.read < tx_cb.n) &&
         (tx_cb.n == sizeof(tx_buf)) && (tx_cb.b == tx_buf) &&
         (memcmp(app_hash__, app_hash, sizeof(app_hash)) == 0);
}

static void log_save(void) {
  saved_log_n = cb_read_avail(&tx_cb);

  // If it is empty then "force" dumping the entire contents
  if (saved_log_n == 0) {
    cb_skip(&tx_cb, 1);
    saved_log_n = cb_read_avail(&tx_cb);
  }
  if (saved_log_n > sizeof(saved_log)) saved_log_n = sizeof(saved_log);

  uint8_t* save = saved_log;
  size_t log_rem = saved_log_n;
  size_t n = cb_peek_avail(&tx_cb);
  if (n > log_rem) n = log_rem;
  memmove(save, cb_peek(&tx_cb), n);
  save += n;
  cb_skip(&tx_cb, n);
  log_rem -= n;
  n = cb_peek_avail(&tx_cb);
  if (n > log_rem) n = log_rem;
  memmove(save, cb_peek(&tx_cb), n);
  cb_skip(&tx_cb, n);

  // Save the app_hash associated with the log.
  memmove(saved_app_hash, app_hash, sizeof(app_hash));
}
#endif

void log_pre_init(void) {
#if LOG_SAVE_ENABLED
  saved_log_n = 0;
  if (log_valid()) log_save();

  // Save current app hash so we can get access on next reset
  // app_hash__ might change between resets.
  memmove(app_hash, app_hash__, sizeof(app_hash));
#endif

  log_data.uart = NULL;
  memset(tx_buf, 0, sizeof(tx_buf));
  cb_init(&tx_cb, tx_buf, sizeof(tx_buf));
  suspend_tx();
  LOG_INFO("log-pre-init");
}

void log_init(uart_t* uart) {
  if (uart == NULL) return;

  uart->tx_cb = &tx_cb;
  log_data.uart = uart;

#if LOG_MAX_PORTS == 0
  uart_init(log_data.uart);
  resume_tx();
#else
#if LOG_USE_SCHEDULER
  memset(log_data.handlers, 0, sizeof(log_data.handlers));

  log_task.entry = log_task_entry;
  log_task.context = &log_data;
  log_task.watchdog_ms = 0;
  scheduler_run(&log_task);
#else
  uart_init(log_data.uart);
  resume_tx();
  CO_INIT(log_data.co_state);
#endif
#endif
}
