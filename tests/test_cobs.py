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
from tools.cobs import enc, dec, enc_zpe, dec_zpe

# TODO Add tests to ensure that length matches expected value.
# Use 0 - 10 254 length input data with no 0x00's.

# TODO Add tests that cause decode errors - mostly  incorrect segment lengths
# likely resulting from truncation.

# TODO Ensure enc_zpe has equal test coverage to base version

tcs = [
    # From the paper
    (
        bytes.fromhex("4500002C4C79000040064F37"),
        bytes.fromhex("024501042C4C79010540064F37"),
    ),
    # Empty string
    (b"", b"\x01"),
    # Single null byte string
    (b"\x00", b"\x01\x01"),
    # String that ends with null
    (b"123\x00", b"\x04123\x01"),
    # String with no null
    (b"123", b"\x04123"),
    # String of length 1 with no null
    (b"1", b"\x021"),
    # String with null in middle
    (b"123\x00456", b"\x04123\x04456"),
    # String with only nulls
    (b"\x00" * 10, b"\x01" * 11),
    # String that starts with null and has null in middle
    (
        b"\x00" + b"1" * 254 + b"\x00123456",
        b"\x01\xff" + b"1" * 254 + b"\x01\x07123456",
    ),
    # Very long string
    (
        b"0123456789" * 150,
        (
            b"\xff0123456789012345678901234567890123456789012345678901234567890"
            + b"12345678901234567890123456789012345678901234567890123456789012345"
            + b"67890123456789012345678901234567890123456789012345678901234567890"
            + b"123456789012345678901234567890123456789012345678901234567890123"
            + b"\xff4567890123456789012345678901234567890123456789012345678901234"
            + b"56789012345678901234567890123456789012345678901234567890123456789"
            + b"01234567890123456789012345678901234567890123456789012345678901234"
            + b"567890123456789012345678901234567890123456789012345678901234567"
            + b"\xff8901234567890123456789012345678901234567890123456789012345678"
            + b"90123456789012345678901234567890123456789012345678901234567890123"
            + b"45678901234567890123456789012345678901234567890123456789012345678"
            + b"901234567890123456789012345678901234567890123456789012345678901"
            + b"\xff2345678901234567890123456789012345678901234567890123456789012"
            + b"34567890123456789012345678901234567890123456789012345678901234567"
            + b"89012345678901234567890123456789012345678901234567890123456789012"
            + b"345678901234567890123456789012345678901234567890123456789012345"
            + b"\xff6789012345678901234567890123456789012345678901234567890123456"
            + b"78901234567890123456789012345678901234567890123456789012345678901"
            + b"23456789012345678901234567890123456789012345678901234567890123456"
            + b"789012345678901234567890123456789012345678901234567890123456789"
            + b"\xe70123456789012345678901234567890123456789012345678901234567890"
            + b"12345678901234567890123456789012345678901234567890123456789012345"
            + b"67890123456789012345678901234567890123456789012345678901234567890"
            + b"123456789012345678901234567890123456789"
        ),
    ),
    # String that is 1 short of 254 length boundary no nulls
    (bytes(range(1, 254)), b"\xfe" + bytes(range(1, 254))),
    # String that is 1 short of 254 length boundary ending in null
    (bytes(range(1, 254)) + b"\x00", b"\xfe" + bytes(range(1, 254)) + b"\x01"),
    # String that is at 254 length boundary no nulls
    (bytes(range(1, 255)), b"\xff" + bytes(range(1, 255))),
    # String that is at 254 length boundary ending in null
    (bytes(range(1, 255)) + b"\x00", b"\xff" + bytes(range(1, 255)) + b"\x01\x01"),
    # String that is 1 beyond 254 length boundary no nulls
    (bytes(range(1, 256)), b"\xff" + bytes(range(1, 255)) + b"\x02\xff"),
    # String that is 1 beyond 254 length boundary ending in null
    (bytes(range(1, 256)) + b"\x00", b"\xff" + bytes(range(1, 255)) + b"\x02\xff\x01"),
]

tcs_zpe = [
    (
        bytes.fromhex("4500002C4C79000040064F37"),
        bytes.fromhex("E245E42C4C790540064F37"),
    ),
]

tcs_dec = [
    bytes.fromhex("01 01 02 02 01 01 01 01 01"),
    bytes.fromhex("02 24 02 02 01"),
    bytes.fromhex("02 55 02 02 0a 3c 75 6e 6b 6e 6f 77 6e 3e"),
    bytes.fromhex("01 01 02 02 02 01 01 01 01"),
    bytes.fromhex("02 24 02 02 01"),
    bytes.fromhex("02 55 02 02 0a 3c 75 6e 6b 6e 6f 77 6e 3e"),
    bytes.fromhex("01 01 02 02 02 02 01 01 01"),
    bytes.fromhex("02 24 02 02 01"),
    bytes.fromhex("02 55 02 02 0a 3c 75 6e 6b 6e 6f 77 6e 3e"),
    bytes.fromhex("01 01 02 02 02 03 01 01 01"),
    bytes.fromhex("02 24 02 02 01"),
    bytes.fromhex("02 55 02 02 0a 3c 75 6e 6b 6e 6f 77 6e 3e"),
    bytes.fromhex("01 01 02 02 02 04 01 01 01"),
    bytes.fromhex("02 24 02 02 01"),
    bytes.fromhex("02 55 02 02 0a 3c 75 6e 6b 6e 6f 77 6e 3e"),
    bytes.fromhex("01 01 02 02 02 05 01 01 01"),
]


class Tests(unittest.TestCase):
    def test_enc_dec(self):
        for v, e in tcs:
            self.assertEqual(enc(v), e)
            self.assertEqual(dec(e), v)

    def test_enc_dec_zpe(self):
        for v, e in tcs_zpe:
            self.assertEqual(enc_zpe(v), e)
            self.assertEqual(dec_zpe(e), v)

    def test_dec(self):
        for v in tcs_dec:
            self.assertEqual(v, enc(dec(v)))
