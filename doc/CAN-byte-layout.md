# CAN Matrix & Byte Layout Specification (v0.1)

Accompanying file: `can_matrix.dbc` (v0.5, validated using `cantools`, encode/decode tested successfully). The tables below are derived directly from the DBC definition to maintain 100% synchronization. **If modifying the network layout, always update the DBC file first and regenerate the documentation rather than editing values manually.**  
- **v0.2:** Added End-to-End (E2E) protection (`Alive_Counter` + `CRC8`) across all frames (see Section 4).  
- **v0.3:** Finalized single-door BCM design (`Door_Open` replacing separate left/right door signals).  
- **v0.4:** Updated topology: Master (STM32MP157) issues commands, Jetson functions as DMS ECU (added `DMS_STATUS`, streamlined `DMS_ALERT`, removed raw EAR from `DMS_ALERT`).  
- **v0.5:** Added signals for distributed logic and fault management: `Decel_Exceeded`, `Alarm_State`, `Fog_Risk`, `Fan_Override`, `Child_Lock`, `Pinch_Detected`, `Child_Lock_State`, `Perclos_Pct`, and added subscription mappings (BCM receives `WHEEL_SPEED`, Window receives `BCM_STATUS`, Climate receives `DMS_ALERT`).

---

## 1. General Conventions

| Parameter | Specification |
| :-- | :-- |
| Bus Protocol & Bitrate | CAN 2.0A, 11-bit standard identifier, 500 kbps |
| Byte Order | **Intel (little-endian)**, designated `@1` in DBC. Start bit represents the least significant bit (LSB) of the signal. |
| Physical Scaling | `physical = raw × factor + offset` |
| Unused Bits | Transmitted as 0; ignored by receivers. |
| Data Length Code (DLC) | **Fixed** per message ID. Mismatched DLC is treated as a transmission fault (`WRONG_LENGTH`). |
| `Alive_Counter` | 4-bit sequence counter, increments by 1 per transmission for each ID, rolling over 15 → 0. Detects dropped frames, duplicate frames, and replay attacks. |
| `CRC8` | **Always located in the final byte of every frame (`byte[DLC - 1]`)**. Uses CRC-8/SAE-J1850 to detect bit corruption (see Section 4). |
| Signal Loss Timeout | Missing a **valid** frame (correct CRC and sequence counter) for **3× nominal cycle time** marks the signal as `INVALID`; the Master displays "N/A" and logs an event. Applies strictly to periodic frames. |
| BCM Safety Timeout | Missing a valid `DMS_ALERT` frame for 500 ms automatically silences the DMS-triggered alert buzzer. |
| Bus Arbitration Priority | Lower numerical CAN ID wins bus arbitration. Safety-critical messages (`WSW_WARNING` 0x100) are assigned the lowest ID / highest priority. |

---

## 2. Frame Summary Matrix

| ID | Message Name | Transmitter | Receivers | DLC | Cycle Time / Transmission Trigger |
| :-- | :-- | :-- | :-- | :-- | :-- |
| 0x100 | WSW_WARNING | WSW | MASTER | 4 | Event (100 ms repetition while active) |
| 0x101 | WHEEL_SPEED | WSW | BCM, MASTER | 7 | 20 ms periodic |
| 0x102 | CLIMATE_STATUS | CLIMATE | MASTER | 8 | 100 ms periodic |
| 0x103 | BCM_STATUS | BCM | MASTER, WINDOW | 4 | 100 ms periodic |
| 0x104 | WINDOW_CMD | MASTER | WINDOW | 3 | Event-triggered |
| 0x105 | CLIMATE_CMD | MASTER | CLIMATE | 5 | Event-triggered |
| 0x106 | BCM_CMD | MASTER | BCM | 3 | Event-triggered |
| 0x110 | DMS_ALERT | DMS | BCM, CLIMATE, MASTER | 3 | Event (100 ms repetition during active alert) |
| 0x111 | DMS_STATUS | DMS | MASTER | 8 | 100 ms periodic |
| 0x204 | WINDOW_STATUS | WINDOW | MASTER | 4 | 100 ms periodic |

> [!NOTE]
> UDS diagnostic messages (0x7E0–0x7E3 / 0x7E8–0x7EB) are excluded from the DBC matrix because they use the ISO-TP multi-frame transport protocol (see Section 7).

---

## 3. Detailed Frame Specifications

### 0x100 — WSW_WARNING

