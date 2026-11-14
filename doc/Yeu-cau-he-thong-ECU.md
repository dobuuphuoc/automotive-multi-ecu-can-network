# Design and Evaluation of a Miniature Automotive E/E Platform over CAN with End-to-End Protection and Fault Management

# System Requirements & Architecture Specification

*(Version v0.8: CAN IDS removed, Automotive Quality Layer added; Master STM32MP157D-DK1 + Jetson Orin Nano as DMS ECU; 4 ECUs running FreeRTOS)*

> **Working Draft:** Subject to peer review and revision. Figures annotated with **(proposed)** represent engineering assumptions, to be updated based on empirical hardware validation and advisor feedback.

---

## 0A. Measurable Project Goals

| ID | Objective | Verification Method |
| :-- | :-- | :-- |
| **G1** | Design and implement a 6-node platform on a 500 kbps CAN bus (MP157 master, Jetson DMS ECU, 4 STM32F103 ECUs running FreeRTOS), with all communication adhering to a unified DBC matrix. | Compare DBC definitions against bus logs; measure empirical bus load. |
| **G2** | End-to-End (E2E) protection and fault management mechanisms detect all 8 fault categories in the fault injection matrix, with a quantitative comparison **between E2E enabled and disabled**. | Experiments E1, E3 (Section 5B). |
| **G3** | Characterize and validate real-time timing behavior: cyclic jitter within ±10%, empirical response times do not exceed theoretical worst-case predictions **across multiple bus load levels**. | Experiments E2, E5. |
| **G4** | Distributed functions (drowsiness warning, speed-dependent auto-locking, auto-window rollup, wheel slip warning) achieve predetermined response deadlines and exhibit fail-safe graceful degradation upon signal loss. | Experiments E4, E6, E7, E8. |

**Architectural Positioning & Known Limitations (to be documented in thesis):**
- **Software Architecture:** The firmware across all 4 microcontrollers adheres to an **AUTOSAR-inspired Layered Architecture**: strictly decoupling the *Application Layer (SWCs)*, *Runtime Environment (RTE)*, *Basic Software (BSW: COM, E2E Transformer, DEM, DCM, CanIf)*, *MCAL*, and *OS (FreeRTOS)*. It conforms to the AUTOSAR E2E Profile 1 specification, UDS diagnostics standard (ISO 14229-1), DTC fault handling, and snapshot freeze frames.
- **Scope Limitations:** The project does not utilize commercial proprietary toolchains and does not claim official ISO 26262 functional safety certification or Full-Stack AUTOSAR compliance due to hardware constraints of the STM32F103 (64 KB Flash, 20 KB RAM); payload cryptographic authentication/encryption is omitted on CAN (no spoofing mitigation); power windows are simulated in software; the DMS utilizes a pretrained MediaPipe model; freeze frames are stored in volatile RAM (lost on power cycle); the system is an experimental tabletop prototype. Detailed specifications can be found in [Kien-truc-Firmware-AUTOSAR.md](Kien-truc-Firmware-AUTOSAR.md).

---

## 0. Finalized Architectural Decisions

| Item | Decision | Architectural Impact |
| :-- | :-- | :-- |
| **BCM Door Count** | **1 door** (1 reed switch) | `BCM_STATUS` utilizes signal `Door_Open`; BCM requires 9 GPIOs; BOM reduced by 1 reed switch module. |
| **Jetson Orin Nano** | **Kit available; serves as DMS ECU** | Dedicated to camera processing, broadcasting `DMS_ALERT` and `DMS_STATUS` via native CAN0 (40-pin header, pins 29 and 31) with an SN65HVD230 transceiver; BOM Jetson quantity set to 0. |
| **Master HMI Display** | **Proposed: 7-inch 1024x600 touchscreen, pending hardware verification** | DK1 only provides HDMI output (no onboard LCD connector); pixel clock is capped at 90 MHz, avoiding native 1080p displays; verify available displays using `modetest -M stm` prior to purchase. |
| **Removal of CAN IDS** | Removed, replaced by Automotive Quality Layer (Section 5A) | Refer to Project Plan v2. |
| **System Architecture** | **Master = STM32MP157D-DK1 (OpenSTLinux, Cortex-A7)** | Qt6 HMI, UDS tester, E2E verification monitor, data logger, command dispatch (`BCM_CMD`/`CLIMATE_CMD`/`WINDOW_CMD`); CAN handled via FDCAN1 (PA11 is RX, PD1 is TX) with an SN65HVD230 transceiver; OpenSTLinux Starter package booted. |
| **RTOS for 4 ECUs** | **FreeRTOS + STM32 HAL** (replacing Zephyr) | Applied to all 4 STM32F103 ECUs; built with CMake + VS Code (FreeRTOS-Kernel and STM32CubeF1 as git submodules); rationale: faster ramp-up within the limited timeline; complement prior experience with Zephyr on Cortex-M4 and Yocto on Cortex-A7. |
| **MP157 Cortex-M4 Core** | **Excluded from primary scope** | Linux on Cortex-A7 retains ownership of FDCAN1; Cortex-M4 remains an optional stretch goal. |
| **Advanced ECU Logic** | **Core:** distributed functions and graceful degradation; Window PID + simulated anti-pinch; WSW deceleration criterion + freeze frames. **Nice-to-have (droppable):** BCM anti-theft state machine and headlight hysteresis; Climate sensor filtering and dew point estimation; DMS PERCLOS. | Negligible hardware footprint; Climate fan PWM remains an optional MOSFET module add-on. |
| **System Evaluation** | Acceptance test plan E1–E9 defined (Section 5B) | Report percentiles, document test conditions, measure common-clock latencies. |
| **CAN Controllers** | bxCAN, FDCAN, mttcan harmonized to Classical CAN 500 kbps, 83.3% sample point | Section 3B. |
| **4th ECU Naming** | Renamed **ABS → WSW (Wheel Slip Warning)** | The ECU monitors wheel speed and flags slip conditions under braking without modulating hydraulic brake calipers. Renamed node, frame `WSW_WARNING` (0x100), signal `Slip_Warning_Active`, prefix `FR-WSW-`. |
| **microSD / JetPack** | **microSD acquired and JetPack fully installed** | Removed from BOM; Week 9 JetPack milestone already fulfilled. |
| **4-ECU PCB** | **Custom carrier board socketing Blue Pill, hand-soldered, 12V → LM2596 5V input** | Avoids soldering bare LQFP48 packages; BOM specifies 6 Blue Pills (+1 spare); refer to Section 3 and `PCB-Requirements-4ECU.md` v0.2. |
| **Non-Functional Thresholds** | **Finalized** | NFR-02 < 100 ms (above 100 rpm), NFR-04 minimum 15 FPS end-to-end (target 25 FPS), NFR-03 maintains 100 ms with event-driven UDS, NFR-01/06/07 intact; slip threshold starting at 20%, E2E timeout 3× period, 3 consecutive errors, MAX_DELTA = 3. |
| **DMS Benchmark (Measured)** | **43.0 FPS on CPU** | `mediapipe==0.10.14`, Jetson Orin Nano, 640x480 video, 60 seconds: 2582 frames, inference latency p50 21.7 ms / p95 24.6 ms / max 126.6 ms, 100% face detection. TFLite XNNPACK delegate on CPU confirmed. Raw inference benchmark only; power profile unrecorded. |

