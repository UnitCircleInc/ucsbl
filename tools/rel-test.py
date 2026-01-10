#! /usr/bin/env python3

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

import argparse
import os
import io
import hashlib
import struct
import time
import datetime

import ihex
import pexpect
import nacl.signing
import nacl.encoding
import nacl.secret
import nacl.utils
import nacl.pwhash
import nacl.exceptions
from tqdm import tqdm
from uclog import StreamClient

# TODO Should move genkey, cert, sign, load_hex, save_hex to sbl.py and refactor sbl.py

SIG_BLOCK_SIZE = 512
SIG_SIZE = 64
MAX_WHAT_SIZE = 163  # Needs to match src-uc/signature.h APP_MAX_WHAT

MAX_APP_SIZE = 480 * 1024
TOTAL_FLASH = 1024 * 1024
FLASH_PAGE_SIZE = 4096
BL_SIZE = 32 * 1024
BL_STATE_SIZE = 8192
MANU_DATA_SIZE = 4096

CODE_TYPE_ENC = {" EFI\x00": b"\x00", " MFI\x00": b"\x01", " AFI\x00": b"\x02"}

# Needs to match src-uc/sbl.h
SBL_RSP = {0: "NORMAL", 1: "FIRST_RUN", 2: "RESTORE", 3: "ERROR", 4: "INTERNAL-ERROR"}

# RESETREAS
RR_RESETPIN = 1 << 0
RR_WATCHDOG = 1 << 1
RR_SREQ = 1 << 2
RR_LOCKUP = 1 << 3
RR_OFF = 1 << 16
RR_LPCOMP = 1 << 17
RR_DIF = 1 << 18
RR_NFC = 1 << 19
RR_VBUS = 1 << 20


def decode_what(data):
    # String must be ascii printable (0x20-0x7e inclusive) and null terminated
    printable = bytes(range(0x20, 0x7F))
    if data[-1:] != b"\x00":
        raise ValueError("missing null terminatorin what string")
    if any(b not in printable for b in data[:-1]):
        raise ValueError("bad character in what string")
    return data.decode("ascii")


def what(data):
    # Extract the first NUL terminated "what" string in the data
    try:
        data = data[data.index(b"@(#)") + 4 :]
        return decode_what(data[: data.index(b"\x00") + 1])
    except ValueError:
        print("error: bad/missing what string")
        exit(1)


def genkey():
    return nacl.signing.SigningKey.generate()


def cert(ts, key, signing_key, chain=b""):
    cert = struct.pack("<Q", ts) + key.verify_key.encode() + chain
    return signing_key.sign(cert).signature + cert


def sign(image, ts, signing_key, cert, code_what=None, code_type=None):
    sig, code = image[:SIG_BLOCK_SIZE], image[SIG_BLOCK_SIZE:]
    code_n = len(code)
    sha512 = hashlib.sha512()
    sha512.update(code)
    code_hash = sha512.digest()
    if code_what is None:
        code_what = what(code)
    if code_type is None:
        if code_what[-5:] not in CODE_TYPE_ENC:
            print("error: invalid code type")
            exit(1)
        code_type = code_what[-5:]
    if len(code_type) > 1:
        code_type = CODE_TYPE_ENC[code_type]

    sigdata = (
        struct.pack("<IQ", code_n, ts)
        + code_hash
        + code_type
        + code_what.encode("ascii")
    )
    pad = SIG_BLOCK_SIZE - len(sigdata) - len(cert) - SIG_SIZE
    if pad < 0:
        print("error: internal error")
        exit(1)
    sigdata = sigdata + b"\xff" * pad + cert
    if len(sigdata) != SIG_BLOCK_SIZE - SIG_SIZE:
        print("error: internal error")
        exit(1)

    sig = signing_key.sign(sigdata).signature

    if len(sig) + len(sigdata) != SIG_BLOCK_SIZE:
        print("internal error")
        exit(1)
    return sig + sigdata + code


