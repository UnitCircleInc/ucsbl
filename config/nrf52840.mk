# © 2025 Unit Circle Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CFLAGS += \
  -DBSP_DEFINES_ONLY \
  -DFLOAT_ABI_HARD \
  -DNRF52840_XXAA \
  -DLOG_MAX_PORTS=8 \
  -DNO_QUIRKS -DCONFIGURED=1 -DDEV_MODE -D_MSC_VER=0 \

#CFLAGS += -Wno-error=expansion-to-defined
#CFLAGS += -Wno-expansion-to-defined

ASMFLAGS += \
  -DBSP_DEFINES_ONLY \
  -DFLOAT_ABI_HARD \
  -DNRF52840_XXAA \
  -D__HEAP_SIZE=16 \
  -D__STACK_SIZE=8192 \
  -D__STARTUP_CLEAR_BSS \
  -D__START=main \

include config/arm-m4.mk
include config/arm-gcc.mk

# Add some useful targets for loading code etc.
# NOTE:
#   Recommend using hex files as nrfjprog fills gaps between sections
#   with 0x00 instead of leaving at 0xff when using elf files.
#   Hex (and bin) files generated with objcopy --fill 0xXX force the fill value
#   so that signatures are computed correctly.
#
#   You can see using readelf -S <elffile> and checking for gaps.
#   If there are any then elf loading (vs hex/bin loading)
#   may fail depending on loader "fill" defaults.
#
# In one window run:
#   make CONFIG=nrf52840-dk uclog
# In a second window run:
#   make CONFIG=nrf52840-dk erase load-mfi.signed load-sbl-pk
# Then run:
#   make CONFIG=nrf52840-dk fwu-afi.signed

load-%:
	nrfjprog -f nrf52 --program bin-test-signing/$*.hex --sectorerase --verify --reset

fwu-%:
	uv run python3 tools/fwu.py bin-test-signing/$*.bin

uclog:
	uv run python3 tools/uclog.py --target /dev/cu.usbmodem00`nrfjprog -i`1 -e $(BIN)/efi/efi.elf -e $(BIN)/mfi/mfi.elf -e $(BIN)/afi/afi.elf -e $(BIN)/sbl-dev/sbl-dev.elf -e $(BIN)/sbl-rel/sbl-rel.elf

erase:
	@echo erasing part ...
	@nrfjprog -f nrf52 --recover

reset:
	@echo resetting device ...
	@nrfjprog -f nrf52 -p

erase-app:
	@echo erasing first page of slot0
	@nrfjprog -f nrf52  --erasepage 0xb000
