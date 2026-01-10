#! /usr/bin/env zsh

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


set -euo pipefail

handle_error() {  echo "Error on line $1"  }

trap 'handle_error $LINENO' ERR

if [[ "${TRACE-0}" == "1" ]]; then
  set -x
fi

if [[ "${1-}" =~ ^-*h(elp)?$ ]] || [ $# -ne  2 ]; then
  echo 'Usage: [TRACE=1] ./test-signing.sh [sbl-to-test.hex] [mfi-afi-dir]'
  exit
fi

echo Running test-signing for $1 $2

SBL=bin-test-signing/${1%.hex}-pk.hex

mkdir -p bin-test-signing

echo Generating a set of keys and corresponding certs
if [ ! -f tools/.sbl/test-root.pub ]; then
  echo "--test-root"
  tools/sbl.py keygen --test --split=3,5 test-root
fi
if [ ! -f tools/.sbl/test-l1.pub ]; then
  echo "--test-l1"
  tools/sbl.py keygen --test --split=3,5 test-l1
fi
if [ ! -f tools/.sbl/test-l2.pub ]; then
  echo "--test-l2"
  tools/sbl.py keygen --test --split=3,5 test-l2
fi
if [ ! -f bin-test-signing/test-l2.cert ]; then
  echo "--test-l1.cert"
  tools/sbl.py certgen --key test-root.1:1 --key test-root.3:3 --key test-root.5:5 --pub test-l1 bin-test-signing/test-l1.cert
  sleep 2  # ensure that timestamps are different
  echo "--test-l2.cert"
  tools/sbl.py certgen --key test-l1.1:1 --key test-l1.3:3 --key test-l1.5:5 --pub test-l2 --chain bin-test-signing/test-l1.cert bin-test-signing/test-l2.cert
fi

echo Customizing SBL to the generated root PK
echo "--sbl-pk.hex"
cp $1 bin-test-signing/sbl-pk.hex
tools/sbl.py config --root test-root bin-test-signing/sbl-pk.hex

echo Signing the apps using the keys/certs
echo "--mfi.signed.hex"
tools/sbl.py sign --key test-l2.1:1 --key test-l2.2:2 --key test-l2.5:5 --code $2/mfi/mfi.hex --cert bin-test-signing/test-l2.cert bin-test-signing/mfi.signed.hex
arm-none-eabi-objcopy -I ihex -O binary bin-test-signing/mfi.signed.hex bin-test-signing/mfi.signed.bin
sleep 2  # ensure that timestamps are different
echo "--afi.signed.bin"
tools/sbl.py sign --key test-l2.1:1 --key test-l2.2:2 --key test-l2.5:5 --code $2/afi/afi.bin --cert bin-test-signing/test-l2.cert bin-test-signing/afi.signed.bin

echo Verifing that everything was customized/signed correctly
tools/sbl.py config --root test-root --verify bin-test-signing/sbl-pk.hex
echo "--mfi.signed.hex"
tools/sbl.py --debug verify --root test-root bin-test-signing/mfi.signed.hex
echo "--mfi.signed.bin"
tools/sbl.py --debug verify --root test-root bin-test-signing/mfi.signed.bin
echo "--afi.signed.bin"
tools/sbl.py --debug verify --root test-root bin-test-signing/afi.signed.bin

export TIMESTAMP=`date -u +%s`
tools/sbl.py sign --key test-l2.1:1 --date $TIMESTAMP --key test-l2.2:2 --key test-l2.5:5 --code $2/mfi/mfi.bin --cert bin-test-signing/test-l2.cert bin-test-signing/mfi-ts.signed.bin
tools/sbl.py sign --key test-l2.1:1 --date $TIMESTAMP --key test-l2.2:2 --key test-l2.5:5 --code $2/mfi/mfi.hex --cert bin-test-signing/test-l2.cert bin-test-signing/mfi-ts.signed.hex
tools/sbl.py --debug verify --root test-root bin-test-signing/mfi-ts.signed.bin
tools/sbl.py --debug verify --root test-root bin-test-signing/mfi-ts.signed.hex

# See config/nrf52840.mk for useful target for loading
# In one window run:
#   make CONFIG=nrf52840-dk uclog
# In a second window run:
#   make CONFIG=nrf52840-dk erase load-mfi.signed load-sbl-pk
# Then run:
#   make CONFIG=nrf52840-dk fwu-afi.signed
