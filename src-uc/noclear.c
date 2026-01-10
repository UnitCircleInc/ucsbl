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

// The no clear section is for data that should be presisted across resets.
// In general the data stored in the noclear area changees too often to be
// persisted to non-volatile storage (e.g. internal/external FLASH/EEPROM).
// It is assumed that the data is recoverable in some.
// Suitable use cases:
// * the cirucular buffer used for logging - allows for remote reset logging.
// * MQTT in-flight data structures - may required a MQTT server reset as well.
//
// noclear_reset should be called when:
// * Bootloader loads a new image
// * POR/BOR reset - as ram is suspect
//
// The noclear section location/size for the app needs to be agreed upon
// by the bootloader and application.
// The no clear section location for the bootloader should be sepereate
// from the app.  Specifically the noclear section used by the logging
// framework for the bootloader must be placed such that it does not overwrite
// the noclear section for the app.

#include <noclear.h>
#include <string.h>
#include <stdint.h>

extern uint8_t noclear_start__[];
extern uint8_t noclear_end__[];

void noclear_reset(void) {
  memset(noclear_start__, 0, noclear_end__ - noclear_start__);
}
