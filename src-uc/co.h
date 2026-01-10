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


// Simple C co-routines using MACROs.
// Patterned after:
//   https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
//
// Using GCC computed gotos instead of switch, so that switch statements can
// be used.

#pragma once

#include <stddef.h>

#define CO_STATE_T void*
#define CO_INIT(state) state = NULL

typedef enum co_ret_e {
  CO_RET_DONE,
  CO_RET_BUSY,
} CO_RET_T;

#define CO_BEGIN(state) do { \
  if ((state) != NULL) goto *(state); \
} while (0)

#define CO_CAT2(s1, s2) s1##s2
#define CO_CAT(s1, s2) CO_CAT2(s1, s2)

#define CO_WAIT_WHILE(state, cond) do { \
  CO_CAT(CO_LABEL, __LINE__): \
  (state) = &&CO_CAT(CO_LABEL, __LINE__); \
  if (cond) return CO_RET_BUSY; \
} while (0)

#define CO_WAIT_UNTIL(state, cond) CO_WAIT_WHILE(state, !(cond))

#define CO_RETURN(state) do { \
  (state) = &&CO_CAT(CO_LABEL, __LINE__); \
  return CO_RET_BUSY; \
  CO_CAT(CO_LABEL, __LINE__):; \
} while (0)

#define CO_END(state) do { \
  CO_CAT(CO_LABEL, __LINE__): \
  (state) = &&CO_CAT(CO_LABEL, __LINE__); \
  return CO_RET_DONE; \
} while (0)


#define CO_RECV(state_, mask_, timeout_) do { \
  scheduler_recv(mask_, timeout_); \
  CO_RETURN(state_); \
} while (0)


#define CO_WAIT_EVENT(state_, mask_, timeout_) do { \
  scheduler_wait_event(mask_, timeout_); \
  CO_RETURN(state_); \
} while (0)


#define CO_SEND(state_, task_, op_, msg_) do { \
  scheduler_send(task_, op_, msg_); \
  CO_RETURN(state_); \
} while (0)


#define CO_ASYNC_CALL(state_, frame_, sub_) do {\
  frame_ .co_state = NULL; \
  CO_CAT(CO_LABEL, __LINE__): \
  (state_) = &&CO_CAT(CO_LABEL, __LINE__); \
  CO_RET_T r_ = sub_(&frame_); \
  if (r_ != CO_RET_DONE) return r_; \
} while (0)