---

## 1. System Overview

| Subsystem | Technology | Role |
| :-- | :-- | :-- |
| **BCM ECU** | STM32F103 + FreeRTOS | Doors, locks, exterior/interior lighting, acoustic buzzer |
| **Window ECU** | STM32F103 + FreeRTOS | Power window (data-only, software simulation, no physical actuator) |
| **Climate ECU** | STM32F103 + FreeRTOS | Cabin temperature, humidity, ventilation fan, air vent flap |
| **WSW ECU** | STM32F103 + FreeRTOS | Wheel speed measurement, wheel slip warning under braking |
| **Master Host** | STM32MP157D-DK1 (OpenSTLinux, A7) | Qt6 HMI, UDS diagnostic tester, E2E monitor, data logger, control command dispatch |
| **DMS ECU** | Jetson Orin Nano + JetPack | Driver camera processing, broadcasting `DMS_ALERT` and `DMS_STATUS` |
| **CAN Bus** | CAN 2.0A at 500 kbps | Master (FDCAN1 + SN65HVD230), DMS ECU (CAN0 + SN65HVD230), and 4 microcontrollers; USB-CAN (candleLight) used exclusively on PC for bus sniffing and fault injection |

**Design Philosophy:** All 4 microcontroller ECUs share identical carrier PCB hardware, differentiated exclusively by their firmware binaries. Each ECU handles two primary responsibilities: **acquire sensor inputs and broadcast CAN frames**, and **consume CAN commands to execute localized actuation** (discrete digital I/O, fixed-duty PWM, threshold evaluation).

---

## 1A. Firmware Software Architecture: AUTOSAR Classic Layered Model

Firmware deployed across all 4 STM32F103 microcontrollers is structured according to the standardized **AUTOSAR Classic Platform Layered Architecture**:

1. **Application Layer (SWCs - Software Components):** Encapsulates pure domain application logic (`SWC_BCM`, `SWC_Climate`, `SWC_Window`, `SWC_WSW`), isolated from hardware dependencies. Periodic (20 ms, 100 ms) and event-driven execution is handled via Runnable Entities.
2. **Runtime Environment (RTE):** Provides Virtual Function Bus abstraction APIs (`Rte_Read_*`, `Rte_Write_*`, `Rte_Call_*`) to decouple SWCs from the underlying Basic Software (BSW) and hardware drivers.
3. **Basic Software (BSW) - Services Layer:**
   - **AUTOSAR E2E Transformer (Profile 1):** Safeguards cyclic communication integrity using a 4-bit Alive Counter, frame-specific DataID, and CRC-8/SAE-J1850 checksums, backed by a 3-count debounce state machine.
   - **COM & PduR (PDU Router):** Performs Signal-to-PDU packing/unpacking matching the CAN DBC communication matrix.
   - **Diagnostic Services (DCM & DEM):** Implements ISO 14229 UDS diagnostics (services 0x10, 0x22, 0x19, 0x14), 3-byte DTC lifecycle management, and volatile RAM snapshot/freeze-frame buffering.
   - **Watchdog Manager (WdgM):** Multi-task Alive Supervision validating task checkpoint heartbeats before servicing the hardware IWDG.
4. **ECU Abstraction & MCAL (Microcontroller Abstraction Layer):**
   - **CanIf & CAN Driver (bxCAN):** Manages the bxCAN peripheral at 500 kbps, 83.3% Sample Point, using interrupt-driven FIFO0 ring buffers.
   - **IoHwAb, Dio, Pwm, Adc, Icu:** Abstracts GPIOs, timer input capture for wheel encoders, PWM generators for lighting/fans/servos, and ADC sampling.
5. **Operating System (OS Layer):** **FreeRTOS** real-time operating system configured with 100% static memory allocation (`configSUPPORT_DYNAMIC_ALLOCATION 0`), SysTick (1 kHz) scheduler timebase, and independent `TIM2` timer as the STM32 HAL timebase.

*Comprehensive specifications, RTE APIs, and data flow diagrams are detailed in:* [Kien-truc-Firmware-AUTOSAR.md](Kien-truc-Firmware-AUTOSAR.md).

---

## 2. Requirements Matrix by ECU

Classification: **Core** requirements are mandatory. **Nice-to-have** requirements have signal slots reserved in the DBC matrix but may be deferred if timeline constraints arise.

