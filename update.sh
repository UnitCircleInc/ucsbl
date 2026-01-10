#! /bin/sh

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

# Instructions:
# Download SDK 17.1.0 from https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download and place into direcctory called `nordic`.
# Then run `./update.sh`, this will copy the needed files from the SDK into
# `src-nordic`.

set -e

SRC=nordic/nRF5_SDK_17.1.0_ddde560
DST=src-nordic

rm -rf $DST
mkdir -p $DST

FILES=(\
components/drivers_nrf/nrf_soc_nosd/nrf_error.h \
components/libraries/delay/nrf_delay.h \
components/libraries/util/app_error.h \
components/libraries/util/app_error_weak.h \
components/libraries/util/app_util.h \
components/libraries/util/app_util_platform.h \
components/libraries/util/nordic_common.h \
components/libraries/util/nrf_assert.h \
components/libraries/util/sdk_errors.h \
components/libraries/util/sdk_resources.h \
components/toolchain/cmsis/include/cmsis_compiler.h \
components/toolchain/cmsis/include/cmsis_gcc.h \
components/toolchain/cmsis/include/cmsis_version.h \
components/toolchain/cmsis/include/core_cm4.h \
components/toolchain/cmsis/include/mpu_armv7.h \
integration/nrfx/legacy/apply_old_config.h \
integration/nrfx/nrfx_config.h \
integration/nrfx/nrfx_glue.h \
modules/nrfx/drivers/nrfx_common.h \
modules/nrfx/drivers/nrfx_errors.h \
modules/nrfx/hal/nrf_clock.h \
modules/nrfx/hal/nrf_gpio.h \
modules/nrfx/hal/nrf_nvmc.c \
modules/nrfx/hal/nrf_nvmc.h \
modules/nrfx/hal/nrf_rng.h \
modules/nrfx/hal/nrf_rtc.h \
modules/nrfx/hal/nrf_systick.h \
modules/nrfx/hal/nrf_uarte.h \
modules/nrfx/hal/nrf_wdt.h \
modules/nrfx/mdk/compiler_abstraction.h \
modules/nrfx/mdk/nrf.h \
modules/nrfx/mdk/nrf51_erratas.h \
modules/nrfx/mdk/nrf51_to_nrf52840.h \
modules/nrfx/mdk/nrf52840.h \
modules/nrfx/mdk/nrf52840_bitfields.h \
modules/nrfx/mdk/nrf52840_peripherals.h \
modules/nrfx/mdk/nrf52_erratas.h \
modules/nrfx/mdk/nrf52_to_nrf52840.h \
modules/nrfx/mdk/nrf53_erratas.h \
modules/nrfx/mdk/nrf91_erratas.h \
modules/nrfx/mdk/nrf_erratas.h \
modules/nrfx/mdk/nrf_peripherals.h \
modules/nrfx/mdk/system_nrf52.c \
modules/nrfx/mdk/system_nrf52.h \
modules/nrfx/mdk/system_nrf52840.c \
modules/nrfx/mdk/system_nrf52840.h \
modules/nrfx/mdk/system_nrf52_approtect.h \
modules/nrfx/nrfx.h \
modules/nrfx/soc/nrfx_atomic.h \
modules/nrfx/soc/nrfx_coredep.h \
modules/nrfx/soc/nrfx_irqs.h \
modules/nrfx/soc/nrfx_irqs_nrf52840.h \
modules/nrfx/mdk/gcc_startup_nrf52840.S \
modules/nrfx/mdk/system_nrf52840.c \
modules/nrfx/hal/nrf_nvmc.c \
)

for FILE in "${FILES[@]}"
do
  mkdir -p $DST/$(dirname $FILE)
  cp $SRC/$FILE $DST/$FILE
done

echo ${SRC##*/} from "https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download" > $DST/soup.txt
