#!/usr/bin/env python3
"""CAN test utility for DK1 (or Jetson/Linux host with SocketCAN). Uses Python 3 standard library only.

  rx : listen for 0x6F0 (transmitted by Blue Pill), verify CRC-8 + Alive_Counter, print statistics every second
  tx : transmit 0x6F1 every 100 ms (with Alive_Counter + CRC-8) for Blue Pill verification

Examples:
  python3 dk1_can_test.py rx can0
  python3 dk1_can_test.py tx can0 --corrupt 20     # transmit an invalid CRC frame every 20 frames
"""
import argparse, socket, struct, sys, time

ID_BP_TO_DK1, ID_DK1_TO_BP = 0x6F0, 0x6F1
DATAID_BP, DATAID_DK1 = 0xA0, 0xB1
FMT = "=IB3x8s"                      # struct can_frame: id, dlc, pad, data[8]
POLY, INIT, XOROUT = 0x1D, 0xFF, 0xFF

def crc8(data: bytes) -> int:        # CRC-8/SAE-J1850, matches e2e_reference.py and common/e2e_crc8.c
    crc = INIT
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ XOROUT

def frame_crc(data_id: int, payload7: bytes) -> int:
    return crc8(bytes([data_id]) + payload7)

def open_can(ifname: str) -> socket.socket:
    s = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.bind((ifname,))
    return s

def cmd_rx(args):
    s = open_can(args.iface)
    s.settimeout(0.2)
    ok = crc_err = alive_err = other = 0
    last_alive = None
    t0 = time.time(); shown = t0
    print(f"Listening on {args.iface}, waiting for ID 0x{ID_BP_TO_DK1:03X} (Ctrl+C to stop)")
    try:
        while True:
            try:
                raw = s.recv(16)
            except socket.timeout:
                raw = None
            if raw:
                cid, dlc, data = struct.unpack(FMT, raw)
                cid &= 0x1FFFFFFF
                if cid != ID_BP_TO_DK1 or dlc != 8:
                    other += 1
                elif frame_crc(DATAID_BP, data[:7]) != data[7]:
                    crc_err += 1
                else:
                    alive = data[0] & 0x0F
                    if last_alive is not None and alive != (last_alive + 1) & 0x0F:
                        alive_err += 1
                    last_alive = alive
                    ok += 1
            if time.time() - shown >= 1.0:
                shown = time.time()
                print(f"[{shown - t0:6.1f}s] ok={ok} crc_err={crc_err} alive_err={alive_err} other={other}")
    except KeyboardInterrupt:
        pass

def cmd_tx(args):
    s = open_can(args.iface)
    alive, seq, n = 0, 0, 0
    print(f"Transmitting 0x{ID_DK1_TO_BP:03X} on {args.iface} every {args.period_ms} ms (Ctrl+C to stop)")
    nxt = time.time()
    try:
        while True:
            p = bytes([alive & 0x0F, 0x11, 0x22, seq & 0xFF, (seq >> 8) & 0xFF, 0x55, 0x66])
            crc = frame_crc(DATAID_DK1, p)
            n += 1
            if args.corrupt and n % args.corrupt == 0:
                crc ^= 0x01                      # Intentionally corrupt CRC to test validator
            s.send(struct.pack(FMT, ID_DK1_TO_BP, 8, p + bytes([crc])))
            alive = (alive + 1) & 0x0F
            seq = (seq + 1) & 0xFFFF
            nxt += args.period_ms / 1000.0
            time.sleep(max(0.0, nxt - time.time()))
    except KeyboardInterrupt:
        pass

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("rx"); r.add_argument("iface", nargs="?", default="can0"); r.set_defaults(fn=cmd_rx)
    t = sub.add_parser("tx"); t.add_argument("iface", nargs="?", default="can0")
    t.add_argument("--period-ms", type=int, default=100)
    t.add_argument("--corrupt", type=int, default=0, help="Inject corrupted CRC frame every N frames (0 = disabled)")
    t.set_defaults(fn=cmd_tx)
    a = ap.parse_args()
    a.fn(a)

if __name__ == "__main__":
    sys.exit(main())
