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
from tools.fp import prime_field_for


class Tests(unittest.TestCase):
    def test_fp_rand(self):
        fp = prime_field_for(2**256)
        for _ in range(1000):
            a = fp.random()
            if a == fp(0):
                self.assertEqual(a.inverse(), fp(0), f"a^-1 * a != 1 ... a: {a}")
            else:
                self.assertEqual(a.inverse() * a, fp(1), f"a^-1 * a != 1 ... a: {a}")
