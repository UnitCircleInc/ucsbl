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

#include "ch.h"
#include "cb.h"

void ch_write(ch_t* ch, void* items, size_t n) {
  cb_write(&ch->cb, items, n * ch->item_size);
  if (ch->on_write) ch->on_write(ch);
}

size_t ch_peek_avail(ch_t* ch) {
  return cb_peek_avail(&ch->cb) / ch->item_size;
}

const void* ch_peek(ch_t* ch) {
  return cb_peek(&ch->cb);
}

void ch_skip(ch_t* ch, size_t n) {
  cb_skip(&ch->cb, n * ch->item_size);
  if (ch->on_skip) ch->on_skip(ch);
}