### 2.1. BCM ECU

| Type | Peripheral Device | Pin Classification | Pin Count |
| :-- | :-- | :-- | :-- |
| **Input** | Door reed switch (1 door) | Digital In | 1 |
| **Input** | Lock / Unlock push button | Digital In | 1 |
| **Input** | PIR motion sensor HC-SR501 | Digital In | 1 |
| **Input** | Ambient light sensor (LDR module) | ADC | 1 |
| **Output** | Door lock relay + unlock relay | Digital Out | 2 |
| **Output** | Headlights (LED) | PWM | 1 |
| **Output** | Cabin dome light (LED, soft dimming) | PWM | 1 |
| **Output** | Acoustic buzzer | Digital Out | 1 |
| | **Total** | 3 DI + 1 ADC + 3 DO + 2 PWM | **9** |

**CAN Interface:** Transmits `BCM_STATUS` (0x103); Receives `BCM_CMD` (0x106), `DMS_ALERT` (0x110), and `WHEEL_SPEED` (0x101).

| Requirement ID | Priority | Specification |
| :-- | :-- | :-- |
| **FR-BCM-01** | Core | Sample door and lock switch status; broadcast `BCM_STATUS` periodically every 100 ms. |
| **FR-BCM-02** | Core | Process lock/unlock commands received via CAN or local push button; actuate door lock relays. |
| **FR-BCM-03** | Core | Automatically illuminate headlights when ambient light falls below a configurable darkness threshold. |
| **FR-BCM-04** | Core | Turn on cabin lighting when a door is opened or when the PIR sensor detects motion. |
| **FR-BCM-05** | Core | Upon receipt of `DMS_ALERT`, actuate the acoustic buzzer according to alert patterns mapped to severity levels (Low, High, Critical). |
| **FR-BCM-06** | Core | **Automatic speed-dependent door locking:** Lock doors when `Wheel_Speed` exceeds a configurable threshold (proposed default: 60 rpm). If `WHEEL_SPEED` is invalid or times out (E2E failure), disable auto-locking and record DTC 0xD00002 or 0xD00003. |
| **FR-BCM-07** | Core | **Ajar door warning while in motion:** Sound the buzzer if `Door_Open` = 1 while speed exceeds the motion threshold. If speed is invalid/unknown, suppress false alarms and record DTC. |
| **FR-BCM-08** | Nice-to-have | Anti-theft alarm state machine: Disarmed, Armed (10 s post-locking), Alarm (PIR or door opened while Armed), transition to Disarmed upon manual unlock; report `Alarm_State` in `BCM_STATUS`. |
| **FR-BCM-09** | Nice-to-have | Automatic headlights with hysteresis and time filtering (turn on after 2 s continuous darkness, turn off after 5 s continuous brightness); cabin lighting soft-dimming via PWM after door closure. |

### 2.2. Window ECU (Power Window Simulated in Software)

| Type | Peripheral Device | Pin Count |
| :-- | :-- | :-- |
| **Input** | "Obstacle" push button (triggers simulated anti-pinch) | 1 |
| **Common** | Status LED | 1 |
| **Actuator** | None (software-modeled motor dynamics) | 0 |

**CAN Interface:** Receives `WINDOW_CMD` (0x104) and `BCM_STATUS` (0x103); Transmits `WINDOW_STATUS` (0x204).

| Requirement ID | Priority | Specification |
| :-- | :-- | :-- |
| **FR-WIN-01** | Core | Consume target window angle (0–180°) from CAN; if angle is out-of-range, reject the command and log DTC 0xB30001. |
| **FR-WIN-02** | Core | Execute a simulated DC motor transfer function (1st or 2nd order with configurable parameters) governed by **position PID control** with integral anti-windup and trapezoidal velocity profiling. Target criteria: overshoot ≤ 5%, settling time ≤ 2.0 s for a 90° step response (empirically calibrated). |
| **FR-WIN-03** | Core | Broadcast `WINDOW_STATUS` cyclically every 100 ms (current angle, target angle, motion state, `Pinch_Detected`, `Child_Lock_State`). |
| **FR-WIN-04** | Core | **Simulated anti-pinch protection:** If the virtual motor current exceeds the stall threshold during window roll-up (injected via physical push button or debug CAN command), immediately halt motion, reverse downward by a predetermined offset, set `Pinch_Detected`, and log DTC 0xB30002. Target response deadline: < 100 ms. |
| **FR-WIN-05** | Core | **Auto-closure upon vehicle locking:** When `Lock_State` = Locked (extracted from `BCM_STATUS`), automatically roll up window unless anti-pinch is active. If `BCM_STATUS` is invalid, maintain current position. |
| **FR-WIN-06** | Nice-to-have | Child lock mode (`Child_Lock`): Ignore roll-down commands, accept roll-up commands only. |

### 2.3. Climate ECU

| Type | Peripheral Device | Pin Classification | Pin Count |
| :-- | :-- | :-- | :-- |
| **Input** | Temperature & humidity sensor (DHT11 / DHT22) | Bi-directional Digital | 1 |
| **Input** | Solar radiation sensor (LDR module) | ADC | 1 |
| **Output** | Ventilation fan (relay on/off; optional PWM via MOSFET) | Digital Out (PWM capable) | 1 |
| **Output** | Air distribution vent servo (SG90) | PWM | 1 |
| | **Total** | | **4** |

**CAN Interface:** Transmits `CLIMATE_STATUS` (0x102); Receives `CLIMATE_CMD` (0x105) and `DMS_ALERT` (0x110).

