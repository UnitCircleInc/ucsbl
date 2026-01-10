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

import subprocess  # nosec

dirs = {
    "nRF SDK": "src-nordic",
    "RFC8032": "src-rfc8032",
}


def run(cmd):
    return subprocess.check_output(cmd.split()).decode().split("\n")[0]  # nosec


repo = run("git remote -v").split(" ")[0].split("\t")[1]
sha = run("git rev-parse HEAD")

print("GIT Repo:")
print(f"  {repo}")
print(f"  {sha}")

for k, v in dirs.items():
    print(f"{k}:")
    with open(f"{v}/soup.txt", "rt") as f:
        for line in f.readlines():
            print(f"  {line.strip()}")
