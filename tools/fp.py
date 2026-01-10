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

import secrets

prime_fields = sorted(
    [2**x - 1 for x in [2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279]]
    + [2**255 - 19, 2**256 + 297, 2**320 + 27, 2**384 + 231]
)


def prime_field_for(i):
    s = [x for x in prime_fields if i < x]
    if len(s) == 0:
        raise ValueError("unable to find suitable prime field")
    return FP(s[0])


class FP:
    def __init__(self, fp):
        self.fp = fp

    def __call__(self, v):
        return FPFE(v, self)

    def __repr__(self):
        return f"FP({self.fp})"

    def __eq__(self, o):
        if self.__class__ != o.__class__:
            return False
        return self.fp == o.fp

    def random(self):
        return self(secrets.randbelow(self.fp))


def egcd(a, b):
    if a.v == 0:
        return (b, a.fp(0), a.fp(1))
    else:
        d, m = divmod(b.v, a.v)
        d = a.fp(d)
        m = a.fp(m)
        g, y, x = egcd(m, a)
        return (g, x - d * y, y)


class FPFE:
    def __init__(self, v, fp):
        self.fp = fp
        self.field = fp
        if v > fp.fp:
            raise ValueError(f"{v} not a member of {fp}")
        self.v = v

    def __repr__(self):
        return f"FPFE({self.v}, {self.fp})"

    def __eq__(self, o):
        if self.__class__ != o.__class__:
            raise TypeError("Can't compare non FE values")
        return self.fp == o.fp and self.v == o.v

    def __add__(self, o):
        if self.__class__ != o.__class__:
            raise TypeError("Can't add non FE values")
        if self.fp != o.fp:
            raise TypeError("Can't add elements of different FP")
        return self.fp((self.v + o.v) % self.fp.fp)

    def __sub__(self, o):
        if self.__class__ != o.__class__:
            raise TypeError("Can't sub non FE values")
        if self.fp != o.fp:
            raise TypeError("Can't sub elements of different FP")
        return self.fp((self.v - o.v) % self.fp.fp)

    def __mul__(self, o):
        if self.__class__ != o.__class__:
            raise TypeError("Can't mul non FE values")
        if self.fp != o.fp:
            raise TypeError("Can't mul elements of different FP")
        return self.fp((self.v * o.v) % self.fp.fp)

    def inverse(self):
        return egcd(self.fp(self.fp.fp), self)[2]