def config(code, root, manu_data_size, max_app_size):
    if manu_data_size <= 0:
        raise ValueError("manu-data-size must be > 0")
    if manu_data_size % FLASH_PAGE_SIZE != 0:
        raise ValueError(f"manu-data-size must multiple {FLASH_PAGE_SIZE}")

    if max_app_size <= 0:
        raise ValueError("max-app-size must be > 0")
    if max_app_size % FLASH_PAGE_SIZE != 0:
        raise ValueError(f"max-app-size must multiple {FLASH_PAGE_SIZE}")
    if (
        BL_SIZE + BL_STATE_SIZE + manu_data_size + max_app_size * 2 + FLASH_PAGE_SIZE
        > TOTAL_FLASH
    ):
        raise ValueError("error: invalid config for part: out of FLASH")

    # This needs to match src-uc/apputils.c initalization for ROOT_CODE_PK
    def_pk = bytes.fromhex(
        "73bed90ce4a9505ff8235e51fece9d4ddeb0fcd44c48e422f200c6b78bd481bf"
    )

    pk = root.verify_key.encode()

    # The chance of collision is vanishingly small - but we check in case
    # it "magically" occures.
    if pk == def_pk:
        raise ValueError("error: invalid root key - same as default PK")

    if code.count(def_pk) != 1:
        raise ValueError("error: 0 or moret than 1 default pk found")
    idx = code.index(def_pk)
    if any(x != 0 for x in code[idx + 32 : idx + 64]):
        raise Exception("mem config not all zeros")

    # Default nRf52 config
    config = {
        "bl-len": 32768,
        "bl-state": 32768,
        "bl-state-len": 8192,
        "manu-data": 32768 + 8192,
        "manu-data-len": manu_data_size,
        "slot0": 32768 + 8192 + manu_data_size,
        "slot1": 32768 + 8192 + manu_data_size + max_app_size,
        "slot-len": max_app_size,
    }
    config = pk + struct.pack(
        "<IIIIIIII",
        config["bl-len"],
        config["bl-state"],
        config["bl-state-len"],
        config["manu-data"],
        config["manu-data-len"],
        config["slot0"],
        config["slot1"],
        config["slot-len"],
    )
    return code[:idx] + config + code[idx + len(config) :]


def corrupt(data, offset, n, mode="flip"):
    if mode == "flip":
        pat = b"\xff" * n
    elif mode == "flip1":
        pat = b"\xff" + b"\x00" * (n - 1)
    else:
        print(f"unknown mode: {mode}")
        exit(1)
    r = (
        data[:offset]
        + bytes([x ^ y for x, y in zip(data[offset : offset + n], pat, strict=True)])
        + data[offset + n :]
    )
    return r


def load_hex(file):
    with open(file, "rt") as f:
        ss, image = ihex.load(f)
    if len(image) != 1:
        print("error: hex file has gaps or is empty")
        exit(1)
    addr, code = image[0]
    return ss, addr, code


def save_hex(file, ss, addr, code):
    with open(file, "wt") as f:
        ihex.dump(f, ss, [(addr, code)])


def recover():
    try:
        buf = io.StringIO()
        child = pexpect.spawn("nrfjprog -f nrf52 --recover", encoding="utf-8")
        child.logfile = buf
        child.expect(r"Recovering device. This operation might take 30s.\r\n")
        child.expect(r"Erasing user code and UICR flash areas.\r\n")
        child.expect(r"Writing image to disable ap protect.\r\n")
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != 0 or child.signalstatus is not None:
            raise Exception(
                f"unable to recover exit status {child.exitstatus} {child.signalstatus}"
            )
    except Exception as e:
        raise Exception(f"unable to recover {e} {buf.getvalue()}") from e
    finally:
        buf.close()


def program(file):
    try:
        buf = io.StringIO()
        child = pexpect.spawn(
            f'nrfjprog -f nrf52 --program "{file}" --sectorerase --verify --reset',
            encoding="utf-8",
        )
        child.logfile = buf
        child.expect(".*Run.\r\n")
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != 0 or child.signalstatus is not None:
            raise Exception(
                f"unable to recover exit status {child.exitstatus} {child.signalstatus}"
            )
    except Exception as e:
        raise Exception(f"unable to program {file} {e} {buf.getvalue()}") from e
    finally:
        buf.close()


def softreset_(pat, exit_status):
    try:
        buf = io.StringIO()
        child = pexpect.spawn("nrfjprog -f nrf52 --reset", encoding="utf-8")
        child.logfile = buf
        child.expect(pat)
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != exit_status or child.signalstatus is not None:
            raise Exception(
                f"unable to recover exit status {child.exitstatus} {child.signalstatus}"
            )
        return
    except Exception as e:
        raise Exception(f"unable to softreset {e} {buf.getvalue()}") from e
    finally:
        buf.close()


def pinreset_():
    try:
        buf = io.StringIO()
        child = pexpect.spawn("nrfjprog -f nrf52 --pinreset", encoding="utf-8")
        child.logfile = buf
        child.expect(".*reset.\r\n")
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != 0 or child.signalstatus is not None:
            raise Exception(
                f"unable to recover exit status {child.exitstatus} {child.signalstatus}"
            )
        return
    except Exception as e:
        raise Exception(f"unable to pinreset {e} {buf.getvalue()}") from e
    finally:
        buf.close()


