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
#
# Build assuptions:
# * Using some form of C compiler
# * There is a default set of CFLAGS, ASMFLAGS, LDFLAGS, CPPFLAGS that
#   are extended by the sw-config and hw-config but not

.PHONY: all artifacts tests clean

all: artifacts

# If clean is the only goal then do not try and load config stuff
# We are trying to erase all dependecis as start form scratch
ifneq (clean,$(MAKECMDGOALS))

# One of the following conditions must be true otherwise an error is returned:
# * make is run as `CONFIG=<config> make`
# * make is run as `make CONFIG=<config>`
# * make is run as `make` and there is only one directory that matches `bin-*`
ifeq ($(origin CONFIG),undefined)
  ifeq ($(words $(wildcard bin-*)),1)
    include $(wildcard bin-*)/config.mk
  else
    $(error CONFIG undefined or too many configs)
  endif
endif

# Load the currently selected config
# This creates base environment variables:
#   CFLAGS, ASMFLAGS, LDFLAGS
#  and macros for:
#   compile-c, compile-S, link, mklib
ifeq ("$(wildcard config/$(CONFIG).mk)","")
  $(error config/$(CONFIG).mk not found)
endif

include config/$(CONFIG).mk

# Capture base environment variables
BASE_CFLAGS   := $(CFLAGS)
BASE_ASMFLAGS := $(ASMFLAGS)
BASE_LDFLAGS  := $(LDFLAGS)
OBJDIRS :=
BUILD :=
BIN := bin-$(CONFIG)

# Record config - allows auto detect if there is only one
$(BIN):
	@echo $@
	@mkdir -p $@

$(BIN)/config.mk: | $(BIN)
	@echo $@
	@echo "CONFIG := $(CONFIG)" > $@

# Update $(BIN)/release.h if git says things have changed or if doesn't exist
platform := $(shell uname -s)
ifeq ($(platform),Linux)
  build_date = $(shell date -u -d @`git log -1 --format=%ct` +"%Y-%m-%dT%H:%M:%SZ")
else ifeq ($(platform),Darwin)
  build_date = $(shell date -u -r `git log -1 --format=%ct` +"%Y-%m-%dT%H:%M:%SZ")
else
  $(error unhandled platform $(platform))
endif
build_branch := $(shell git rev-parse --abbrev-ref HEAD)
build_ver := $(shell git describe --dirty --tags 2> /dev/null || echo "unknown")
build_machine := $(shell uname -a)
build_user := $(shell whoami)

build_all := $(build_ver) $(build_date) $(build_branch) $(build_machine) $(build_user)

build_last := $(shell [ -e $(BIN)/release.h ] &&  cut -d \" -f 2 $(BIN)/release.h  | tr "\n" " " || echo "<unknown-last>")
ifeq ($(strip $(build_all)),$(strip $(build_last)))
else
  $(info $(BIN)/release.h)
  $(shell mkdir -p $(BIN);\
    echo "#define BUILD_VER \"$(build_ver)\"" > $(BIN)/release.h;\
    echo "#define BUILD_DATE \"$(build_date)\"" >> $(BIN)/release.h;\
    echo "#define BUILD_BRANCH \"$(build_branch)\"" >> $(BIN)/release.h;\
    echo "#define BUILD_MACHINE \"$(build_machine)\"" >> $(BIN)/release.h;\
    echo "#define BUILD_USER \"$(build_user)\"" >> $(BIN)/release.h;\
  )
endif

# $(call uniq,words) returns the list of uniq words without sorting
uniq = $(if $1,$(firstword $1) $(call uniq,$(filter-out $(firstword $1),$1)))

define new-artifact
  SRC      :=
  INC      := $(BIN)
  LIBS     :=
  CFLAGS   := $(BASE_CFLAGS)
  ASMFLAGS := $(BASE_ASMFLAGS)
  LDFLAGS  := $(BASE_LDFLAGS)
  LDFILE_FLAGS :=
endef

define mkdir #dir
  $1:
	@echo mkdir $1
	@mkdir -p $1

  -include $1/*.d
endef

define build-objs
  INC := $(addprefix -I ,$(call uniq,$(patsubst %/,%,$(dir $(SRC)))) $(INC))
  OBJS := $(addprefix $(BIN)/$(ARTIFACT)/,$(addsuffix .o,$(SRC)))
  OBJDIRS += $(addprefix $(BIN)/$(ARTIFACT)/,$(call uniq,$(patsubst %/,%,$(dir $(SRC)))))

endef

define add-flags #src-file, flags
$(BIN)/$(ARTIFACT)/$1.o: XFLAGS = $2
endef

define executable
  $(eval $(call build-objs))
  $(eval $(call build-ld))
  $(eval $(foreach src, $(filter %.c,$(SRC)), $(eval $(call compile_c,$(src)))))
  $(eval $(foreach src, $(filter %.S,$(SRC)), $(eval $(call compile_S,$(src)))))
  $(eval $(call link,$(OBJS) $(LIBS)))
  $(ARTIFACT): $(BIN)/$(ARTIFACT)/$(ARTIFACT).elf $(BIN)/$(ARTIFACT)/$(ARTIFACT).bin $(BIN)/$(ARTIFACT)/$(ARTIFACT).hex

  BUILD += $(ARTIFACT)
endef

define library
  $(eval $(call build-objs))
  $(eval $(foreach src, $(filter %.c,$(SRC)), $(eval $(call compile_c,$(src)))))
  $(eval $(foreach src, $(filter %.S,$(SRC)), $(eval $(call compile_S,$(src)))))
  $(eval $(call link-lib,$(OBJS)))
  $(ARTIFACT): $(BIN)/$(ARTIFACT)/$(ARTIFACT).a

  BUILD += $(ARTIFACT)
endef


define load-mk #<mkfile>
  $(eval ARTIFACT := $(strip $(basename $(notdir $(1)))))
  $(eval $(call mkdir,$(BIN)/$(ARTIFACT)))
  $(eval include $1)
endef

# Load artifact generating configurations
$(foreach mkfile,$(wildcard prod-*/*.mk),$(eval $(call load-mk,$(mkfile))))