| Requirement ID | Priority | Specification |
| :-- | :-- | :-- |
| **FR-CLI-01** | Core | Read cabin temperature and humidity every 2.0 s (sensor sampling constraint); transmit `CLIMATE_STATUS` every 100 ms using the most recent valid sample. |
| **FR-CLI-02** | Core | Automatic climate control mode: Bang-bang fan actuation with configurable temperature hysteresis bands. |
| **FR-CLI-03** | Core | Manual override mode: Execute CAN commands setting fan state or fixed duty cycles. |
| **FR-CLI-04** | Core | Execute CAN commands positioning the vent servo angle (0–180°). |
| **FR-CLI-05** | Core | **Driver drowsiness cross-domain response:** Upon receiving `DMS_ALERT` with Severity ≥ High, force ventilation fan to 100% for up to 30 s before reverting to previous operating state; assert `Fan_Override` = 1 during the event. If `DMS_ALERT` is invalid or timed out, suppress override. |
| **FR-CLI-06** | Nice-to-have | Sensor plausibility filtering: 3-sample median filter, rate-of-change clamping; assert `DHT_Fault` and record DTC 0xB20001 if readings violate physical limits. |
| **FR-CLI-07** | Nice-to-have | Calculate dew point using the Magnus formula; assert `Fog_Risk` = 1 when cabin temperature approaches within 3.0 °C of dew point (proposed value). |

### 2.4. WSW ECU (Wheel Slip Warning)

| Type | Peripheral Device | Pin Classification | Pin Count |
| :-- | :-- | :-- | :-- |
| **Input** | Optical/magnetic wheel speed encoder | Timer Capture / EXTI | 1 |
| **Input** | Brake pedal push button | Digital In | 1 |
| **Output** | Wheel slip warning LED indicator | Digital Out | 1 |
| **Output** | Instrument warning lamp output | Digital Out | 1 |
| **Output** | **Motor driver interface (IN1, IN2, PWM)** | 2 DO + 1 PWM | 3 |
| | **Total** | | **7** |

**CAN Interface:** Transmits `WHEEL_SPEED` (0x101) and `WSW_WARNING` (0x100).

| Requirement ID | Priority | Specification |
| :-- | :-- | :-- |
| **FR-WSW-01** | Core | Measure the **elapsed period between consecutive encoder pulses** (timer input capture) to compute instantaneous wheel angular velocity; update the transmission buffer every 20 ms. (Pulse counting within a fixed 20 ms window is avoided due to quantization errors at low speeds). |
| **FR-WSW-02** | Core | Compute reference vehicle velocity using a moving average window ($N$ samples, configurable parameter). |
| **FR-WSW-03** | Core | Assert slip warning when the brake button is engaged and slip criteria are satisfied across **2 consecutive sampling cycles**. Apply **hysteresis** to slip ratio (entry threshold 20%, exit threshold 10%) with a minimum warning hold duration of 500 ms. Thresholds are empirically calibrated in Week 7. |
| **FR-WSW-04** | Core | Broadcast `WHEEL_SPEED` cyclically every 20 ms. |
| **FR-WSW-05** | Core | **Secondary deceleration criterion:** Assert `Decel_Exceeded` = 1 when wheel deceleration rate exceeds a configurable negative threshold within a 100 ms observation window. Fusion logic supports configurable AND / OR modes (defaulting to AND to suppress false positives). |
| **FR-WSW-06** | Core | **Snapshot Freeze Frame:** Capture diagnostic context into RAM upon slip warning assertion (wheel speed, slip ratio, deceleration, timestamps); accessible via UDS service 0x22 using DID 0x0120. Volatile memory (cleared upon reset). |
| **FR-WSW-07** | Nice-to-have | Standstill detection: If no encoder pulses are captured for > 500 ms, force wheel speed output to zero. |

> **Note:** The DC motor wheel kit requires an H-bridge driver module (TB6612FNG) for bidirectional PWM actuation, provisioned in the BOM.

### 2.5. Distributed Functions & Graceful Fail-Safe Degradation

The cross-domain distributed features below execute over the unified CAN matrix without supplemental wiring. The rightmost column defines safe degradation states during signal faults (E2E corruption or frame timeout).

| Distributed Function | Signal Origin | Executing Node | Fail-Safe Behavior Upon Corrupted / Lost Signal |
| :-- | :-- | :-- | :-- |
| **Speed-Dependent Auto-Lock** | `WHEEL_SPEED` (WSW ECU) | BCM ECU | Inhibit auto-locking; retain manual control; record DTC. |
| **Ajar Door Warning in Motion** | `WHEEL_SPEED` + `Door_Open` | BCM ECU | Suppress false acoustic alarms; record DTC. |
| **3-Level Drowsiness Reaction** | `DMS_ALERT` (DMS ECU) | BCM (buzzer), Climate (100% fan), Master (HMI) | BCM deactivates buzzer after 500 ms without valid frames; Climate cancels fan override. |
| **Auto-Window Rollup on Lock** | `Lock_State` (BCM ECU) | Window ECU | Inhibit auto-closure; freeze current window position. |

---

## 3. Common Carrier PCB Requirements

**Architectural Approach:** A custom carrier PCB socketing a standard **STM32F103C8T6 (Blue Pill)** via two 2.54 mm female headers. All 4 ECUs utilize identical carrier boards differentiated solely by firmware binaries. Components are selected for hand soldering (0805 passives, SOIC-8 ICs, screw terminals). Comprehensive specifications (decisions D1–D6, schematics, pin mapping, connectors, bring-up procedures) are documented in `PCB-Requirements-4ECU.md` v0.2.

**Carrier Board Subsystems:**