def debugreset_():
    try:
        buf = io.StringIO()
        child = pexpect.spawn("nrfjprog -f nrf52 --debugreset", encoding="utf-8")
        child.logfile = buf
        child.expect(".*reset.\r\n")
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != 0 or child.signalstatus is not None:
            raise Exception(
                f"unable to recover exit status {child.exitstatus} {child.signalstatus}"
            )
        return
    except Exception as e:
        raise Exception(f"unable to debugreset {e} {buf.getvalue()}") from e
    finally:
        buf.close()


def load(bl, app):
    last_e = None
    for _ in range(4):
        try:
            recover()
            program(app)
            program(bl)
            time.sleep(1)
            pinreset_()  # Needed so that APP_PROTECT settings are applied
            return
        except Exception as e:
            last_e = e
    raise Exception(f"Failed to load after several attempts {last_e}")


def app_ver(args):
    with StreamClient(args, stream=0) as device:
        m = device.txrx({"cmd": "info-app"}, timeout=1)
        if m is not None and "ver" in m:
            if m["sbl-rsp"] not in SBL_RSP:
                return None, None, None, None, None
            return SBL_RSP[m["sbl-rsp"]], m["ts"], m["ver"], m["sbl-ver"], m["rr"]
        else:
            return None, None, None, None, None


def fw_update(args, data):
    with StreamClient(args, stream=0) as device:
        # Need timeout to be long enough to allow erasing all slot1 pages
        m = device.txrx({"cmd": "fw-start"}, timeout=15)
        if "error" in m:
            print(m)
            exit(1)
        block_size = m["block-size"]
        bar = tqdm(
            total=len(data),
            bar_format="{percentage:3.0f}%|{bar}|{n_fmt}/{total_fmt} ",
            leave=False,
        )
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


def accept(args):
    with StreamClient(args, stream=0) as device:
        _ = device.txrx({"cmd": "accept-image"})


def reject(args):
    with StreamClient(args, stream=0) as device:
        _ = device.txrx({"cmd": "reject-image"})


def watchdog(args):
    with StreamClient(args, stream=0) as device:
        _ = device.txrx({"cmd": "watch-dog"})


def softreset(args, pat=".*Run.\r\n", exit_status=0):
    last_e = None
    for _ in range(4):
        try:
            softreset_(pat, exit_status)
            return
        except Exception as e:
            last_e = e
    raise Exception(f"Failed to softreset after several attempts {last_e}")


def pinreset(args):
    last_e = None
    for _ in range(4):
        try:
            pinreset_()
            return
        except Exception as e:
            last_e = e
    raise Exception(f"Failed to pinreset after several attempts {last_e}")


def debugreset(args):
    last_e = None
    for _ in range(4):
        try:
            debugreset_()
            return
        except Exception as e:
            last_e = e
    raise Exception(f"Failed to debugreset after several attempts {last_e}")


def flash_write(args, addr, val):
    with StreamClient(args, stream=0) as device:
        _ = device.txrx({"cmd": "flash-write", "addr": addr, "val": val})


def flash_erase(args, addr):
    with StreamClient(args, stream=0) as device:
        _ = device.txrx({"cmd": "flash-erase", "addr": addr})


RESET_ACTIONS = {
    "reject": reject,
    "watch-dog": watchdog,
    "soft-reset": softreset,
    "debug-reset": debugreset,
    "pin-reset": pinreset,
}


