#!/usr/bin/env python3
"""Reference implementation of the project's E2E protection (Python).

Every frame in can_matrix.dbc carries:
  * Alive_Counter: 4-bit counter, +1 per transmitted frame, wraps 15 -> 0
  * CRC8 in the LAST payload byte: CRC-8/SAE-J1850
        poly 0x1D, init 0xFF, no reflection, xorout 0xFF
        computed over [E2E_DataID] + payload[0 : DLC-1]
E2E_DataID is a per-message constant stored in the DBC (attribute E2E_DataID).

Use it as: (1) the specification the C code on the ECUs must match,
(2) the checker on the Jetson, (3) a source of test vectors for ztest.
Run `python3 e2e_reference.py` to execute the self-test.
"""
import sys
import cantools

POLY, INIT, XOROUT = 0x1D, 0xFF, 0xFF
MAX_DELTA = 3          # counter jumps of 2..MAX_DELTA are accepted as "some frames lost"


def crc8(data: bytes) -> int:
    crc = INIT
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ XOROUT


def data_id(msg) -> int:
    return msg.dbc.attributes["E2E_DataID"].value


def frame_crc(msg, data: bytes) -> int:
    return crc8(bytes([data_id(msg)]) + bytes(data[:-1]))


def protect(db, name: str, values: dict, counter: int) -> bytes:
    """Encode a frame with Alive_Counter and CRC8 filled in."""
    msg = db.get_message_by_name(name)
    assert msg.signals[-1].name == "CRC8" and msg.signals[-1].start == (msg.length - 1) * 8
    data = bytearray(msg.encode({**values, "Alive_Counter": counter & 0xF, "CRC8": 0}))
    data[-1] = frame_crc(msg, bytes(data))
    return bytes(data)


class E2EChecker:
    """Receiver-side check. Returns one of:
    OK, OK_SOME_LOST, INITIAL, REPEATED, WRONG_SEQUENCE, WRONG_CRC, WRONG_LENGTH.
    Only OK, OK_SOME_LOST and INITIAL frames may be used by the application."""

    ACCEPT = {"OK", "OK_SOME_LOST", "INITIAL"}

    def __init__(self, db):
        self.db = db
        self.last = {}

    def check(self, frame_id: int, data: bytes) -> str:
        msg = self.db.get_message_by_frame_id(frame_id)
        if len(data) != msg.length:
            return "WRONG_LENGTH"
        if data[-1] != frame_crc(msg, data):
            return "WRONG_CRC"
        cnt = msg.decode(bytes(data), decode_choices=False)["Alive_Counter"]
        prev = self.last.get(frame_id)
        if prev is not None and cnt == prev:
            return "REPEATED"
        self.last[frame_id] = cnt
        if prev is None:
            return "INITIAL"
        delta = (cnt - prev) & 0xF
        if delta == 1:
            return "OK"
        return "OK_SOME_LOST" if delta <= MAX_DELTA else "WRONG_SEQUENCE"


def _selftest(dbc_path: str) -> None:
    assert crc8(b"123456789") == 0x4B, "CRC-8/SAE-J1850 check value"
    db = cantools.database.load_file(dbc_path, strict=True)
    ids = set()
    for m in db.messages:
        assert data_id(m) not in ids, "E2E_DataID must be unique"
        ids.add(data_id(m))
        base = {s.name: 0 for s in m.signals if s.name not in ("Alive_Counter", "CRC8")}
        chk = E2EChecker(db)
        f0 = protect(db, m.name, base, 0)
        assert chk.check(m.frame_id, f0) == "INITIAL"
        assert chk.check(m.frame_id, protect(db, m.name, base, 1)) == "OK"
        assert chk.check(m.frame_id, protect(db, m.name, base, 1)) == "REPEATED"
        assert chk.check(m.frame_id, protect(db, m.name, base, 3)) == "OK_SOME_LOST"
        assert chk.check(m.frame_id, protect(db, m.name, base, 9)) == "WRONG_SEQUENCE"
        bad = bytearray(protect(db, m.name, base, 10)); bad[0] ^= 0x01
        assert chk.check(m.frame_id, bytes(bad)) == "WRONG_CRC"
        assert chk.check(m.frame_id, f0[:-1]) == "WRONG_LENGTH"
    print(f"self-test OK: CRC check value 0x4B, {len(db.messages)} messages")


if __name__ == "__main__":
    _selftest(sys.argv[1] if len(sys.argv) > 1 else "can_matrix.dbc")
