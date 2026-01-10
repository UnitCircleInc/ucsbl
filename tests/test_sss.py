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

import unittest
import itertools
from tools import gf2
from tools import fp
from tools.sss import split, join


def test_field(field, k, n):
    for _ in range(100):
        s = field.random()
        while s == field(0):
            s = field.random()
        r = split(s, k, n)

        # Check all permutations
        for j in range(1, n + 1):
            for i, z in enumerate(itertools.permutations(r, j)):
                ss = join(z)
                if j < k:
                    # Should fail
                    if ss == s:
                        print(z)
                else:
                    # Should decode
                    if not (ss == s):
                        print(z)


class Tests(unittest.TestCase):
    def test_fp256(self):
        test_field(fp.prime_field_for(2**256), 3, 5)

    def test_gf256(self):
        test_field(gf2.GF2([256, 10, 5, 2, 0]), 3, 5)