Wheel slip warning. Transmitted upon state change, repeated every 100 ms while `Slip_Warning_Active = 1`. Lowest numerical ID provides highest bus arbitration priority.

- **Transmitter:** WSW  
- **Receiver:** MASTER  
- **DLC:** 4  
- **Cycle Time:** Event-driven (repeated every 100 ms while active)

**Payload Byte Map** (bit 7 down to bit 0; `[a:b]` denotes signal bit range):

| Byte | Content |
| :-- | :-- |
| 0 | 7-4: Reserved · 3: Decel_Exceeded · 2: Warn_Lamp · 1: Brake_Pressed · 0: Slip_Warning_Active |
| 1 | 7-0: Slip_Percent[7:0] |
| 2 | 7-4: Reserved · 3-0: Alive_Counter[3:0] |
| 3 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Slip_Warning_Active | 0 | 1 | 1 | 0 | 0–1 | — | 0=Inactive; 1=Active |
| Brake_Pressed | 1 | 1 | 1 | 0 | 0–1 | — | 0=Released; 1=Pressed |
| Warn_Lamp | 2 | 1 | 1 | 0 | 0–1 | — | 0=Off; 1=On |
| Decel_Exceeded | 3 | 1 | 1 | 0 | 0–1 | — | 1=Deceleration exceeds slip threshold |
| Slip_Percent | 8 | 8 | 1 | 0 | 0–100 | % | Slip = (Ref - Speed) / Ref × 100 (clipped to 0..100) |
| Alive_Counter | 16 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 24 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x101 — WHEEL_SPEED

Wheel speed and moving-average reference speed, transmitted periodically every 20 ms. Speed is computed from encoder pulse period; latest valid value is repeated between pulses.

- **Transmitter:** WSW  
- **Receivers:** BCM, MASTER  
- **DLC:** 7  
- **Cycle Time:** 20 ms periodic

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-0: Wheel_Speed[7:0] |
| 1 | 7-0: Wheel_Speed[15:8] |
| 2 | 7-0: Ref_Speed[7:0] |
| 3 | 7-0: Ref_Speed[15:8] |
| 4 | 7-1: Reserved · 0: Brake_Pressed |
| 5 | 7-4: Reserved · 3-0: Alive_Counter[3:0] |
| 6 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Wheel_Speed | 0 | 16 | 0.1 | 0 | 0–6553.5 | rpm | Measured wheel rotational speed |
| Ref_Speed | 16 | 16 | 0.1 | 0 | 0–6553.5 | rpm | Moving average of Wheel_Speed over N samples |
| Brake_Pressed | 32 | 1 | 1 | 0 | 0–1 | — | 0=Released; 1=Pressed |
| Alive_Counter | 40 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 48 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x102 — CLIMATE_STATUS

Climate ECU status telemetry, transmitted periodically every 100 ms. DHT sensor is sampled every 2 s, with the latest valid sample repeated. Signals are bit-packed to accommodate E2E protection fields within 8 bytes.

- **Transmitter:** CLIMATE  
- **Receiver:** MASTER  
- **DLC:** 8  
- **Cycle Time:** 100 ms periodic

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-0: Cabin_Temp[7:0] |
| 1 | 7-4: Cabin_Humidity[3:0] · 3-0: Cabin_Temp[11:8] |
| 2 | 7-6: Sunlight_Level[1:0] · 5-0: Cabin_Humidity[9:4] |
| 3 | 7-6: Fan_Speed[1:0] · 5-0: Sunlight_Level[7:2] |
| 4 | 7-5: Flap_Angle[2:0] · 4-0: Fan_Speed[6:2] |
| 5 | 7: Fog_Risk · 6: DHT_Fault · 5: Climate_Mode · 4-0: Flap_Angle[7:3] |
| 6 | 7-5: Reserved · 4: Fan_Override · 3-0: Alive_Counter[3:0] |
| 7 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Cabin_Temp (signed) | 0 | 12 | 0.1 | 0 | -40–125 | °C | Ambient interior cabin temperature |
| Cabin_Humidity | 12 | 10 | 0.1 | 0 | 0–100 | % | Relative humidity |
| Sunlight_Level | 22 | 8 | 1 | 0 | 0–255 | — | Solar radiation sensor (scaled LDR value) |
| Fan_Speed | 30 | 7 | 1 | 0 | 0–100 | % | Blower fan duty cycle |
| Flap_Angle | 37 | 8 | 1 | 0 | 0–180 | deg | Air distribution vane servo angle |
| Climate_Mode | 45 | 1 | 1 | 0 | 0–1 | — | 0=Manual; 1=Auto |
| DHT_Fault | 46 | 1 | 1 | 0 | 0–1 | — | 1=Last DHT22 sensor read failed |
| Fog_Risk | 47 | 1 | 1 | 0 | 0–1 | — | 1=Cabin temp near dew point (Magnus formula) |
| Alive_Counter | 48 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| Fan_Override | 52 | 1 | 1 | 0 | 0–1 | — | 1=Fan forced to max via active DMS alert |
| CRC8 | 56 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x103 — BCM_STATUS

