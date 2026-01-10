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

#include <stdlib.h>
#include "cb.h"

typedef struct ch_s {
  cb_t cb;
  size_t item_size;
  void (*on_write)(const void*) ;
  const void* on_write_ctx;
  void (*on_skip)(const void*) ;
  const void* on_skip_ctx;
} ch_t;

void ch_write(ch_t* ch, void* items, size_t n);
size_t ch_peek_avail(ch_t* ch);
const void* ch_peek(ch_t* ch);
void ch_skip(ch_t* ch, size_t n);