def do_action(args, step, action):
    if action[0] == "load":
        _, bl, app = action
        print(f"step: {step} loading: {bl} - {app}")
        load(os.path.join(TMPDIR, bl), os.path.join(TMPDIR, app))
        time.sleep(2)
    elif action[0] == "load-bin":
        _, bl, app, desc = action
        print(f"step: {step} loading: {bl} - {desc}")
        save_hex(os.path.join(TMPDIR, "bad.hex"), args.afi_ss, args.afi_addr, app)
        load(os.path.join(TMPDIR, bl), os.path.join(TMPDIR, "bad.hex"))
        time.sleep(2)
    elif action[0] == "expect":
        _, image_rsp, image_ts, image_what, reset_reason = action
        if image_ts is None:
            image_ts_s = "None"
        else:
            image_ts_s = datetime.datetime.fromtimestamp(image_ts, datetime.UTC)
        rsp, ts, what, bwhat, rr = app_ver(args)
        print(f"step: {step} expecting: {image_rsp}")
        if ts is None:
            ts_s = "None"
        else:
            ts_s = datetime.datetime.fromtimestamp(ts, datetime.UTC)
        if rsp != image_rsp:
            raise ValueError(f"rsp: {rsp} != {image_rsp}")
        if ts != image_ts:
            raise ValueError(f"ts: {ts_s} != {image_ts_s}")
        if what != image_what:
            raise ValueError(f"what: {what} != {image_what}")
        if image_what is not None and bwhat != args.bfi_what:
            raise ValueError(f"bfi what: {bwhat} != {args.bfi_what}")
        if reset_reason is not None and rr != reset_reason:
            raise ValueError(f"reset reason: {rr} != {reset_reason}")
    elif action[0] == "fwu":
        _, image, desc = action[:3]
        timeout = action[3] if len(action) > 3 else 10
        print(f"step: {step} fwu: {desc}")
        fw_update(args, image)
        time.sleep(timeout)
    elif action[0] == "accept":
        print(f"step: {step} accept")
        accept(args)
    elif action[0] in RESET_ACTIONS.keys():
        print(f"step: {step} {action[0]}")
        if len(action) == 3:
            RESET_ACTIONS[action[0]](args, action[1], action[2])
        elif len(action) == 2:
            RESET_ACTIONS[action[0]](args, action[1])
        else:
            RESET_ACTIONS[action[0]](args)
        time.sleep(10)
    elif action[0] == "flash-write":
        _, addr, value = action
        print(f"step: {step} flash-write[0x{addr:08x}] = 0x{value:08x}")
        flash_write(args, addr, value)
        time.sleep(10)
    elif action[0] == "flash-erase":
        _, addr = action
        print(f"step: {step} flash-erase[0x{addr:08x}]")
        flash_erase(args, addr)
        time.sleep(10)
    else:
        raise Exception(f"unknown action: {action}")


def test_case(args, desc, actions):
    print()
    print(f"==== RUNNING {desc} ====")
    try:
        for step, action in enumerate(actions):
            do_action(args, step, action)
    except Exception as e:
        print(f"==== FAILED  {desc} ====")
        print(f"     Exception {e}")
        return
    print(f"==== PASSED {desc} ====")


def test_normal(args):
    if args.skip_normal:
        return
    test_case(
        args,
        "test_normal",
        (
            ("load", "bfi.hex", "efi.hex"),
            ("expect", "NORMAL", args.t2, args.efi_what, None),
            ("fwu", args.efi2, "afi2"),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("accept",),
            ("fwu", args.mfi1, "mfi1"),
            ("expect", "FIRST_RUN", args.t4, args.mfi_what, RR_SREQ),
            ("accept",),
            ("fwu", args.mfi2, "mfi2"),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("accept",),
            ("fwu", args.afi1, "afi1"),
            ("expect", "FIRST_RUN", args.t6, args.afi_what, RR_SREQ),
            ("accept",),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("accept",),
        ),
    )


def test_double_update(args):
    if args.skip_double_update:
        return
    test_case(
        args,
        "test_double_update",
        (
            ("load", "bfi.hex", "afi.hex"),
            ("expect", "NORMAL", args.t6, args.afi_what, None),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            # Missing accept
            ("fwu", args.afi3, "afi3 - full image", 30),
            ("expect", "FIRST_RUN", args.t8, args.afi_what, RR_SREQ),
            ("accept",),
            ("debug-reset",),
            ("expect", "NORMAL", args.t8, args.afi_what, RR_SREQ),
            ("reject",),
            ("expect", "NORMAL", args.t8, args.afi_what, RR_SREQ),
        ),
    )


def test_white_box(args):
    if args.skip_white_box:
        return
    test_case(
        args,
        "test_white_box",
        (
            ("load", "bfi.hex", "afi.hex"),
            ("expect", "NORMAL", args.t6, args.afi_what, None),
            (
                "fwu",
                args.afi_bad_key_chain_1,
                "afi primary key signed with incorrect root",
            ),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            (
                "fwu",
                args.afi_bad_key_chain_2,
                "afi secondary key signed with incorrect primary key",
            ),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            (
                "fwu",
                args.afi_bad_key_chain_3,
                "afi image signed with incorrect secondary key",
            ),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            (
                "fwu",
                args.afi_bad_ts1,
                "afi image primary cert timestamp later than secondary",
            ),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            (
                "fwu",
                args.afi_bad_ts2,
                "afi image secondary cert timestamp later than image",
            ),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi_bad_ver, "afi invalid version string"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi_bad_type, "afi invalid image type"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi_bad_extra, "afi extra bytes after image length in slot"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi_bad_length, "afi bad length"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
        ),
    )


