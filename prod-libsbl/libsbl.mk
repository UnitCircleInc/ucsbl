# © 2023 Unit Circle Inc.
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

$(eval $(call new-artifact))

# List of source files for library
SRC += \
  prod-libsbl/sbl-nrf52840.c \
  prod-libsbl/sbl.c \
  src-rfc8032/sha512.c \
  src-uc/ssp.c \

# Needed so we can get CMSIS functions
INC += \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/modules/nrfx/mdk \


# Addional include directories needed for project
INC += \
  prod-libsbl \
  src-uc \

# A grep regular expression - can also be -f whitelist.txt with
# each line being a regular expression to match
WHITE_LIST := -e " *U \(memmove\|memcpy\|memcmp\|memset\|memchr\)"

$(eval $(call library))