Body Control Module status, transmitted periodically every 100 ms. Configured for a single-door vehicle model (one reed switch).

- **Transmitter:** BCM  
- **Receivers:** MASTER, WINDOW  
- **DLC:** 4  
- **Cycle Time:** 100 ms periodic

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-6: Alarm_State[1:0] · 5: PIR_Motion · 4: Buzzer_On · 3: CabinLight_On · 2: Headlight_On · 1: Lock_State · 0: Door_Open |
| 1 | 7-0: Ambient_Light[7:0] |
| 2 | 7-4: Reserved · 3-0: Alive_Counter[3:0] |
| 3 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Door_Open | 0 | 1 | 1 | 0 | 0–1 | — | 0=Closed; 1=Open |
| Lock_State | 1 | 1 | 1 | 0 | 0–1 | — | 0=Unlocked; 1=Locked |
| Headlight_On | 2 | 1 | 1 | 0 | 0–1 | — | 0=Off; 1=On |
| CabinLight_On | 3 | 1 | 1 | 0 | 0–1 | — | 0=Off; 1=On |
| Buzzer_On | 4 | 1 | 1 | 0 | 0–1 | — | 0=Off; 1=On |
| PIR_Motion | 5 | 1 | 1 | 0 | 0–1 | — | 0=No motion; 1=Motion detected |
| Alarm_State | 6 | 2 | 1 | 0 | 0–2 | — | 0=Disarmed; 1=Armed; 2=Alarm |
| Ambient_Light | 8 | 8 | 1 | 0 | 0–255 | — | Ambient light reading (0=dark, 255=bright) |
| Alive_Counter | 16 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 24 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x104 — WINDOW_CMD

Window target position command from Master. The Window ECU implements a virtual motor model driven by a position PID controller. Commanded angles > 180° are rejected and logged as DTC `0xB30001`.

- **Transmitter:** MASTER  
- **Receiver:** WINDOW  
- **DLC:** 3  
- **Cycle Time:** Event-triggered

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-0: Target_Angle[7:0] |
| 1 | 7-5: Reserved · 4: Child_Lock · 3-0: Alive_Counter[3:0] |
| 2 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Target_Angle | 0 | 8 | 1 | 0 | 0–180 | deg | Desired window opening angle (0=Closed, 180=Full Open) |
| Alive_Counter | 8 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| Child_Lock | 12 | 1 | 1 | 0 | 0–1 | — | 1=Child lock active (disables opening commands) |
| CRC8 | 16 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x105 — CLIMATE_CMD

Climate system control command. In manual mode, `Fan_Speed_Cmd` and `Flap_Angle_Cmd` are applied directly. In auto mode, the ECU controls fan and vane based on `Temp_Setpoint`.

- **Transmitter:** MASTER  
- **Receiver:** CLIMATE  
- **DLC:** 5  
- **Cycle Time:** Event-triggered

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-4: Alive_Counter[3:0] · 3-1: Reserved · 0: Climate_Mode_Cmd |
| 1 | 7-0: Fan_Speed_Cmd[7:0] |
| 2 | 7-0: Flap_Angle_Cmd[7:0] |
| 3 | 7-0: Temp_Setpoint[7:0] |
| 4 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Climate_Mode_Cmd | 0 | 1 | 1 | 0 | 0–1 | — | 0=Manual; 1=Auto |
| Alive_Counter | 4 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| Fan_Speed_Cmd | 8 | 8 | 1 | 0 | 0–100 | % | Commanded blower fan duty cycle |
| Flap_Angle_Cmd | 16 | 8 | 1 | 0 | 0–180 | deg | Commanded air vane angle |
| Temp_Setpoint | 24 | 8 | 0.5 | 0 | 0–50 | °C | Target temperature setpoint for auto mode |
| CRC8 | 32 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x106 — BCM_CMD

