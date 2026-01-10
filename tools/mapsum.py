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

import os
import argparse
import pandas as pd

parser = argparse.ArgumentParser(description="Rollup sizes using map file")
parser.add_argument(
    "--detail", default=1, choices=(1, 2, 3), type=int, help="detail level to print"
)
parser.add_argument("--full", action="store_true", help="full")
parser.add_argument("map_file", help="map file to rollup")
args = parser.parse_args()

df = pd.DataFrame(columns=["source", "section", "name", "size"])
idx = 0

with open(args.map_file, "rt") as f:
    lines = iter(f)

    # Skip all "discarded sections"
    for line in lines:
        if line.strip() == "Linker script and memory map":
            break

    output_section = None
    split_line = None
    for line in lines:
        line = line.strip("\n")
        if line.startswith("Cross Reference Table"):
            break
        if len(line) == 0:
            continue
        if line.startswith(" *("):
            # Input section script "comment"
            continue
        if line.startswith(" *crtbegin"):
            # Input section script "comment"
            continue
        if line.startswith("LOAD"):
            # Input file script "comment"
            continue
        if split_line:
            # Glue a line that was split in two back together
            if line.startswith(" " * 16) and len(line[16:]) > 0 and line[16] != " ":
                line = split_line + line
            else:  # Shouldn't happen
                print("Warning: discarding line ", split_line)
            split_line = None
        else:
            pieces = line.split(None, 3)  # Don't split paths containing spaces
            if len(pieces) == 1 and len(line) > 14:
                # ld splits the rest of this line onto the next if the section name is too long
                split_line = line
                continue

        pieces = line.split(None, 3)  # Don't split paths containing spaces
        if line.startswith("."):
            # This is an output section name
            # Note: this line might be wrapped, with the size of the section
            # on the next line, but we ignore the size anyway and will ignore that line
            output_section = pieces[0]
            input_section = "<linker>"
            source = "<linker>"
            size = 0
        else:
            if output_section not in [".text", ".rodata", ".data", ".bss"]:
                continue
            if "=" in pieces:
                # this is a variable definition
                continue
            if "before" in pieces:
                # this is a comment (size before relaxing)
                continue
            if not line.startswith(" *fill*"):
                if not line.startswith(" ."):
                    # These are sub entries of input section - usually func/variable name
                    continue

            if pieces[0] == "*fill*":
                input_section = pieces[0]
                size = int(pieces[-1], 16)
            else:
                source = pieces[-1]
                input_section = pieces[0]  # .split('.')[-1]
                size = int(pieces[-2], 16)

            if input_section.startswith(".glue"):
                continue

            if ".a(" in source:
                # path/to/archive.a(object.o)
                source = source[: source.index(".a(") + 2]
                source = "[gcc]/" + os.path.basename(source)

            if "arm-none-eabi" in source:
                source = "[gcc]/" + os.path.basename(source)

            if input_section.startswith(".text."):
                input_section = input_section[6:]
            elif input_section.startswith(".bss."):
                input_section = input_section[5:]
            elif input_section.startswith(".rodata."):
                input_section = input_section[8:]
            elif input_section.startswith(".data."):
                input_section = input_section[6:]

            df.loc[idx] = [source, output_section, input_section, size]
            idx += 1

bysection = df.groupby("section")["size"].agg("sum")

if args.full:
    for section in [".text", ".rodata", ".data", ".bss"]:
        print(f"{section:40s} {bysection[section]:10d}")
        if args.detail <= 1:
            continue
        source_df = df.loc[df["section"] == section]
        bysource = (
            source_df.groupby("source")["size"].sum().sort_values(ascending=False)
        )
        for idx, item in bysource.items():
            print(f"  {idx:40s} {item:10d} {item / bysection[section] * 100:7.2f}%")
            if args.detail <= 2:
                continue
            source = source_df.loc[df["source"] == idx].sort_values(
                by="size", ascending=False
            )
            for idx, row in source.iterrows():
                print(f"    {row['name']:40s} {row['size']:10d}")
else:
    for section in [".text", ".rodata", ".data", ".bss"]:
        print(f"{section:40s} {bysection[section]:10d}")
        if args.detail <= 1:
            continue
        source_df = df.loc[df["section"] == section]
        source_df = source_df.sort_values(by="size", ascending=False)
        for idx, row in source_df.iterrows():
            print(
                f" {row['name']:40s} {row['size']:10d} {row['size'] / bysection[section] * 100:7.2f}%"
            )