| Subsystem | Requirement |
| :-- | :-- |
| **CAN Bus** | SN65HVD230 (SOIC-8) connected to PA12 (TX) and PA11 (RX); Rs/STBY pin controlled via PB4; onboard 120 $\Omega$ termination resistor with 2-pin jumper enable (installed only on the two bus termination endpoints). |
| **Flashing & Debug** | SWD header for ST-Link (SWDIO, SWCLK, 3.3V, GND); UART debug header on PA9 (TX) / PA10 (RX); accessible BOOT0 jumper on the Blue Pill. |
| **User Interface** | Dedicated Fault LED (PA2), User Push Button (PA3), 2-bit ECU_ID hardware jumpers (PA4, PA5). |
| **I/O Connectors** | 2-channel ADC (PA0, PA1), PWM outputs on TIM3 (PB0, PB1) and TIM4 (PB6, PB7), timer capture for encoder on PA8 (TIM1_CH1), single-wire DHT interface on PB9, 4x Digital Outputs, 4x Digital Inputs (PB12–PB15). |
| **Logic Levels** | Relay driver outputs use open-drain topology with selectable 5V pull-up jumpers; PB5 excluded from 5V interfacing; PA0–PA7, PB0, PB1 are strictly non-5V-tolerant (3.3V max). |

**Power Architecture:**
- Input: 12V DC input $\rightarrow$ SS34 Schottky diode (reverse polarity protection) $\rightarrow$ Resettable Polyfuse $\rightarrow$ TVS transient suppression diode $\rightarrow$ **LM2596** 5V Buck step-down module (mounted via 2.54 mm pin header footprint) $\rightarrow$ Isolation jumper `JP_5V` $\rightarrow$ Blue Pill 5V rail; Blue Pill onboard LDO derives 3.3V logic.
- Disconnect `JP_5V` prior to attaching USB cables to the Blue Pill to prevent reverse-current damage.
- Electrically decouple the high-current 5V actuator rail (relays, servos, fan) from logic rails using bulk filter capacitors ($100\,\mu\text{F} - 470\,\mu\text{F}$) to prevent brownout resets during transient motor loads.
- Buck converter rated for minimum 2.0 A – 3.0 A continuous output.

**Mechanical Specifications:** Maximum PCB outline 100 mm $\times$ 80 mm, 4 corner M3 mounting holes; 2.54 mm headers for sensors, 5.08 mm screw terminal blocks for 12V power and CAN bus connections.

---

### Timer and PWM Pin Allocation Guidelines

- Headlights, cabin lighting (BCM), and optional fan PWM (Climate) must be mapped to distinct hardware PWM timer channels.
- On the WSW ECU, the timer capture channel for encoder speed sensing and the PWM channel driving the motor must reside on **two independent hardware timers** to eliminate frequency and prescaler conflicts.
- PA11 and PA12 are allocated to CAN (bxCAN); TIM1 channels multiplexed on these pins must remain unassigned.
- RC servos operate at 50 Hz PWM, whereas LED lighting requires high-frequency PWM (> 1 kHz) to eliminate flicker: assign them to separate timer instances.

---

## 3B. Bit Timing and CAN Controller Harmonization

Three distinct CAN controllers (bxCAN on STM32F103, Bosch M_CAN/FDCAN on STM32MP157, and Bosch mttcan on Jetson) communicate across a shared physical CAN 2.0A bus. **Mandatory Configuration:** Classical CAN mode (FD disabled), 500 kbps nominal bitrate, Sample Point calibrated between 80.0% – 87.5% (target: **83.3%**).

| Node | CAN Controller | Bit Timing Configuration Parameters |
| :-- | :-- | :-- |
| **4x STM32F103 ECUs** | bxCAN (`HAL_CAN`) | Assuming APB1 clock = 36 MHz: Prescaler = 4, Total Time Quanta = 18 ($1 \text{ TQ Sync} + 14 \text{ TQ BS1} + 3 \text{ TQ BS2}$), SJW = 1. Yields 500 kbps, 83.33% Sample Point. |
| **Master STM32MP157D-DK1** | FDCAN1 (`m_can`) | `ip link set can0 type can bitrate 500000 sample-point 0.833 restart-ms 100`; CAN-FD disabled. |
| **DMS ECU Jetson Orin Nano** | mttcan (`CAN0`) | Identical `ip link` configuration command as above. |

| Requirement ID | Specification | Verification Method |
| :-- | :-- | :-- |
| **FR-NET-01** | Enforce unified 500 kbps, 83.3% Sample Point timing across all 6 bus nodes. | Cross-check software driver registers and initialization scripts. |
| **FR-NET-02** | Incremental bring-up strategy: Blue Pill to DK1 (Week 4), add Jetson Orin Nano, then add ECUs sequentially. | Hardware bring-up test log. |
| **FR-NET-03** | Under nominal operating conditions, the Linux kernel bus error counter must remain zero. | `ip -details -statistics link show can0` |
| **FR-NET-04** | Continuous ground reference (GND) across all nodes; 120 $\Omega$ terminators populated strictly at the two bus endpoints; 3.3V transceivers utilized globally. | Resistance measurement: $60\,\Omega \pm 2\,\Omega$ across $CAN\_H$ and $CAN\_L$ with power off. |

---

## 4. Master Host Requirements (STM32MP157D-DK1)

| Requirement ID | Specification |
| :-- | :-- |
| **FR-MST-01** | Ingest and decode all CAN frames over Linux SocketCAN (`can0` via FDCAN1) matching the DBC matrix. |
| **FR-MST-02** | Qt6 graphical dashboard rendering live vehicle status: door ajar, lighting, cabin temperature, wheel speed, window position, and DMS state. |
| **FR-MST-03** | Transmit user control commands (`BCM_CMD`, `CLIMATE_CMD`, `WINDOW_CMD`) directly from the touchscreen UI. |
| **FR-MST-04** | Embedded telemetry data logger streaming timestamps and decoded signals to CSV / SQLite databases. |
| **FR-MST-05** | Onboard UDS diagnostic client issuing Service 0x22 (Read Data by Identifier) and Service 0x19 (Read DTC Information) to all 4 microcontrollers and the DMS ECU. |
| **FR-MST-06** | Real-time E2E integrity checker (CRC, Alive_Counter, DLC, timeout) for all incoming cyclic frames; flag corrupted signals as "N/A" on the UI and log fault events. |
| **FR-MST-07** | Display active and historical Diagnostic Trouble Codes (DTCs) retrieved via UDS Service 0x19 on the diagnostic screen. |
| **FR-MST-08** | Render "DMS Offline" alert when `DMS_STATUS` is missing for 3 consecutive cycles (300 ms timeout). |
| **FR-MST-09** | *(Nice-to-have)* HMI Alert Prioritization Manager: Priority arbitration (Wheel Slip > Drowsiness > Door Ajar), manual acknowledgement, and visual de-glitching. |
| **FR-MST-10** | Transmit `WINDOW_CMD` with `Child_Lock` control; visualize `Pinch_Detected`, `Alarm_State`, and `Fog_Risk` telemetry. |

