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

CC      := $(ARM_GCC_ROOT)arm-none-eabi-gcc
CPP     := $(ARM_GCC_ROOT)arm-none-eabi-cpp
LD      := $(ARM_GCC_ROOT)arm-none-eabi-ld
OBJDUMP := $(ARM_GCC_ROOT)arm-none-eabi-objdump
OBJCOPY := $(ARM_GCC_ROOT)arm-none-eabi-objcopy
SIZE    := $(ARM_GCC_ROOT)arm-none-eabi-size
AR      := $(ARM_GCC_ROOT)arm-none-eabi-ar
NM      := $(ARM_GCC_ROOT)arm-none-eabi-nm
$(if $(shell $(CC) --version),,$(error Can't find compiler $(CC) - try setting $$(ARM_GCC_ROOT) or add to path))

# Use ccache if available and on path
CCACHE  := $(CCACHE_ROOT)ccache
$(if $(shell $(CCACHE) --version 2> /dev/null),$(eval CC := $(CCACHE) $(CC)))

ASMFLAGS += -MMD -g3

CFLAGS  += \
  -MMD -std=c11 \
  -Os -g3 \
  -fno-common -fmessage-length=0 -fno-exceptions -fanalyzer \
  -ffunction-sections -fdata-sections \
  -nostdlib -ffreestanding \
  -fno-builtin-printf -fstack-usage \
  -Wall -Wextra -Wformat=2 -Wtrampolines -Werror \
  -Wfloat-equal -fstack-protector-strong \

# Can be used for debugging, but disables ccache which slows down regular builds
#  -save-temps \
#  -fdump-rtl-dfinish \

LDFLAGS += -Wl,--gc-sections,-z,max-page-size=4096 -static -Wl,--cref

# $(CFLAGS) expands to current "artifacts" CFLAGS
# $$(XFLAGS) is an optional set of extra flags that can be set on
# a specfic target
define compile_c #src
  $(BIN)/$(ARTIFACT)/$1.o: $1 $$(filter-out %.d,$$(MAKEFILE_LIST)) | $(patsubst %/, %, $(dir $(BIN)/$(ARTIFACT)/$1))
	@echo $$@
	@$(CC) $(CFLAGS) $(INC) $$(XFLAGS) -c -o $$@ $$<
endef

define compile_S #src
  $(BIN)/$(ARTIFACT)/$1.o: $1 $$(filter-out %.d,$$(MAKEFILE_LIST)) | $(patsubst %/, %, $(dir $(BIN)/$(ARTIFACT)/$1))
	@echo $$@
	@$(CC) $(ASMFLAGS) $(INC) $$(XFLAGS) -c -o $$@ $$<
endef

# Build the linker description file by using cpp to allow for
#  #defines etc
#  #include
#
# -P    - omit generating #line markers
# -CC   - do not discard comments - to handle cases like following
#         (as the /* starts a comment)
#               KEEP(*path/*.o(.rodata .rodata*))
# -MMD - to generate dependencies - which need to be fixed up :-(
# -o   - to set output file
# -I   - to set default include path
#
#  $(LDFILE_FLAGS) - to allow passing -Dxxx to the $(LDFILE)
#
# CPP command line options: https://gcc.gnu.org/onlinedocs/cpp/Invocation.html

define build-ld
  $(BIN)/$(ARTIFACT)/$(ARTIFACT).ld: $(LDFILE) $$(filter-out %.d,$$(MAKEFILE_LIST))
	@echo $$@
	@$(CPP) -CC -P $(LDFILE_FLAGS) -I. -MMD -o $$@ $(LDFILE)
	@sed -i.bak -e s?^$(basename $(notdir $(LDFILE))).o:?$$@:? $$(@:.ld=.d)

-include $(BIN)/$(ARTIFACT)/$(ARTIFACT).d
endef

define link # objs
  $(BIN)/$(ARTIFACT)/$(ARTIFACT).elf: $1 $(LIBS) $(BIN)/$(ARTIFACT)/$(ARTIFACT).ld | $(BIN)/$(ARTIFACT)
	@echo $$@
	@$(CC) $(LDFLAGS) $(filter-out -fstack-protector-strong,$(CFLAGS)) -Wl,-Map=$$(basename $$@).map -T $(BIN)/$(ARTIFACT)/$(ARTIFACT).ld $1 $(LIBS) -o $$@
	@uv run python3 tools/logdata.py --ofile $(BIN)/$(ARTIFACT)/$(ARTIFACT).cbor $$@
	@${OBJCOPY} --add-section .logdata_cbor=$(BIN)/$(ARTIFACT)/$(ARTIFACT).cbor $$@
	@$(SIZE) $$@
endef

define link-lib # objs
  $(BIN)/$(ARTIFACT)/$(ARTIFACT).a: $1 | $(BIN)/$(ARTIFACT)
	@echo $$@
	@$(AR) rcs $$@ $$^
	@$(LD) -r -o $$@.o $$^
	@UNDEFLIST=`$(NM) $$@.o -u | grep -v $(WHITE_LIST)`; if test -n "$${UNDEFLIST}"; then printf "the following undefined sysbols not on whitelist\n"; printf "$${UNDEFLIST}\n"; exit 1; fi
endef

%.bin: %.elf
	@echo $@
	@$(OBJCOPY) -O binary --gap-fill 0x00 $< $@

%.sig: %.bin
	@echo $@
	@uv run python3 tools/ucssm.py --passphrase $(UCSSM_PASSPHRASE) sign --bin $@ $($(notdir $(basename $@))-SIGNING_CERT) $<

%.hex: %.elf
	@echo $@
	@$(OBJCOPY) -O ihex --gap-fill 0x00 $< $@