def test_corruption(args):
    if args.skip_corruption:
        return
    test_case(
        args,
        "test_corruption",
        (
            ("load", "bfi.hex", "afi.hex"),
            ("expect", "NORMAL", args.t6, args.afi_what, None),
            ("fwu", corrupt(args.afi2, 0, 64), "afi image signature corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 64, 4), "afi image length corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 68, 8), "afi image date corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 76, 64), "afi image hash corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 140, 1), "afi image type corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 141, 163), "afi image what string corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            (
                "fwu",
                corrupt(args.afi2, 304, 64),
                "afi secondary cert signature corrupt",
            ),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 368, 8), "afi secondary cert date corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 376, 32), "afi secondary cert key corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 408, 64), "afi primary cert signature corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 472, 8), "afi primary cert date corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 480, 32), "afi primary cert key corrupt"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 512, 1), "afi code corrupt example 1"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", corrupt(args.afi2, 513, 101), "afi code corrupt example 2"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
        ),
    )


def test_downgrade(args):
    if args.skip_downgrade:
        return
    test_case(
        args,
        "test_downgrade",
        (
            # Verify downgrading (timestamps and image types) not possible
            ("load", "bfi.hex", "efi.hex"),
            ("expect", "NORMAL", args.t2, args.efi_what, None),
            ("fwu", args.dg_efi_bad_ts1, "efi new primary cert ts < cur ts"),
            ("expect", "ERROR", args.t2, args.efi_what, RR_SREQ),
            ("fwu", args.dg_efi_bad_ts2, "efi new secondary cert ts < cur ts"),
            ("expect", "ERROR", args.t2, args.efi_what, RR_SREQ),
            ("fwu", args.dg_efi_bad_ts3, "efi new image ts < cur ts"),
            ("expect", "ERROR", args.t2, args.efi_what, RR_SREQ),
            ("fwu", args.mfi1, "mfi1"),
            ("expect", "FIRST_RUN", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.dg_mfi_efi, "try to downgrade to efi"),
            ("expect", "ERROR", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.dg_mfi_bad_ts1, "mfi new primary cert ts < cur ts"),
            ("expect", "ERROR", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.dg_mfi_bad_ts2, "mfi new secondary cert ts < cur ts"),
            ("expect", "ERROR", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.dg_mfi_bad_ts3, "mfi new image ts < cur ts"),
            ("expect", "ERROR", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.afi1, "afi1"),
            ("expect", "FIRST_RUN", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.dg_afi_efi, "try to downgrade to efi"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.dg_afi_mfi, "try to downgrade to mfi"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.dg_afi_bad_ts1, "afi new primary cert ts < cur ts"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.dg_afi_bad_ts2, "afi new secondary cert ts < cur ts"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.dg_afi_bad_ts3, "afi new image ts < cur ts"),
            ("expect", "ERROR", args.t6, args.afi_what, RR_SREQ),
        ),
    )


def test_recovery(args):
    # Verify that after a FW update with a reset  before "acceptance" causes "recovery" to the original FW.
    if args.skip_recovery:
        return
    test_case(
        args,
        "test_recovery",
        (
            ("load", "bfi.hex", "afi.hex"),
            ("expect", "NORMAL", args.t6, args.afi_what, None),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("reject",),
            (
                "expect",
                "RESTORE",
                args.t6,
                args.afi_what,
                RR_SREQ,
            ),  # SW reset  +setting SBL_CMD to NONE
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("watch-dog",),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ | RR_WATCHDOG),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("debug-reset",),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
        ),
    )


def test_accept(args):
    if args.skip_accept:
        return
    test_case(
        args,
        "test_accept",
        (
            # Verify that after a FW update with a reset  after "acceptance" continues to run New(AFI)
            ("load", "bfi.hex", "afi.hex"),
            ("expect", "NORMAL", args.t6, args.afi_what, None),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("accept",),
            ("debug-reset",),
            ("expect", "NORMAL", args.t7, args.afi_what, RR_SREQ),
        ),
    )


