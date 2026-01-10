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
import os
from tools.zbase32 import encode, decode


class Tests(unittest.TestCase):
    def test_rand(self):
        # Check every length between 1 and 256 bytes
        for n in range(256):
            # Use random bytes all bytes except the last for which we test all possible values
            for k in range(256):
                m = os.urandom(n) + bytes([k])
                e = encode(m)
                d = decode(e)
                ld = len(e.replace("-", "")) * 5 - len(m) * 8
                self.assertTrue(ld >= 0)
                self.assertTrue(ld < 5)
                self.assertEqual(m, d, "m: {m.hex()} e: {e} d: {d.hex()}")
