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
//
#include <stdbool.h>
#include <stdint.h>

#include "nrf_gpio.h"
#include "nrf_uarte.h"

#include "uart.h"
#include "cb.h"

typedef enum {
  UART_ERROR_NONE = 0,
  UART_ERROR_TX_DMA = 1 << 0,
  UART_ERROR_RX_DMA = 1 << 1,
  UART_ERROR_OR     = 1 << 2,
  UART_ERROR_FRAMING= 1 << 3,
  UART_ERROR_NOISE  = 1 << 4,
} uart_error_t;

void uart_handler(uart_t* uart) {
  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_ERROR)) {
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ERROR);
    //nrf_uarte_errorsrc_get_and_clear(uart->uart);
  }
  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_ENDRX)) {
    // This will trigger a STARTRX
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDRX);
  }
  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_RXDRDY)) {
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXDRDY);
    size_t w = uart->rx_cb->write += 1;
    if (w >= uart->rx_cb->n) w -= uart->rx_cb->n;
    uart->rx_cb->write = w;
  }

  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_RXTO)) {
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXTO);
  }

  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_RXSTARTED)) {
    // Setup for next buffer after current RX completes
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXSTARTED);
    nrf_uarte_rx_buffer_set(uart->uart, uart->rx_cb->b, uart->rx_cb->n);
  }


  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_ENDTX)) {
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDTX);
    nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STOPTX);
  }

  if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_TXSTOPPED)) {
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_TXSTOPPED);

    // If there is more data then send it now
    size_t n = cb_peek_avail(uart->tx_cb);
    if (n > 0) {
      nrf_uarte_tx_buffer_set(uart->uart, cb_peek(uart->tx_cb), n);
      cb_skip(uart->tx_cb, n);
      nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STARTTX);
    }
    else {
      uart->tx_active = false;
    }
  }
}

void uart_init(uart_t* uart) {
  nrf_gpio_pin_set(uart->tx);
  nrf_gpio_cfg_output(uart->tx);

  nrf_gpio_cfg_input(uart->rx, NRF_GPIO_PIN_NOPULL);

  nrf_uarte_baudrate_set(uart->uart, NRF_UARTE_BAUDRATE_1000000);
  nrf_uarte_configure(uart->uart, NRF_UARTE_PARITY_EXCLUDED,
                      NRF_UARTE_HWFC_DISABLED);
  nrf_uarte_txrx_pins_set(uart->uart, uart->tx, uart->rx);

  // Enable interrupts
  if (uart->rx_cb) {
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDRX);
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ERROR);
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXTO);
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXDRDY);
    nrf_uarte_int_enable(uart->uart, NRF_UARTE_INT_ENDRX_MASK |
                                     NRF_UARTE_INT_ERROR_MASK |
                                     NRF_UARTE_INT_RXTO_MASK  |
                                     NRF_UARTE_INT_RXDRDY_MASK);
  }


  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDTX);
  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_TXSTOPPED);


  nrf_uarte_int_enable(uart->uart, NRF_UARTE_INT_ENDTX_MASK |
                                   NRF_UARTE_INT_TXSTOPPED_MASK);

  NVIC_SetPriority(nrfx_get_irq_number((void *)uart->uart), 3);
  NVIC_EnableIRQ(nrfx_get_irq_number((void *)uart->uart));

  uart->tx_active = false;
  uart->panic = false;

  nrf_uarte_enable(uart->uart);

  if (uart->rx_cb) {
    nrf_uarte_rx_buffer_set(uart->uart, uart->rx_cb->b, uart->rx_cb->n);
    nrf_uarte_shorts_enable(uart->uart, NRF_UARTE_SHORT_ENDRX_STARTRX);
    nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STARTRX);
  }
}

static void tx_panic(uart_t* uart) {
  size_t n = cb_peek_avail(uart->tx_cb);
  while (n > 0) {
    nrf_uarte_tx_buffer_set(uart->uart, cb_peek(uart->tx_cb), n);
    cb_skip(uart->tx_cb, n);

    nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STARTTX);
    while (!nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_ENDTX)) {
      __NOP();
    }
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDTX);
    nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STOPTX);

    while (!nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_TXSTOPPED)) {
      __NOP();
    }
    nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_TXSTOPPED);

    n = cb_peek_avail(uart->tx_cb);
  }
}

void uart_tx_schedule(uart_t* uart) {
  (void) uart;
  if (uart->panic) {
    tx_panic(uart);
    return;
  }
  if (uart->tx_active) return;
  size_t n = cb_peek_avail(uart->tx_cb);
  if (n == 0) return;
  uart->tx_active = true;

  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDTX);
  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_TXSTOPPED);
  nrf_uarte_tx_buffer_set(uart->uart, cb_peek(uart->tx_cb), n);
  cb_skip(uart->tx_cb, n);
  nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STARTTX);
}

void uart_tx(uart_t* uart, const uint8_t* b, size_t n) {
  cb_write(uart->tx_cb, b, n);
  uart_tx_schedule(uart);
}

size_t uart_rx_avail(uart_t* uart) {
  if (uart->rx_cb) {
    return cb_peek_avail(uart->rx_cb);
  }
  else {
    return 0;
  }
}

const uint8_t* uart_rx_peek(uart_t* uart) {
  return cb_peek(uart->rx_cb);
}

void uart_rx_skip(uart_t* uart, size_t n) {
  cb_skip(uart->rx_cb, n);
}

void uart_tx_panic(uart_t* uart) {
  // Disable all interrupts
  NVIC_DisableIRQ(nrfx_get_irq_number((void *)uart->uart));
  nrf_uarte_int_disable(uart->uart, NRF_UARTE_INT_ENDRX_MASK |
                                    NRF_UARTE_INT_ERROR_MASK |
                                    NRF_UARTE_INT_RXTO_MASK  |
                                    NRF_UARTE_INT_RXDRDY_MASK  |
                                    NRF_UARTE_INT_ENDTX_MASK |
                                    NRF_UARTE_INT_TXSTOPPED_MASK);
  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDRX);
  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ERROR);
  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXTO);
  nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_RXDRDY);

  // Flush the current uart->tx_cb;
  while (uart->tx_active) {
    // Could be END_TX or TXSTOPPED event pending
    while (!nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_ENDTX) &&
           !nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_TXSTOPPED)) {
      __NOP();
    }

    if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_ENDTX)) {
      nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_ENDTX);
      nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STOPTX);
    }
    else if (nrf_uarte_event_check(uart->uart, NRF_UARTE_EVENT_TXSTOPPED)) {
      nrf_uarte_event_clear(uart->uart, NRF_UARTE_EVENT_TXSTOPPED);

      // If there is more data then send it now
      size_t n = cb_peek_avail(uart->tx_cb);
      if (n > 0) {
        nrf_uarte_tx_buffer_set(uart->uart, cb_peek(uart->tx_cb), n);
        cb_skip(uart->tx_cb, n);
        nrf_uarte_task_trigger(uart->uart, NRF_UARTE_TASK_STARTTX);
      }
      else {
        uart->tx_active = false;
      }
    }
    else {
      // Should never happen
    }
  }

  // We are now in panic mode - no interrupts just polling
  uart->panic = true;
}

void uart_tx_flush(uart_t* uart) {
  while (uart->tx_active) __NOP();
}