---

## 4B. DMS ECU Requirements (Jetson Orin Nano)

| Requirement ID | Specification |
| :-- | :-- |
| **FR-DMS-01** | Enable native CAN0: pinmux configured via Jetson-IO, driver `mttcan`, SN65HVD230 (3.3V) transceiver attached to header pins 29 (CAN0_DIN) and 31 (CAN0_DOUT); `can0` operating at 500 kbps. |
| **FR-DMS-02** | Execute MediaPipe Face Mesh to calculate Eye Aspect Ratio (EAR) and head pose angles; trigger `DMS_ALERT` when eyes remain closed > 2.0 s or head deflection > 3.0 s. Configurable detection thresholds calibrated in Week 14. |
| **FR-DMS-03** | Broadcast `DMS_STATUS` cyclically every 100 ms (`Face_Detected`, `Camera_OK`, EAR, head pose, FPS) functioning as a telemetry heartbeat. |
| **FR-DMS-04** | All outgoing frames must include 4-bit `Alive_Counter` and CRC-8 checksums adhering to AUTOSAR E2E Profile 1 (generated via `e2e_reference.py`). |
| **FR-DMS-05** | If camera feed is interrupted or pipeline throughput drops below 10 FPS for > 5.0 s, record DTC 0xE00001 or 0xE00002 and transmit `Camera_OK` = 0. |
| **FR-DMS-06** | *(Optional)* Respond to UDS Service 0x22 requests for software version, uptime, and FPS telemetry at CAN ID 0x7E4 / 0x7EC. |
| **FR-DMS-07** | *(Nice-to-have)* 3-tier escalated drowsiness warning based on eye closure duration (Low > 1.0 s, High > 2.0 s, Critical > 4.0 s) and publish `Perclos_Pct` (percentage of eye closure over a 60 s sliding window) in `DMS_STATUS`. |
| **FR-DMS-08** | *(Nice-to-have)* Driver-specific EAR baseline auto-calibration during the initial 10 seconds of operation. |

---

## 5. Non-Functional Requirements

| ID | Requirement | Verification Method |
| :-- | :-- | :-- |
| **NFR-01** | CAN 500 kbps bus utilization remains under 10% under nominal cyclic load (projected: ~2% - 3%); empirical utilization reported. | Bit counter script processing `candump` logs. |
| **NFR-02** | Latency from wheel slip threshold breach to `WSW_WARNING` appearance on the bus is **< 100 ms** at nominal wheel speeds (> 100 rpm). Speeds below 100 rpm are logged without deterministic deadline guarantees. | Firmware GPIO timestamp vs. CAN bus sniffer timestamp. |
| **NFR-03** | Round-trip UDS diagnostic request/response completed in **< 100 ms**, provided UDS frames are processed **event-driven** in the communication task rather than polled in a slow 500 ms cycle. | Host Python script logging high-resolution SocketCAN timestamps. |
| **NFR-04** | DMS maintains a minimum throughput of **15 FPS** end-to-end (camera capture to `DMS_ALERT` dispatch, concurrent with HMI and CAN), with a design target of **25 FPS**. (Baseline standalone CPU inference measured: 43 FPS on CPU). | Frame rate counter instrumentation in DMS script. |
| **NFR-05** | Corrupted frames (CRC errors, repeated counters, counter jumps exceeding MAX_DELTA) are discarded; communication failures flagged within at most 3 frame cycles. | Automated fault injection matrix (Section 5A). |
| **NFR-06** | Microcontroller cyclic frame transmission period adheres to nominal intervals within ±10% jitter. `configTICK_RATE_HZ` configured to 1000 Hz to prevent timing quantization errors on 20 ms tasks. | High-resolution CAN bus log timestamp analysis. |

**Nominal Bus Load Estimation:** Approximately 90 frames/second $\times \sim 120\text{ bits/frame} \approx 11\text{ kbps}$, representing 2.2% of the 500 kbps bandwidth. Adding E2E CRC bytes increases bus loading negligibly.

---

## 5A. Automotive Quality Layer Requirements

