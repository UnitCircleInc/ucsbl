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

#include <string.h>
#include <cb.h>

void cb_init(cb_t* cb, void* b, size_t n) {
  cb->b = b;
  cb->n = n;
  cb->read = 0;
  cb->write = 0;
}

void cb_reset(cb_t* cb) {
  cb->read = 0;
  cb->write = 0;
}

size_t cb_read_avail(const cb_t* cb) {
  return cb->read <= cb->write ? cb->write - cb->read : cb->n - cb->read + cb->write;
}

size_t cb_peek_avail(const cb_t *cb) {
  // return what can be read linearly via cb_peek.
  // The result might be 2 calls to peek/skip to read everything that is queued.
  // Assumes that next layer up will convert input to a linear buffer
  // for the largest "message" size.
  return cb->read <= cb->write ? cb->write - cb->read : cb->n - cb->read;
}

const void* cb_peek(const cb_t* cb) {
  return cb->b + cb->read;
}

void cb_skip(cb_t* cb, size_t n) {
  size_t r = cb->read + n;
  if (r >= cb->n) r -= cb->n;
  cb->read = r;
}

size_t cb_write_avail(const cb_t* cb) {
  return (cb->read > cb->write ? cb->read - cb->write : cb->n - cb->write + cb->read) - 1;
}

void cb_write(cb_t* cb, const void* in, size_t n) {
  // This code ignores wrap around - write catching up to read
  // Caller should check with write_avail and read enough to ensure that
  // wrap around can't happen.
  size_t n1 = cb->n - cb->write; // max write before wrapping buffer
  if (n1 > n) n1 = n;
  memmove(cb->b + cb->write, in, n1);
  if (n > n1) memmove(cb->b, in + n1, (n - n1));
  n1 = cb->write + n;
  if (n1 >= cb->n) n1 -= cb->n;
  cb->write = n1;
}
