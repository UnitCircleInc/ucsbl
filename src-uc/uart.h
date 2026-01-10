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

#include <stdint.h>
#include <stdbool.h>

#include "cb.h"
#if 0
#include <dma.h>
#include <gpio.h>
#include <stm32l4xx_ll_usart.h>
#include <scheduler.h>
#endif

typedef struct {
  void*  uart;
  uint32_t tx;
  uint32_t rx;
  cb_t*  tx_cb;
  cb_t*  rx_cb;
  volatile bool   tx_active;
  volatile bool   panic;
} uart_t;

void uart_handler(uart_t* uart);

void uart_init(uart_t* uart);

void uart_tx_panic(uart_t* uart);
void uart_tx_schedule(uart_t* uart);
void uart_tx(uart_t* uart, const uint8_t* b, size_t n);
void uart_tx_flush(uart_t* uart);

size_t uart_rx_avail(uart_t* uart);
const uint8_t* uart_rx_peek(uart_t* uart);
void uart_rx_skip(uart_t* uart, size_t n);

#if 0

typedef enum {
  UART_ERROR_NONE = 0,
  UART_ERROR_TX_DMA = 1 << 0,
  UART_ERROR_RX_DMA = 1 << 1,
  UART_ERROR_OR     = 1 << 2,
  UART_ERROR_FRAMING= 1 << 3,
  UART_ERROR_NOISE  = 1 << 4,
} uart_error_t;


typedef struct {
#if 0
  USART_TypeDef*     uart;
  uint32_t           baud;
  const gpio_alternate_t*  tx;
  const gpio_alternate_t*  rx;
  const gpio_alternate_t*  rts;
  const gpio_alternate_t*  cts;
  const gpio_alternate_t*  de;
  const gpio_output_t*     re;
  dma_t* tx_dma;
  dma_t* rx_dma;
#else
  nrf_drv_uart_t uart;
  uint32_t baud;
  uint32_t tx;
  uint32_t rx;
  uint32_t rts;
  uint32_t cts;
#endif
  size_t tx_n;
#if 0
  task_t* notify;
#endif
  uint32_t events;
  uart_error_t error;
  uint32_t error_noise_count;
  uint32_t error_frame_count;
  bool panic_mode;
} uart_t;


void uart_deinit(uart_t* uart);
void uart_set_baud(uart_t* uart, uint32_t baud);
#if 0
void uart_on_wakeup_notify(uart_t* uart, task_t* notify, uint32_t events);
#endif

void uart_error_counts(uart_t* uart, uint32_t* noise, uint32_t* frame);
uart_error_t uart_error(uart_t* uart);
void uart_restart_rx(uart_t* uart);

void uart_tx_buffer(uart_t* uart, const uint8_t* b, size_t n);
void uart_tx(uart_t* uart, const uint8_t* b, size_t n);
bool uart_tx_done(uart_t* uart);

bool uart_cts_active(uart_t* uart);

void uart_rx_start(uart_t* uart);
void uart_rx_stop(uart_t* uart);
size_t uart_rx_avail(uart_t* uart);
const uint8_t* uart_rx_peek(uart_t* uart);
void uart_rx_skip(uart_t* uart, size_t n);

bool uart_lp_enter(uart_t* uart);
void uart_lp_exit(uart_t* uart);

void uart_pause(uart_t* uart);
void uart_resume(uart_t* uart);
void uart_resume_set_baud(uart_t* uart, uint32_t baud);
#endif