| Requirement ID | Specification | Verification Method |
| :-- | :-- | :-- |
| **FR-E2E-01** | All CAN frames contain a 4-bit `Alive_Counter` and CRC-8 checksum at the final byte (CRC-8/SAE-J1850, polynomial `0x1D`, init `0xFF`, xorout `0xFF`, frame-specific DataID). | DBC matrix check, `e2e_reference.py` automated self-test. |
| **FR-E2E-02** | Receiving nodes validate DLC, CRC, and sequence counters before passing data to application layers; only INITIAL, OK, and OK_SOME_LOST frames are accepted. | Host Unit Tests (Unity) + Fault injection. |
| **FR-E2E-03** | Fault debounce filter: 3 consecutive faulty frames mark signal as INVALID; 3 consecutive valid frames restore operational state. | Host Unit Tests (Unity). |
| **FR-FM-01** | Hardware Independent Watchdog (IWDG) coupled with multi-task Alive Supervision (WdgM): watchdog refreshed only if all registered tasks report alive checkpoints within the period; a hung task triggers an automatic hardware reset. | Fault injection test case #6. |
| **FR-FM-02** | Reception timeout: If a cyclic frame is absent for 3 consecutive periods, mark signal as INVALID and log DTC 0xD00003. | Fault injection test case #4. |
| **FR-FM-03** | Automatic recovery upon CAN Controller Bus-Off; log DTC 0xD00001. | Fault injection test case #7. |
| **FR-FM-04** | Each ECU stores DTC records and responds to UDS Service 0x19 queries. Each ECU defines a safe state (Section 2.5). | Automated UDS test script. |
| **FR-FM-05** | Climate ECU: When `DHT_Fault` = 1, force ventilation fan to 50% duty cycle and log DTC 0xB20001. | Fault injection test case #5. |
| **FR-TIM-01** | Measure cyclic transmission jitter (p50, p99, max) for every periodic frame from timestamped bus logs. | Python log parsing script. |
| **FR-TIM-02** | Perform Worst-Case Response-Time (WCRT) analysis for all CAN frames and correlate theoretical predictions with empirical measurements under load. | Python analysis script + physical bus measurements. |
| **FR-TIM-03** | Measure CPU utilization and maximum stack high-water mark for every FreeRTOS task across all ECUs. | FreeRTOS runtime stats & `uxTaskGetStackHighWaterMark`. |
| **FR-VER-01** | Host Unit Tests (Unity + gcc running via CTest) for pure C modules: CRC-8, E2E state machine, wheel slip math, Climate hysteresis, DTC manager. | `ctest` command execution. |
| **FR-VER-02** | Execute the complete Fault Injection Matrix; report explicit Pass/Fail status for each case. | Test execution report. |
| **FR-VER-03** | Static analysis (cppcheck, clang-tidy) with zero high-severity warnings; coding guidelines aligned with MISRA C principles (no dynamic allocation, no recursion). | Static analysis report. |
| **FR-VER-04** | Requirements Traceability Matrix: Every functional requirement (FR) mapped to at least one test case. | Traceability matrix table. |
| **FR-SAF-01** | FMEA-lite analysis for safety-critical signals `WSW_WARNING` and `DMS_ALERT` (failure modes, system effects, detection methods, mitigations). | Safety engineering report. |
| **NFR-07** | Continuous endurance soak test for minimum **4 hours** (overnight recommended). Criteria: Zero unintended watchdog resets; zero frame loss and zero CRC errors under nominal bus conditions. | Linux master continuous log. |
| **NFR-08** | 100% of functional requirements covered by verified test cases. | Requirements Traceability Matrix. |

**Fault Injection Matrix:**

| Test Case | Injected Fault | Injection Mechanism | Expected System Response |
| :--: | :-- | :-- | :-- |
| **1** | Corrupted CRC Checksum | Broadcast CAN command with inverted CRC byte using `cansend`. | Drop frame; log DTC 0xD00002 after 3 consecutive corruptions. |
| **2** | Frame Replay Attack | Broadcast duplicated CAN frame with frozen `Alive_Counter`. | Tagged as REPEATED; frame dropped. |
| **3** | Sequence Counter Jump | Transmit frames with counter skipping +2, +3, then $\ge 4$. | Flagged as OK_SOME_LOST then WRONG_SEQUENCE; frame dropped. |
| **4** | Cyclic Frame Dropout | Physically disconnect target ECU or kill DMS process. | After 3 missed periods: declare INVALID, display "N/A" on HMI, log DTC 0xD00003. |
| **5** | Sensor Hardware Disconnection | Disconnect DHT sensor wire from Climate ECU. | Assert `DHT_Fault` = 1; fan runs at 50% safe speed; log DTC 0xB20001. |
| **6** | Firmware Deadlock / Hang | Inject infinite loop into a test task build. | Alive supervision detects missing checkpoint; IWDG triggers MCU hardware reset. |
| **7** | CAN Bus-Off Forcing | Connect misconfigured node operating at mismatched bitrate. | CAN peripheral recovers automatically; logs DTC 0xD00001. |
| **8** | Out-of-Range Command | Transmit `WINDOW_CMD` requesting a 200° roll angle. | Command rejected; window holds position; logs DTC 0xB30001. |

---

## 5B. Experimental Evaluation Plan

**Measurement Methodology:** Report empirical data using statistical percentiles (p50, p95, p99, max) alongside arithmetic means. Rigorously record test conditions (git commit hash, Jetson power mode, ambient temperature, background bus load). Measure inter-node communication latency using a **common reference clock** (multi-channel logic analyzer capturing MCU GPIO toggle pins concurrently with CAN differential bus lines, or hardware-timestamped CAN sniffer). Negative or unexpected results must be documented with engineering root-cause analysis.

| Test ID | Experiment Description | Independent Variables | Target Metrics | Sample Size |
| :--: | :-- | :-- | :-- | :-- |
| **E1** | E2E Protection Effectiveness | E2E Enabled vs. Disabled across all 8 Fault Injection cases | Detection rate per fault type; undetected corruptions when E2E disabled; fault-to-DTC latency | $\ge 100$ iterations per fault mode |
| **E2** | CAN Bus Response Time | Background bus injection load: 2%, 30%, 60% (via `cangen`) | Theoretical vs. empirical worst-case response times per frame; latency vs. load curves | Thousands of frames per load level |
| **E3** | Watchdog Alive Supervision | Simple hardware IWDG vs. Multi-task Alive Supervision | Fault detection capability and time-to-reset when a single non-watchdog task hangs | $\ge 20$ iterations per configuration |
| **E4** | Critical Safety Latency | Event generation to bus frame arrival: `WSW_WARNING`; `DMS_ALERT` to buzzer sound | Latency percentiles (p50 / p95 / max) | $\ge 30$ trials per path |
| **E5** | Cyclic Jitter & Resources | 4-hour continuous endurance soak test | Cyclic jitter p99 per frame, per-task CPU % and stack margin, dropped packets, CRC errors, resets | Entire 4-hour continuous log |
| **E6** | Power Window Control | Step commands (30°, 90°, 150°); obstacle injection | Overshoot %, settling time; anti-pinch reaction latency | $\ge 10$ steps each; $\ge 30$ pinch events |
| **E7** | Wheel Slip Warning Logic | Hard braking vs. gradual braking; slip, decel, AND, OR modes | Alert classification confusion matrix; trigger response time | $\ge 30$ runs per scenario |
| **E8** | Driver Monitoring System | 3 lighting conditions (bright, dim, backlight); simulated driver behavior | End-to-end FPS, detection accuracy, false positive rate, alert latency | $\ge 30$ trials per condition |
| **E9** | Distributed Graceful Degradation | Signal source cutoff (disconnect WSW, DMS, BCM) | Verify safe-state degradation conforms to Section 2.5 | $\ge 10$ trials per function |