Master control commands for body functions. Setting value to 0 denotes no action / automatic control.

- **Transmitter:** MASTER  
- **Receiver:** BCM  
- **DLC:** 3  
- **Cycle Time:** Event-triggered

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7: Reserved · 6: Buzzer_Test · 5-4: CabinLight_Cmd[1:0] · 3-2: Headlight_Cmd[1:0] · 1-0: Lock_Cmd[1:0] |
| 1 | 7-4: Reserved · 3-0: Alive_Counter[3:0] |
| 2 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Lock_Cmd | 0 | 2 | 1 | 0 | 0–2 | — | 0=No_Action; 1=Lock; 2=Unlock |
| Headlight_Cmd | 2 | 2 | 1 | 0 | 0–2 | — | 0=Auto; 1=Force_Off; 2=Force_On |
| CabinLight_Cmd | 4 | 2 | 1 | 0 | 0–2 | — | 0=Auto; 1=Force_Off; 2=Force_On |
| Buzzer_Test | 6 | 1 | 1 | 0 | 0–1 | — | 1=Trigger 200 ms audible test chirp |
| Alive_Counter | 8 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 16 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x110 — DMS_ALERT

Driver Monitoring alert emitted by the DMS ECU (Jetson). Repeated every 100 ms while `Alert_Type != 0`. When the condition resolves, a single frame with `Alert_Type = 0` is transmitted to clear alerts. BCM automatically silences the buzzer if no valid frame arrives within 500 ms.

- **Transmitter:** DMS  
- **Receivers:** BCM, CLIMATE, MASTER  
- **DLC:** 3  
- **Cycle Time:** Event-driven (repeated every 100 ms while active)

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-5: Reserved · 4-3: Severity[1:0] · 2-0: Alert_Type[2:0] |
| 1 | 7-4: Reserved · 3-0: Alive_Counter[3:0] |
| 2 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Alert_Type | 0 | 3 | 1 | 0 | 0–3 | — | 0=None; 1=Drowsy; 2=Distracted; 3=No_Face |
| Severity | 3 | 2 | 1 | 0 | 0–3 | — | 0=None; 1=Low; 2=High; 3=Critical |
| Alive_Counter | 8 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 16 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x111 — DMS_STATUS

DMS ECU heartbeat and telemetry frame, transmitted periodically every 100 ms. The Master marks DMS offline if valid frames cease for 3× cycle time (300 ms).

- **Transmitter:** DMS  
- **Receiver:** MASTER  
- **DLC:** 8  
- **Cycle Time:** 100 ms periodic

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-2: Reserved · 1: Camera_OK · 0: Face_Detected |
| 1 | 7-0: EAR_Value[7:0] |
| 2 | 7-0: Head_Yaw[7:0] |
| 3 | 7-0: Head_Pitch[7:0] |
| 4 | 7-0: Dms_FPS[7:0] |
| 5 | 7-0: Perclos_Pct[7:0] |
| 6 | 7-4: Reserved · 3-0: Alive_Counter[3:0] |
| 7 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Face_Detected | 0 | 1 | 1 | 0 | 0–1 | — | 0=No face tracked; 1=Face acquired |
| Camera_OK | 1 | 1 | 1 | 0 | 0–1 | — | 0=Camera offline/error; 1=Sensor streaming |
| EAR_Value | 8 | 8 | 0.01 | 0 | 0–2.55 | — | Eye Aspect Ratio × 100 (telemetry only) |
| Head_Yaw (signed) | 16 | 8 | 1 | 0 | -90–90 | deg | Head yaw angle (-90 left, +90 right) |
| Head_Pitch (signed) | 24 | 8 | 1 | 0 | -90–90 | deg | Head pitch angle (-90 down, +90 up) |
| Dms_FPS | 32 | 8 | 1 | 0 | 0–255 | fps | Real-time computer vision frame rate |
| Perclos_Pct | 40 | 8 | 1 | 0 | 0–100 | % | Percentage of eye closure over sliding 60 s window |
| Alive_Counter | 48 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 56 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

### 0x204 — WINDOW_STATUS

Power Window position and motor state telemetry, transmitted periodically every 100 ms.

- **Transmitter:** WINDOW  
- **Receiver:** MASTER  
- **DLC:** 4  
- **Cycle Time:** 100 ms periodic

