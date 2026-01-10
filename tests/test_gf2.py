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
from tools.gf2 import GF2


class Tests(unittest.TestCase):
    def test_gf256(self):
        gf256 = GF2([256, 10, 5, 2, 0])
        a = gf256(
            45336173511836269889408084816807631337429260845285062844452882334097940431673
        )
        b = gf256(
            53564442259709651273587585814486384398306019044608658763095222993226935316161
        )
        d, m = divmod(a, b)
        self.assertEqual(a, d * b + m)
        self.assertNotEqual(a.inverse(), a)
        self.assertEqual(a.inverse() * a, gf256(1))

    def test_gf256_rand(self):
        gf256 = GF2([256, 10, 5, 2, 0])
        for _ in range(1000):
            a = gf256.random()
            if a == gf256(0):
                self.assertEqual(a.inverse(), gf256(0), f"a^-1 * a != 1 ... a: {a}")
            else:
                self.assertEqual(a.inverse() * a, gf256(1), f"a^-1 * a != 1 ... a: {a}")

    def test_gf8(self):
        # Example from https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm
        gf8 = GF2([8, 4, 3, 1, 0])
        self.assertEqual(
            gf8(2**6 + 2**4 + 2**1 + 1).inverse(), gf8(2**7 + 2**6 + 2**3 + 2**1)
        )
