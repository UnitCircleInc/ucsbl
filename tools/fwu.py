#! /usr/bin/env python3

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

import argparse
import time
from tqdm import tqdm

# from datetime import datetime as dt
# from datetime import timezone as tz
from uclog import StreamClient

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", help="host:port to use when connecting to server")
    parser.add_argument("--target", help="serial port to use when connecting")

    parser.add_argument("file", help="new firmware image to update to")
    args = parser.parse_args()
    args.raw = False
    with StreamClient(args, stream=0) as device:
        if args.file == "accept":
            m = device.txrx({"cmd": "accept-image"})
            print(m)
        elif args.file == "reject":
            m = device.txrx({"cmd": "reject-image"})
            print(m)
        elif args.file == "watch-dog":
            m = device.txrx({"cmd": "watch-dog"})
            print(m)
        elif args.file == "write-mfi":
            m = device.txrx({"cmd": "flash-write", "addr": 0xA000, "val": 0xAAAAAAAA})
            print(m)
        elif args.file == "erase-mfi":
            m = device.txrx({"cmd": "flash-erase", "addr": 0xA000})
            print(m)
        elif args.file == "write-sbl":
            m = device.txrx({"cmd": "flash-write", "addr": 0x0000, "val": 0xAAAAAAAA})
            print(m)
        elif args.file == "erase-sbl":
            m = device.txrx({"cmd": "flash-erase", "addr": 0x0000})
            print(m)
        elif args.file == "write-app":
            m = device.txrx({"cmd": "flash-write", "addr": 0xB000, "val": 0xAAAAAAAA})
            print(m)
        elif args.file == "erase-app":
            m = device.txrx({"cmd": "flash-erase", "addr": 0xB000})
            print(m)
        elif args.file == "info-app":
            m = device.txrx({"cmd": "info-app"})
            print(m)
        elif args.file == "fwu-schedule":
            s = int(time.time() + 5)
            m = device.txrx({"cmd": "fw-schedule", "timestamp": s})
            if "error" in m:
                print(m)
                exit(1)
        elif args.file == "stop":
            m = device.txrx({"cmd": "stop"})
            print(m)
        else:
            # Need timeout to be long enough to allow erasing all slot1 pages
            m = device.txrx({"cmd": "fw-start"}, timeout=15)
            if "error" in m:
                print(m)
                exit(1)
            block_size = m["block-size"]
            with open(args.file, "rb") as f:
                data = f.read()

            with tqdm(
                total=len(data),
                bar_format="{percentage:3.0f}%|{bar}|{n_fmt}/{total_fmt} ",
            ) as bar:
                while len(data) > 0:
                    block, data = data[:block_size], data[block_size:]
                    m = device.txrx({"cmd": "fw-block", "data": block}, 1)
                    if "error" in m:
                        print(m)
                        exit(1)
                    bar.update(len(block))
            # s = dt.fromtimestamp(int(time.time()+5), tz = tz.utc)
            # print(s.timestamp())
            s = int(time.time() + 5)
            m = device.txrx({"cmd": "fw-schedule", "timestamp": s})
            if "error" in m:
                print(m)
                exit(1)