# Create rules to ensure object directories get built and any .d dependency files get loaded
$(foreach objdir,$(OBJDIRS),$(eval $(call mkdir,$(objdir))))

# Tests get complicated
# * Maybe build an artifact for a embedded target (or QEMU) or an artifact for local machine testing
# * The run run the test which might involve running on a target or QEMU or just a local executable
#
# so maybe source files are in test-<hw>-<name> where hw can be a embedded target or "qemu" or "host" only tests that match the <hw>-
# host means use gcc (host/native compile), use arm-gcc (cross compile)

artifacts: $(BIN)/config.mk $(BUILD)

endif

clean:
	@echo rm -rf bin-*
	@rm -rf bin-*


release: sbl-rel sbl-dev libsbl
	@echo releasing $(build_ver)
	@rm -rf release/$(build_ver)
	@mkdir -p release/$(build_ver)
	@mkdir -p release/$(build_ver)/dev
	@mkdir -p release/$(build_ver)/rel
	@mkdir -p release/$(build_ver)/lib
	@mkdir -p release/$(build_ver)/tools
	@cp $(BIN)/sbl-rel/sbl-rel.elf release/$(build_ver)/rel
	@cp $(BIN)/sbl-rel/sbl-rel.hex release/$(build_ver)/rel
	@cp $(BIN)/sbl-rel/sbl-rel.bin release/$(build_ver)/rel
	@cp $(BIN)/sbl-rel/sbl-rel.map release/$(build_ver)/rel
	@cp $(BIN)/sbl-rel/sbl-rel.cbor release/$(build_ver)/rel
	@cp $(BIN)/sbl-dev/sbl-dev.elf release/$(build_ver)/dev
	@cp $(BIN)/sbl-dev/sbl-dev.hex release/$(build_ver)/dev
	@cp $(BIN)/sbl-dev/sbl-dev.bin release/$(build_ver)/dev
	@cp $(BIN)/sbl-dev/sbl-dev.map release/$(build_ver)/dev
	@cp $(BIN)/sbl-dev/sbl-dev.cbor release/$(build_ver)/dev
	@cp $(BIN)/libsbl/libsbl.a release/$(build_ver)/lib
	@cp src-uc/sbl.h release/$(build_ver)/lib
	@cp tools/sbl.py release/$(build_ver)/tools
	@cp tools/sss.py release/$(build_ver)/tools
	@cp tools/gf2.py release/$(build_ver)/tools
	@cp tools/zbase32.py release/$(build_ver)/tools
	@cp tools/ihex.py release/$(build_ver)/tools
	@cp tools/eff_short_wordlist_2_0.txt release/$(build_ver)/tools
	@echo "-- Build Environment --" > release/$(build_ver)/$(build_ver).build_env
	@printf "Ubuntu: %s %s %s\n" `sh -c "if command -v lsb_release > /dev/null; then lsb_release -ds; else echo missing-lsb_release; fi"` >> release/$(build_ver)/$(build_ver).build_env
	@echo "Machine: `uname -a`" >> release/$(build_ver)/$(build_ver).build_env
	@echo "GCC: `arm-none-eabi-gcc --version`" >> release/$(build_ver)/$(build_ver).build_env
	@echo "Python: `python3 --version`" >> release/$(build_ver)/$(build_ver).build_env
	@echo "-- Build Details --" >> release/$(build_ver)/$(build_ver).build_env
	@cat $(BIN)/release.h >> release/$(build_ver)/$(build_ver).build_env
	@echo "-- SOUP report --" >> release/$(build_ver)/$(build_ver).build_env
	@uv run python3 tools/sbom.py >> release/$(build_ver)/$(build_ver).build_env
	@cat release/$(build_ver)/$(build_ver).build_env