**Payload Byte Map:**

| Byte | Content |
| :-- | :-- |
| 0 | 7-0: Current_Angle[7:0] |
| 1 | 7-0: Target_Angle_Echo[7:0] |
| 2 | 7-4: Alive_Counter[3:0] · 3: Reserved · 2: Child_Lock_State · 1: Pinch_Detected · 0: Moving |
| 3 | 7-0: CRC8[7:0] |

**Signal Definitions:**

| Signal | Start | Len | Factor | Offset | Min–Max | Unit | Values / Description |
| :-- | :-- | :-- | :-- | :-- | :-- | :-- | :-- |
| Current_Angle | 0 | 8 | 1 | 0 | 0–180 | deg | Estimated current window position |
| Target_Angle_Echo | 8 | 8 | 1 | 0 | 0–180 | deg | Target angle currently tracked by PID |
| Moving | 16 | 1 | 1 | 0 | 0–1 | — | 0=Stationary; 1=Motor moving |
| Pinch_Detected | 17 | 1 | 1 | 0 | 0–1 | — | 1=Anti-pinch triggered (virtual motor current limit) |
| Child_Lock_State | 18 | 1 | 1 | 0 | 0–1 | — | 0=Disabled; 1=Active |
| Alive_Counter | 20 | 4 | 1 | 0 | 0–15 | — | Increments per transmission, wraps 15 → 0 |
| CRC8 | 24 | 8 | 1 | 0 | 0–255 | — | CRC-8/SAE-J1850 over [DataID, payload[0:DLC-2]] |

---

## 4. End-to-End (E2E) Protection (Alive_Counter + CRC-8)

All ECU firmware implementations follow the **AUTOSAR Classic layered architecture** utilizing a **Spec-compliant Custom BSW**: the `common/e2e_crc8` module adheres to the **AUTOSAR E2E Profile 1** technical specification (4-bit Alive Counter, DataID, CRC-8/SAE-J1850 polynomial 0x1D, 3-strike debounce fault/recovery filter per ISO 26262 ASIL B/D) integrated seamlessly with COM/PduR and UDS DCM/DEM without requiring expensive commercial full-stack licenses. Every CAN frame carries these two integrity fields, and the receiver verifies them prior to consuming data (for architectural details, refer to [Kien-truc-Firmware-AUTOSAR.md](Kien-truc-Firmware-AUTOSAR.md)).

**CRC Calculation Specifications:**

| Parameter | Specification |
| :-- | :-- |
| Algorithm | CRC-8/SAE-J1850: polynomial 0x1D, init 0xFF, no input/output bit reflection, xorout 0xFF |
| Input Array | `[DataID]` prepended to payload bytes **excluding the final CRC byte** (`payload[0 : DLC-2]`) |
| Location | Final byte of each frame (`byte[DLC - 1]`) |
| Standard Test Vector | `crc8("123456789") = 0x4B` (SAE J1850 check value) |

`DataID` is a unique 1-byte constant assigned to each message ID, stored as the DBC attribute `E2E_DataID`. This mechanism guarantees that a valid frame from one ID injected into another ID will fail CRC validation (masquerading protection).

| Message ID | Message Name | E2E DataID |
| :-- | :-- | :-- |
| 0x100 | WSW_WARNING | 0x3C |
| 0x101 | WHEEL_SPEED | 0x5A |
| 0x102 | CLIMATE_STATUS | 0x96 |
| 0x103 | BCM_STATUS | 0xC3 |
| 0x104 | WINDOW_CMD | 0x69 |
| 0x105 | CLIMATE_CMD | 0xA5 |
| 0x106 | BCM_CMD | 0x0F |
| 0x110 | DMS_ALERT | 0xF0 |
| 0x111 | DMS_STATUS | 0x99 |
| 0x204 | WINDOW_STATUS | 0x33 |

**Receiver Verification State Machine** (`delta = (counter_new − counter_prev) mod 16`):

| Evaluation Result | Entry Condition | Data Consumable by SWC? |
| :-- | :-- | :-- |
| `INITIAL` | First valid frame received for this message ID | Yes |
| `OK` | Valid CRC, sequence counter delta = 1 | Yes |
| `OK_SOME_LOST` | Valid CRC, delta = 2 or 3 (1–2 dropped frames) | Yes (increment lost frame telemetry) |
| `REPEATED` | Valid CRC, delta = 0 (repeated frame or replay attack) | **No** |
| `WRONG_SEQUENCE` | Valid CRC, delta ≥ 4 | **No** |
| `WRONG_CRC` | CRC checksum mismatch | **No** |
| `WRONG_LENGTH` | Received DLC does not match DBC specification | **No** |

