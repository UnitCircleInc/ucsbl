#! /usr/bin/env python3

# © 2022 Unit Circle Inc.
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

import argparse
from uclog import StreamClient

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", help="host:port to use when connecting to server")
    parser.add_argument("--target", help="serial port to use when connecting")
    parser.add_argument(
        "--raw",
        action="store_true",
        help="with serial port use direct - no logging framework",
    )

    args = parser.parse_args()
    with StreamClient(args, stream=0) as device:
        while True:
            # device.tx(args.message)
            m = device.rx(timeout=15)
            if m:
                print(m.decode(), end="")