---

## 6. Preliminary CAN Communication Matrix

**Arbitration Principle:** On CAN networks, **lower numerical IDs indicate higher arbitration priority**. Consequently, safety-critical messages (`WSW_WARNING`) are assigned the lowest numeric identifiers.

| CAN ID | Message Name | Transmitter $\rightarrow$ Receiver | Transmission Type |
| :-- | :-- | :-- | :-- |
| **0x100** | `WSW_WARNING` | WSW ECU $\rightarrow$ Master, Bus | Event-triggered |
| **0x101** | `WHEEL_SPEED` | WSW ECU $\rightarrow$ BCM, Master | Cyclic 20 ms |
| **0x102** | `CLIMATE_STATUS` | Climate ECU $\rightarrow$ Master | Cyclic 100 ms |
| **0x103** | `BCM_STATUS` | BCM ECU $\rightarrow$ Window, Master | Cyclic 100 ms |
| **0x104** | `WINDOW_CMD` | Master $\rightarrow$ Window ECU | Event-triggered |
| **0x105** | `CLIMATE_CMD` | Master $\rightarrow$ Climate ECU | Event-triggered |
| **0x106** | `BCM_CMD` | Master $\rightarrow$ BCM ECU | Event-triggered |
| **0x110** | `DMS_ALERT` | DMS ECU $\rightarrow$ BCM, Climate, Master | Event-triggered |
| **0x111** | `DMS_STATUS` | DMS ECU $\rightarrow$ Master | Cyclic 100 ms |
| **0x204** | `WINDOW_STATUS` | Window ECU $\rightarrow$ Master | Cyclic 100 ms |
| **0x7E0 / 0x7E8** | UDS BCM (Req / Resp) | Master $\leftrightarrow$ BCM ECU | Diagnostic / Event |
| **0x7E1 / 0x7E9** | UDS Window (Req / Resp) | Master $\leftrightarrow$ Window ECU | Diagnostic / Event |
| **0x7E2 / 0x7EA** | UDS Climate (Req / Resp) | Master $\leftrightarrow$ Climate ECU | Diagnostic / Event |
| **0x7E3 / 0x7EB** | UDS WSW (Req / Resp) | Master $\leftrightarrow$ WSW ECU | Diagnostic / Event |
| **0x7E4 / 0x7EC** | UDS DMS ECU (Optional) | Master $\leftrightarrow$ DMS ECU | Diagnostic / Event |

*Complete signal-level bit packing and byte layouts are formally specified in the network DBC file `can_matrix.dbc` and `CAN-byte-layout.md`.*

---

## 7. Scope Boundaries and Engineering Risk Analysis

| Engineering Domain | Evaluation & Mitigation Strategy |
| :-- | :-- |
| **Microcontroller Bootloader / CAN OTA** | Documented in conceptual plan but **excluded from the 16-week execution timeline**. Classified as a stretch goal; only implemented if all primary objectives finish ahead of schedule. |
| **Window ECU Physical Actuation** | Data-only simulation. The thesis will explicitly state this scoping boundary to concentrate efforts on real-time embedded CAN networking and control theory. |
| **PID Closed-Loop Control** | Maintained in the Window ECU position controller to demonstrate control systems engineering competencies. |
| **WSW Motor Driver** | Standardized on the TB6612FNG dual H-bridge module; incorporated into the hardware bill of materials. |
| **Removal of CAN IDS** | **Removed in v2.** Substituted with the Automotive Quality Layer: E2E protection, fault management, timing response analysis, verification testing, and FMEA-lite. |
| **DMS Performance on Jetson** | **Low Risk (Validated):** MediaPipe achieves 43.0 FPS on CPU standalone. Total latency re-measured with USB camera capture and CAN transmission overhead. Dependencies locked to `mediapipe==0.10.14` and `numpy<2` due to Linux aarch64 wheel availability. |
| **FDCAN on STM32MP157D-DK1** | Requires device tree customization (only FDCAN1 on pins PA11 and PD1 is routed to external headers). Resolved in Week 4 with verified loopback and transceiver bring-up. |
| **Qt6 UI on OpenSTLinux** | Utilize ST's official `X-LINUX-QT` package or custom Yocto build. Dashboard kept concise to maintain schedule adherence. |
| **ISO-TP on FreeRTOS** | Decided in Week 5: integrate lightweight C library (`isotp-c`) or constrain UDS diagnostic payloads to single CAN frames (SF). |

---

## 8. Immediate Action Items

1. Verify the hardware pin mapping against all purchased sensors and breakout modules.
2. Finalize the detailed E/E system block diagram.
3. Complete signal byte encoding in `can_matrix.dbc` and generate C serialization stubs using `cantools`.
4. Procure remaining hardware components with extended delivery lead times.
5. Finalize KiCad schematic design for the 4-ECU Carrier Board.