**Fault Debouncing & Failsafe Responses:**

- **Fault Trigger (3-Strike Rule):** 3 consecutive errors (`REPEATED`, `WRONG_SEQUENCE`, `WRONG_CRC`) or missing frames exceeding 3× nominal cycle time marks the signal as **`INVALID`**, logs a diagnostic trouble code (DTC), and forces the ECU into its safe state (see table below).
- **Self-Recovery:** Returning to normal requires receiving **3 consecutive valid frames**. The DTC state transitions to "previously failed" (clearing active warning), remaining readable via UDS Service 0x19.
- **Aperiodic Messages:** Non-periodic frames (commands, `DMS_ALERT` when inactive) have no timeout monitoring, but sequence counter and CRC validation remain strictly enforced on every arrival.

**Receiver Failsafe Behavior Matrix:**

| Receiving Node | Supervised Frame | Failsafe Action on Invalid / Timeout | Associated DTC |
| :-- | :-- | :-- | :-- |
| BCM | DMS_ALERT | Silence DMS buzzer; preserve door, lock, and lighting states | 0xD00002 / 0xD00003 |
| BCM | BCM_CMD | Reject command; retain current actuator states | 0xD00002 |
| Climate | CLIMATE_CMD | Reject command; maintain current climate mode and blower speed | 0xD00002 |
| Window | WINDOW_CMD | Reject command; freeze window at current position | 0xD00002 |
| Master (MP157) | All Status Frames & `DMS_STATUS` | Dashboard displays "N/A" with warning icon; logs fault event | (System Log) |
| Master (MP157) | `DMS_STATUS` Timeout (3× 100 ms) | HMI displays "DMS Offline", logs warning; BCM auto-silences buzzer after 500 ms without valid `DMS_ALERT` | (System Log) |

**Reference C Implementation on ECUs** (matching `e2e_reference.py`):

```c
static uint8_t crc8_byte(uint8_t crc, uint8_t b)
{
    crc ^= b;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x1D) : (uint8_t)(crc << 1);
    }
    return crc;
}

/* len = payload length EXCLUDING the final CRC byte (DLC - 1) */
uint8_t e2e_crc(uint8_t data_id, const uint8_t *payload, size_t len)
{
    uint8_t crc = crc8_byte(0xFF, data_id);
    for (size_t i = 0; i < len; i++) {
        crc = crc8_byte(crc, payload[i]);
    }
    return crc ^ 0xFF;
}
```

The accompanying `e2e_reference.py` script provides the reference Python implementation, complete with the receiver state machine and self-test suite. It serves as the test vector generator for unit tests (Unity) running on host or target.

---

## 5. Test Frame Examples for `cansend` / `candump`

The following test frames have valid CRC values precalculated via `e2e_reference.py`. Due to sequence counter checking, **transmitting an identical frame multiple times consecutively will result in `REPEATED` on the receiver**, demonstrating active replay protection.

| Test Objective | Shell Command | Interpretation / Signal Values |
| :-- | :-- | :-- |
| Window 90° Position Command | `cansend can0 104#5A00BE` | Target_Angle = 90°, Alive_Counter = 0 |
| Door Lock + Headlights On | `cansend can0 106#090061` | Lock_Cmd = 1, Headlight_Cmd = 2, Alive_Counter = 0 |
| High-Severity Drowsiness Alert | `cansend can0 110#110081` | Alert_Type = 1 (Drowsy), Severity = 2 (High), Alive_Counter = 0 |
| DMS ECU Telemetry Sample | `cansend can0 111#031CF40519080099` | Face detected, Camera OK, EAR 0.28, Yaw -12°, Pitch 5°, 25 FPS, Alive_Counter = 0 |
| Wheel Speed Telemetry Sample | `cansend can0 101#D2041405010351` | Wheel 123.4 rpm, Ref 130.0 rpm, Brake = 1, Alive_Counter = 3 |
| Climate Telemetry Sample | `cansend can0 102#CBFF28B24C2B0759` | -5.3 °C, 65.5% RH, Solar 200, Fan 50%, Vane 90°, Auto mode, Alive_Counter = 7 |
| **Invalid CRC Injection Test** | `cansend can0 104#5A0041` | Window 90° with corrupted CRC; receiver flags `WRONG_CRC` and rejects command |
| **Replay Attack Injection Test** | Send `104#5A00BE` twice consecutively | Second frame is flagged as `REPEATED` and dropped |
| **Dropped Frame Test** | Send `104#5A00BE`, followed by `104#5A0399` | Sequence counter jumps 0 → 3; receiver flags `OK_SOME_LOST` and accepts data |

