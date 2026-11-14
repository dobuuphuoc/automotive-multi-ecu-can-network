# CAN Bring-Up: Blue Pill ↔ DK1 (500 kbps, CAN 2.0A)

## A. Wiring (Bench Setup without Carrier Board)
Use one SN65HVD230 (3.3 V) transceiver module at each bus end. Connect CANH–CANH, CANL–CANL, with a **common ground (GND)**.
- **Blue Pill**: module `D` (TXD) ← PA12, `R` (RXD) → PA11, VCC = 3.3 V, GND.
- **DK1**: module `D` ← pin FDCAN1_TX (PD1), `R` → pin FDCAN1_RX (PA11), VCC = 3.3 V, GND. Refer to the DK1 user manual for connector header pinouts (search "FDCAN1").
- **120 Ω Termination**: Required **only at both ends of the bus**. Many breakout modules have pre-soldered 120 Ω resistors. With power OFF, measure resistance across CANH–CANL: it must read ≈ 60 Ω (two 120 Ω resistors in parallel). A reading of 120 Ω indicates only one termination resistor is present; 40 Ω means an extra resistor is attached.
- Tie the `Rs` pin of the SN65HVD230 to GND (high-speed mode) – pre-configured on most modules.

## B. DK1 Linux Configuration
```bash
ip link show can0                         # If can0 is missing, see section D
ip link set can0 down
ip link set can0 type can bitrate 500000 sample-point 0.833
ip link set can0 up
ip -details -statistics link show can0    # Inspect actual negotiated bitrate and sample-point
candump can0                              # If can-utils is installed
python3 dk1_can_test.py rx can0           # Verify CRC-8 + Alive_Counter of 0x6F0 frames
python3 dk1_can_test.py tx can0           # Transmit 0x6F1 frames for Blue Pill verification
python3 dk1_can_test.py tx can0 --corrupt 20   # Inject CRC fault every 20 frames
```
Transfer `tools/dk1_can_test.py` to DK1 via `scp`.

## C. Verification Sequence (Step-by-Step Gate)
| Step | Action | Pass Criteria |
|---|---|---|
| 1 | Blue Pill standalone, preset `debug-loopback` | UART log: `self ok` increments ~10/s; `TEC=0 REC=0`; no `BUS-OFF` |
| 2 | Blue Pill + transceiver module, preset `debug`, DK1 disconnected | `TX fail` may increment (no ACK present), confirms transceiver TX path without software freeze |
| 3 | Connect DK1, run `rx` on DK1 | `ok` counter increments ~10/s, `crc_err=0`, `alive_err=0` |
| 4 | Run `tx` on DK1 | Blue Pill UART log: `DK1 ok` increments ~10/s, `crc=0 alive=0` |
| 5 | Run `tx --corrupt 20` | Blue Pill `DK1 crc` increments by ≈ 5% of frames; corrupted frames are discarded and not marked valid |
| 6 | 10-minute continuous run | `ip -s link show can0`: errors = 0; `TEC=0 REC=0` |
| 7 | Logic analyzer probe on CANH/D | 1-bit nominal width = 2 µs (500 kbps, 83.3% sample point) |

## D. DK1 SocketCAN Troubleshooting
If `can0` is not listed:
```bash
dmesg | grep -i -E "can|m_can|fdcan"
find /proc/device-tree -name 'can@*'
cat /proc/device-tree/soc/*/can@*/status 2>/dev/null     # "disabled" -> must enable in device tree
```
Common error `"no clock found"`: FDCAN clock node missing in device tree.

## E. Rapid Troubleshooting Guide
| Symptom | Probable Root Cause |
|---|---|
| `BUS-OFF` or TEC continuously increasing | Missing common GND, missing/extra 120 Ω termination, incorrect bitrate/sample point, inverted D/R, or reversed CANH/CANL |
| `crc_err` constantly increments | Incorrect Data ID (0xA0 for 0x6F0, 0xB1 for 0x6F1) or misaligned payload format |
| `alive_err` increments during `crc_err` | Normal behavior: frames with corrupted CRC are discarded, causing counter jumps |
| No UART log output | Inverted TX/RX lines, incorrect baud rate (must be 115200 8N1), or missing GND |
| OpenOCD `UNEXPECTED idcode` | Blue Pill clone (CKS32/GD32): append `-c "set CPUTAPID 0x2ba01477"` |