def test_write_erase_afi(args):
    if args.skip_write_erase_afi:
        return
    test_case(
        args,
        "test_write_erase_afi",
        (
            # Write/erase flash cases
            # AFI should fail on writes/erases to BL/manu data/APP areas
            ("load", "bfi.hex", "afi.hex"),
            ("expect", "NORMAL", args.t6, args.afi_what, None),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("flash-write", 0x00000000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("flash-erase", 0x00000000),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("flash-write", 0x0000A000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("flash-erase", 0x0000A000),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("flash-write", 0x0000B000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
            ("fwu", args.afi2, "afi2"),
            ("expect", "FIRST_RUN", args.t7, args.afi_what, RR_SREQ),
            ("flash-erase", 0x0000B000),
            ("expect", "RESTORE", args.t6, args.afi_what, RR_SREQ),
        ),
    )


def test_write_erase_efi(args):
    if args.skip_write_erase_efi:
        return
    test_case(
        args,
        "test_write_erase_efi",
        (
            # EFI should allow write/erase to manu data area,
            # otherwise should fail on writes/erases to BL and APP areas
            ("load", "bfi.hex", "efi.hex"),
            ("expect", "NORMAL", args.t2, args.efi_what, None),
            ("fwu", args.efi2, "efi2"),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("flash-write", 0x00000000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t2, args.efi_what, RR_SREQ),
            ("fwu", args.efi2, "efi2"),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("flash-erase", 0x00000000),
            ("expect", "RESTORE", args.t2, args.efi_what, RR_SREQ),
            ("fwu", args.efi2, "efi2"),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("flash-write", 0x0000A000, 0xAAAAAAAA),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("flash-erase", 0x0000A000),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("flash-write", 0x0000B000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t2, args.efi_what, RR_SREQ),
            ("fwu", args.efi2, "efi2"),
            ("expect", "FIRST_RUN", args.t3, args.efi_what, RR_SREQ),
            ("flash-erase", 0x0000B000),
            ("expect", "RESTORE", args.t2, args.efi_what, RR_SREQ),
        ),
    )


def test_write_erase_mfi(args):
    if args.skip_write_erase_mfi:
        return
    test_case(
        args,
        "test_write_erase_mfi",
        (
            # MFI should allow write/erase to manu data area,
            # otherwise should fail on writes/erases to BL and APP areas
            ("load", "bfi.hex", "mfi.hex"),
            ("expect", "NORMAL", args.t4, args.mfi_what, None),
            ("fwu", args.mfi2, "mfi2"),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("flash-write", 0x00000000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.mfi2, "mfi2"),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("flash-erase", 0x00000000),
            ("expect", "RESTORE", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.mfi2, "mfi2"),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("flash-write", 0x0000A000, 0xAAAAAAAA),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("flash-erase", 0x0000A000),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("flash-write", 0x0000B000, 0xAAAAAAAA),
            ("expect", "RESTORE", args.t4, args.mfi_what, RR_SREQ),
            ("fwu", args.mfi2, "mfi2"),
            ("expect", "FIRST_RUN", args.t5, args.mfi_what, RR_SREQ),
            ("flash-erase", 0x0000B000),
            ("expect", "RESTORE", args.t4, args.mfi_what, RR_SREQ),
        ),
    )


def test_slot0_white_box(args):
    if args.skip_slot0_white_box:
        return
    test_case(
        args,
        "test_slot0_white_box",
        (
            (
                "load-bin",
                "bfi.hex",
                args.afi_bad_key_chain_1,
                "afi primary key signed with incorrect root",
            ),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            (
                "load-bin",
                "bfi.hex",
                args.afi_bad_key_chain_2,
                "afi secondary key signed with incorrect primary key",
            ),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            (
                "load-bin",
                "bfi.hex",
                args.afi_bad_key_chain_3,
                "afi image signed with incorrect secondary key",
            ),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            (
                "load-bin",
                "bfi.hex",
                args.afi_bad_ts1,
                "afi image primary cert timestamp later than secondary",
            ),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            (
                "load-bin",
                "bfi.hex",
                args.afi_bad_ts2,
                "afi image secondary cert timestamp later than image",
            ),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            ("load-bin", "bfi.hex", args.afi_bad_ver, "afi invalid version string"),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            ("load-bin", "bfi.hex", args.afi_bad_type, "afi invalid image type"),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            (
                "load-bin",
                "bfi.hex",
                args.afi_bad_extra,
                "afi extra bytes after image length in slot",
            ),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
            ("load-bin", "bfi.hex", args.afi_bad_length, "afi bad length"),
            (
                "expect",
                None,
                None,
                None,
                None,
            ),
        ),
    )


def test_slot0_corruption(args):
    if args.skip_slot0_corruption:
        return
    test_case(
        args,
        "test_slot0_corruption",
        (
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 0, 64),
                "afi image signature corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 64, 4),
                "afi image length corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 68, 8),
                "afi image date corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 76, 64),
                "afi image hash corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 140, 1),
                "afi image type corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 141, 163),
                "afi image what string corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 304, 64),
                "afi secondary cert signature corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 368, 8),
                "afi secondary cert date corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 376, 32),
                "afi secondary cert key corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 408, 64),
                "afi primary cert signature corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 472, 8),
                "afi primary cert date corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 480, 32),
                "afi primary cert key corrupt",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 512, 1),
                "afi code corrupt example 1",
            ),
            ("expect", None, None, None, None),
            (
                "load-bin",
                "bfi.hex",
                corrupt(args.afi2, 513, 101),
                "afi code corrupt example 2",
            ),
            ("expect", None, None, None, None),
        ),
    )