**Python Verification Script Example:**

```python
import cantools
import e2e_reference as e2e

db = cantools.database.load_file("can_matrix.dbc")
chk = e2e.E2EChecker(db)
data = bytes.fromhex("D2041405010351")
print(chk.check(0x101, data))          # Expected: INITIAL
print(db.decode_message(0x101, data))
```

---

## 6. Estimated Bus Utilization

Calculated using worst-case bit stuffing formula:  
$$\text{Frame Bits} = 47 + 8 \times \text{DLC} + \left\lfloor \frac{34 + 8 \times \text{DLC} - 1}{4} \right\rfloor$$

| Operational Scenario | Bus Load @ 500 kbps |
| :-- | :-- |
| Nominal load (4 ECUs + DMS periodic streaming, no active alerts) | ≈ **2.2%** |
| Peak worst-case load (WSW slip active + DMS alert streaming + 5 commands/sec) | ≈ **2.8%** |

The minimal bus load leaves abundant headroom for network expansion. Adding CRC8 and counter fields across all frames contributes negligible bandwidth overhead. This calculation provides the primary benchmark KPI to compare against physical oscilloscope / bus analyzer captures.

---

## 7. Streamlined UDS Diagnostics (ISO 14229)

**Addressing Architecture (Physical Addressing, ISO-TP):**

| Node | Request ID (Master → ECU) | Response ID (ECU → Master) |
| :-- | :-- | :-- |
| BCM | 0x7E0 | 0x7E8 |
| Window | 0x7E1 | 0x7E9 |
| Climate | 0x7E2 | 0x7EA |
| WSW | 0x7E3 | 0x7EB |
| DMS (Jetson) | 0x7E4 | 0x7EC |

**Supported UDS Services:**

| Service ID (SID) | Service Name | Supported Scope & Notes |
| :-- | :-- | :-- |
| `0x22` | ReadDataByIdentifier | Query vendor-defined DIDs (see table below) |
| `0x19` | ReadDTCInformation | Sub-function `0x02` (`reportDTCByStatusMask`) |
| `0x7F` | Negative Response | NRC `0x11` (serviceNotSupported), `0x12` (subFunctionNotSupported), `0x31` (requestOutOfRange) |

**Vendor-Defined Data Identifiers (DIDs):**

| DID | Description | Length | Format / Implementation Notes |
| :-- | :-- | :-- | :-- |
| `0x0100` | Firmware Version | 2 bytes | Major, Minor (uint8 each) |
| `0x0101` | System Uptime | 4 bytes | Operating seconds, big-endian uint32 |
| `0x0102` | Transmitted CAN Frame Count | 4 bytes | Frame counter, big-endian uint32 |
| `0x0103` | CAN Error Counters | 2 bytes | Byte 0 = Transmit Error Counter (TEC), Byte 1 = Receive Error Counter (REC) |
| `0x0120` | Wheel Slip Freeze Frame (WSW only) | 4 bytes | Raw Wheel_Speed (2 bytes, 0.1 rpm), Slip % (1 byte), Deceleration delta over 100 ms (1 byte); stored in static RAM |
| `0x0110` | Primary Node Telemetry | 2 bytes | BCM: Ambient_Light; Climate: Cabin_Temp raw; WSW: Wheel_Speed raw; Window: Current_Angle; DMS: Dms_FPS |

**Single-Frame Diagnostic Examples:**

| Action | Transmitted CAN Frame |
| :-- | :-- |
| Query BCM Firmware Version | `7E0#03220100` |
| Positive Response (v1.2) | `7E8#056201000102` |
| Read Active DTCs (Mask 0xFF) | `7E0#031902FF` |
| Positive Response (DTC 0xB10001, Status 0x01) | `7E8#075902FFB1000101` |
| Negative Response (Unsupported DID) | `7E8#037F2231` |

> [!TIP]
> Multi-DTC responses exceeding 8 bytes require **ISO-TP multi-frame handling** (First Frame, Flow Control, Consecutive Frames). On Linux Master, use kernel `can-isotp` (`CONFIG_CAN_ISOTP`) or Python `can-isotp`. On Jetson, use Python `can-isotp`. On STM32 ECUs, either integrate a compact C ISO-TP library (e.g., `isotp-c`) or cap diagnostic responses to single-frame size (SF <= 7 payload bytes).

**Diagnostic Trouble Code (DTC) Definitions:**

| DTC | ECU Node | Fault Description | Trigger Condition |
| :-- | :-- | :-- | :-- |
| `0xB10001` | BCM | LDR Ambient Light Sensor Out of Range | ADC reading stuck at 0 or 4095 for > 5 seconds |
| `0xB20001` | Climate | DHT22 Sensor Read Failure | Corresponds to `DHT_Fault = 1` |
| `0xB20002` | Climate | Solar Radiation LDR Out of Range | ADC reading stuck at 0 or 4095 for > 5 seconds |
| `0xB30001` | Window | Target Angle Command Out of Range | Commanded `Target_Angle > 180°` |
| `0xC10001` | WSW | Optical Encoder Loss of Signal | Motor energized but zero capture pulses detected |
| `0xC10002` | WSW | Wheel Slip Warning Event Triggered | Latches active slip event; cleared on reset |
| `0xD00001` | All | CAN Controller Bus-Off Recovered | Set upon auto-recovery from CAN bus-off state |
| `0xD00002` | All | E2E Data Integrity Check Failure | 3 consecutive errors (CRC, REPEATED, or WRONG_SEQUENCE) |
| `0xD00003` | All | Periodic Frame Reception Timeout | Missing valid periodic frame for > 3× cycle time |
| `0xE00001` | DMS (Jetson) | Driver Monitoring Camera Unavailable | `Camera_OK = 0` |
| `0xE00002` | DMS (Jetson) | Sustained Low Vision Pipeline Framerate | `Dms_FPS < 10` for > 5 consecutive seconds |

Simplified DTC Status Byte: `0x01` = `testFailed` (fault actively present); `0x00` = `previouslyFailed` (fault cleared/healed).

---

## 8. Utilizing the DBC Matrix Across the Project

| Target Environment | Implementation Methodology |
| :-- | :-- |
| FreeRTOS Firmware (4 ECUs) | Execute `cantools generate_c_source can_matrix.dbc` to auto-generate C pack/unpack functions, preventing manual bit-shifting bugs. Note: generated `pack` functions do not compute CRC; firmware must invoke `e2e_crc()` after packing. |
| Master (MP157) & DMS (Jetson) | Leverage Python `cantools` + `python-can` for frame decoding; C++/Qt services utilize DBC-generated unpack routines; `e2e_reference.py` handles CRC and counter verification. |
| Qt Dashboard HMI | Access signals via Python bindings or a dedicated C++ decoding adapter matching Section 3. |
| Verification & Benchmarking | Nominal cycle times (`GenMsgCycleTime` attribute) serve as timing reference for jitter analysis and CAN bus response-time verification. |
| Diagnostic Bus Analyzers | Load `can_matrix.dbc` directly into PCAN-View, SavvyCAN, or CANalyzer for real-time physical signal graphing. |

---

## 9. Engineering Notes and Alignment Points

1. **Resolution & Data Types:** Confirm whether signal resolution (e.g., wheel speed at 0.1 rpm, temperature at 0.1 °C) provides sufficient precision for all experimental evaluations.
2. **Door Sensor Configuration:** Confirmed single-door model (`Door_Open` signal); no further architectural changes required.
3. **Firmware Tuning Parameters:** Internal algorithmic constants (moving-average filter depth $N$, slip threshold %, ambient light switching hysteresis) are intentionally excluded from CAN frames and maintained in local firmware configuration headers (`config.h`).
4. **Diagnostic Scope:** The Section 7 DID and DTC definitions constitute the baseline specification; additional DIDs may be registered if expanded live telemetry is needed during thesis defense demonstrations.
5. **Bit-Packed Climate Frame:** Note that `CLIMATE_STATUS` utilizes non-byte-aligned bit fields (12-bit temp, 10-bit humidity, 7-bit fan) to preserve bandwidth for E2E protection. Always rely on auto-generated DBC unpack functions rather than manual bit masking.