def test_swd(args):
    if args.skip_swd:
        return
    test_case(
        args,
        "test_swd",
        (
            ("load", "bfi.hex", "afi.hex"),
            ("watch-dog",),  # SoftReset like NVIC_SystemReset is not enough
            # debug-reset will still work as it uses CTRL-AP
            # which is still active after AHB-AP is locked
            # by APPROTECT rom UICR and SW register.
            ("soft-reset", ".*ap-protection is enabled.\r\n", 16),
        ),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser("rel-test")
    parser.add_argument("--host", help="host:port to use when connecting to server")
    parser.add_argument("--target", help="serial port to use when connecting")
    parser.add_argument(
        "--sbl", help="SBL.HEX file to use instead of <bindir>/sbl-rel/sbl-rel.hex"
    )
    parser.add_argument(
        "--manu-data-size",
        default="4096",
        help="manufacturing area data size",
    )
    parser.add_argument(
        "--max-app-size",
        default="491520",
        help="maximum size of an application",
    )
    parser.add_argument(
        "bindir",
        help="directory containing boot loader and EFI, MFI, AFI to test",
    )
    args = parser.parse_args()
    args.raw = False

    # get path to default if --sbl not specified
    if args.sbl is None:
        args.sbl = os.path.join(args.bindir, "sbl-rel", "sbl-rel.hex")

    # Get the baseline (uncustomed/unsigned) images
    bfi_ss, bfi_addr, bfi = load_hex(args.sbl)
    efi_ss, efi_addr, efi = load_hex(os.path.join(args.bindir, "efi", "efi.hex"))
    mfi_ss, mfi_addr, mfi = load_hex(os.path.join(args.bindir, "mfi", "mfi.hex"))
    afi_ss, afi_addr, afi = load_hex(os.path.join(args.bindir, "afi", "afi.hex"))

    # Don't care about race as we are only process using this temp dir
    TMPDIR = os.path.join(args.bindir, "reltest")
    if not os.path.exists(TMPDIR):
        os.makedirs(TMPDIR)

    args.efi = efi
    args.mfi = mfi
    args.afi = afi
    args.afi_ss = afi_ss
    args.afi_addr = afi_addr

    # Remove the null terminator
    args.bfi_what = bfi_what = what(bfi)[:-1]
    args.efi_what = efi_what = what(efi)[:-1]
    args.mfi_what = mfi_what = what(mfi)[:-1]
    args.afi_what = afi_what = what(afi)[:-1]

    print(f"BFI Version: {bfi_what}")
    print(f"EFI Version: {efi_what}")
    print(f"MFI Version: {mfi_what}")
    print(f"AFI Version: {afi_what}")

    # Some keys for signing
    root = nacl.signing.SigningKey.generate()
    primary = nacl.signing.SigningKey.generate()
    secondary = nacl.signing.SigningKey.generate()
    fake = nacl.signing.SigningKey.generate()
    args.root = root
    args.primary = primary
    args.secondary = secondary

    # Customize boot loader
    bfi = config(bfi, root, int(args.manu_data_size), int(args.max_app_size))

    # Some timestmaps
    args.t1 = t1 = 1735689600  # 2025-01-01T00:00:00Z
    args.t2 = t2 = t1 + 1
    args.t3 = t3 = t2 + 1
    args.t4 = t4 = t3 + 1
    args.t5 = t5 = t4 + 1
    args.t6 = t6 = t5 + 1
    args.t7 = t7 = t6 + 1
    args.t8 = t8 = t7 + 1

    # Happy case images
    args.s_cert = s_cert = cert(t1, secondary, primary, cert(t1, primary, root))
    args.efi1 = efi1 = sign(efi, t2, secondary, s_cert)
    args.efi2 = efi2 = sign(efi, t3, secondary, s_cert)
    args.efi3 = efi3 = sign(efi, t7, secondary, s_cert)
    args.mfi1 = mfi1 = sign(mfi, t4, secondary, s_cert)
    args.mfi2 = mfi2 = sign(mfi, t5, secondary, s_cert)
    args.mfi3 = mfi3 = sign(mfi, t7, secondary, s_cert)
    args.afi1 = afi1 = sign(afi, t6, secondary, s_cert)
    args.afi2 = afi2 = sign(afi, t7, secondary, s_cert)
    args.afi3 = afi3 = sign(
        afi + os.urandom(MAX_APP_SIZE - len(afi)), t8, secondary, s_cert
    )

    # Save some starting happy images to be the initial APP
    save_hex(os.path.join(TMPDIR, "bfi.hex"), bfi_ss, bfi_addr, bfi)
    save_hex(os.path.join(TMPDIR, "efi.hex"), efi_ss, efi_addr, efi1)
    save_hex(os.path.join(TMPDIR, "mfi.hex"), mfi_ss, mfi_addr, mfi1)
    save_hex(os.path.join(TMPDIR, "afi.hex"), afi_ss, afi_addr, afi1)

    # White box corruption tests - i.e. data structures are corrupted
    # before signatures are added.
    args.afi_bad_key_chain_1 = sign(
        afi, t7, secondary, cert(t1, secondary, primary, cert(t1, primary, fake))
    )
    args.afi_bad_key_chain_2 = sign(
        afi, t7, secondary, cert(t1, secondary, fake, cert(t1, primary, root))
    )
    args.afi_bad_key_chain_3 = sign(
        afi, t7, fake, cert(t1, secondary, primary, cert(t1, primary, root))
    )

    args.afi_bad_ts1 = sign(
        afi, t7, secondary, cert(t1, secondary, primary, cert(t2, primary, root))
    )
    args.afi_bad_ts2 = sign(
        afi, t7, secondary, cert(t8, secondary, primary, cert(t8, primary, root))
    )
    args.afi_bad_ver = sign(
        afi,
        t7,
        secondary,
        s_cert,
        code_what="x" * MAX_WHAT_SIZE,
        code_type=" AFI\x00",
    )
    args.afi_bad_type = sign(afi, t7, secondary, s_cert, code_type=b"\x03")

    # Images need to be multiple of 4
    args.afi_bad_extra = sign(afi, t7, secondary, s_cert) + b"\xff" * 100 + b"1234"
    args.afi_bad_length = sign(
        afi + os.urandom(MAX_APP_SIZE - len(afi) + 4), t7, secondary, s_cert
    )

    # Images for testing downgrading
    args.dg_efi_bad_ts1 = sign(
        efi, t3, secondary, cert(t1, secondary, primary, cert(t1 - 1, primary, root))
    )
    args.dg_efi_bad_ts2 = sign(
        efi, t3, secondary, cert(t1 - 1, secondary, primary, cert(t1, primary, root))
    )
    args.dg_efi_bad_ts3 = sign(
        efi, t2, secondary, cert(t1, secondary, primary, cert(t1, primary, root))
    )

    args.dg_mfi_efi = sign(
        efi, t5, secondary, cert(t1, secondary, primary, cert(t1, primary, root))
    )
    args.dg_mfi_bad_ts1 = sign(
        mfi, t5, secondary, cert(t1, secondary, primary, cert(t1 - 1, primary, root))
    )
    args.dg_mfi_bad_ts2 = sign(
        mfi, t5, secondary, cert(t1 - 1, secondary, primary, cert(t1, primary, root))
    )
    args.dg_mfi_bad_ts3 = sign(
        mfi, t4, secondary, cert(t1, secondary, primary, cert(t1, primary, root))
    )

    args.dg_afi_efi = sign(
        efi, t7, secondary, cert(t1, secondary, primary, cert(t1, primary, root))
    )
    args.dg_afi_mfi = sign(
        mfi, t7, secondary, cert(t1, secondary, primary, cert(t1, primary, root))
    )
    args.dg_afi_bad_ts1 = sign(
        afi, t7, secondary, cert(t1, secondary, primary, cert(t1 - 1, primary, root))
    )
    args.dg_afi_bad_ts2 = sign(
        afi, t7, secondary, cert(t1 - 1, secondary, primary, cert(t1, primary, root))
    )
    args.dg_afi_bad_ts3 = sign(
        afi, t6, secondary, cert(t1, secondary, primary, cert(t1, primary, root))
    )

    args.skip_normal = False
    args.skip_double_update = False
    args.skip_white_box = False
    args.skip_corruption = False
    args.skip_downgrade = False
    args.skip_recovery = False
    args.skip_accept = False
    args.skip_write_erase_afi = False
    args.skip_write_erase_efi = False
    args.skip_write_erase_mfi = False
    args.skip_slot0_white_box = False
    args.skip_slot0_corruption = False
    args.skip_swd = False

    test_normal(args)
    test_double_update(args)
    test_white_box(args)
    test_corruption(args)
    test_downgrade(args)
    test_recovery(args)
    test_accept(args)
    test_write_erase_afi(args)
    test_write_erase_efi(args)
    test_write_erase_mfi(args)
    test_slot0_white_box(args)
    test_slot0_corruption(args)
    test_swd(args)
