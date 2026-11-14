# 4-Month Project Implementation Plan — Version v3

## Design and Evaluation of a Miniature Automotive E/E Platform over CAN with End-to-End Protection and Fault Management

### Official Direction: Automotive Development Platform — 4 STM32F103 ECUs running FreeRTOS + Master STM32MP157D-DK1 (Linux, Qt HMI) + Jetson Orin Nano as DMS ECU + Automotive Quality Layer

> The thesis is structured as a **miniature automotive development platform** following a distributed E/E architecture: **6 nodes communicating across a single 500 kbps CAN bus**. Four microcontroller ECUs (BCM, Window, Climate, WSW) run **FreeRTOS + STM32 HAL**; the **Master host is an STM32MP157D-DK1** (OpenSTLinux on Cortex-A7) hosting a Qt6 HMI, UDS diagnostic tester, E2E integrity monitor, and data logger; the **Jetson Orin Nano acts as a vision-based Driver Monitoring System (DMS) ECU**, connected directly to the bus via native CAN0. The primary technical focus centers on **embedded firmware, CAN networking, real-time operating systems, and automotive quality mechanisms (E2E protection, fault management, timing analysis, and software verification)**, alongside distributed cross-domain features with deterministic fail-safe degradation.
>
> **Architectural Decision History (to prevent retrofits):**
> - **RTOS: FreeRTOS + STM32 HAL** (replacing Zephyr), built with **CMake + VS Code** (FreeRTOS-Kernel and STM32CubeF1 as git submodules). Rationale: Faster ramp-up within the limited timeline, minimal memory footprint on resource-constrained microcontrollers (64 KB Flash / 20 KB RAM), and complements prior experience with Zephyr (on Cortex-M4) and Yocto (on Cortex-A7) to broaden the engineering portfolio. Custom implementations are developed for CAN drivers, ISO-TP transport framing, DTC management, and host-based unit tests.
> - **Hardware Architecture:** Master host is the **STM32MP157D-DK1** (OpenSTLinux Starter package booted); **Jetson Orin Nano serves as the DMS ECU** (JetPack installed, baseline CPU inference validated at 43.0 FPS). **The MP157 Cortex-M4 coprocessor is excluded from the primary scope** (reserved as an optional stretch goal).
> - **Omission of CAN IDS** (heavy on machine learning, deviating from core embedded automotive systems), replaced by the **Automotive Quality Layer**. **ABS renamed to WSW (Wheel Slip Warning)** because the system monitors slip under braking rather than modulating hydraulic brake pressure.
> - **ECU Functional Logic:** The Window ECU incorporates a **closed-loop position PID controller + simulated anti-pinch protection**; Climate maintains bang-bang control with temperature hysteresis; WSW adds deceleration criteria, slip hysteresis, and freeze-frame diagnostics. All additions are software-based without requiring extra hardware complexity.
> - **Standardized AUTOSAR Classic Firmware Architecture:** Firmware across all 4 microcontrollers adheres to the **AUTOSAR Classic Layered Architecture** (Application SWCs, Runtime Environment RTE, BSW Services: COM, E2E Profile 1, UDS DCM/DEM, WdgM Alive Supervision, CanIf/IoHwAb, MCAL, and FreeRTOS) following a **Spec-compliant Custom BSW** approach. Due to STM32F103 memory limitations (64 KB Flash, 20 KB RAM), the project does not claim commercial certification for ISO 26262 or Full-Stack AUTOSAR. Detailed architecture is documented in `doc/Kien-truc-Firmware-AUTOSAR.md`. Bootloader/CAN-OTA is an optional stretch goal.

**Hardware Finalization Summary:** The Window ECU operates **without physical actuators** (window dynamics are simulated via a motor transfer function in software, using a physical push button to inject obstacle conditions). The WSW ECU utilizes a pre-assembled "gearmotor + wheel + optical speed encoder" kit paired with a TB6612FNG H-bridge module; reference vehicle speed is derived via a software moving average filter; physical hydraulic valves are represented by warning LEDs.

---

## 1. Summary of System Components to Build

| Component | Responsibility / Technology Stack |
| :---- | :---- |
| **Master — STM32MP157D-DK1** (OpenSTLinux on Cortex-A7, CAN via FDCAN1 + SN65HVD230) | Qt6 graphical dashboard, **UDS diagnostic tester**, **E2E reception checker**, telemetry logger, DTC / freeze-frame visualization, control command dispatch (`BCM_CMD`, `CLIMATE_CMD`, `WINDOW_CMD`). |
| **DMS ECU — Jetson Orin Nano** (JetPack, native CAN0 via 40-pin header + SN65HVD230) | Vision pipeline processing (MediaPipe Face Mesh), broadcasting `DMS_ALERT` and `DMS_STATUS` (heartbeat + telemetry). |
| **CAN Bus 500 kbps** | Central backbone interconnecting Master, DMS ECU, and 4 microcontrollers; 6 physical nodes, dual 120 $\Omega$ split terminations at endpoints, continuous ground reference. |
| **BCM ECU** (STM32F103 + FreeRTOS) | Body control: door reed switch, lock button, LDR, PIR; door lock relays, headlights, cabin dimming LED, buzzer. Logic: auto-lock in motion, door ajar warning in motion, `DMS_ALERT` buzzer actuation; nice-to-have: anti-theft state machine, headlight hysteresis. |
| **Window ECU** | Simulated power window: software DC motor dynamic model + **position PID** + trapezoidal profile + **simulated anti-pinch**; auto-rollup on vehicle lock; nice-to-have: child lock. |
| **Climate ECU** | Cabin comfort: DHT22, LDR, ventilation fan, SG90 servo vent. Logic: bang-bang temperature hysteresis; maximum fan purge upon critical drowsiness alert; nice-to-have: sensor filtering, dew point calculation, windshield defog flag. |
| **WSW ECU (Wheel Slip Warning)** | Active safety: DC motor wheel kit, TB6612FNG driver, encoder, brake button, warning LED. Logic: period measurement between encoder pulses, moving average reference speed, **slip and deceleration dual-criteria** with hysteresis, **snapshot freeze frames** via UDS. |
| **Driver Camera (USB Webcam / CSI)** | Video input source for the DMS ECU. |
| **HDMI Display for Master** | Recommended: 7-inch 1024x600 capacitive touch display (MP157 pixel clock max 90 MHz, avoiding native 1080p); verified via `modetest -M stm`. |
| **Automotive Quality Layer** | AUTOSAR E2E Profile 1 (Alive_Counter + CRC-8) on all frames, Watchdog Manager with multi-task Alive Supervision, timeout monitoring, bus-off auto-recovery, UDS DTCs, timing analysis, host unit tests, fault injection matrix, FMEA-lite. |
| **Physical Test Rig** | Acrylic panel on aluminum extrusion frame housing Master, Jetson, 4 ECUs, CAN bus wiring, camera, and display. |

**Microcontroller Firmware Architecture (AUTOSAR Classic Model):**  
Firmware across all 4 microcontrollers implements the standardized **AUTOSAR Classic** layered architecture (detailed in [Kien-truc-Firmware-AUTOSAR.md](Kien-truc-Firmware-AUTOSAR.md)):
- **Application Layer (`apps/ecu_*`):** Software Components (`SWC_BCM`, `SWC_Window`, `SWC_Climate`, `SWC_WSW`) executing periodic and event-driven Runnables, decoupled from hardware.
- **RTE Layer (`rte/`):** Virtual Function Bus abstraction APIs (`Rte_Read_*`, `Rte_Write_*`, `Rte_Call_*`) interconnecting SWCs and BSW.
- **BSW Services Layer (`common/`):** `can_com` (COM/PduR), `e2e_crc8` (AUTOSAR E2E Profile 1), `diag_dcm` (ISO 14229 UDS), `diag_dem` (DTC lifecycle, RAM freeze frames), `wdgm` (Watchdog Manager & Alive Supervision).
- **ECU Abstraction & MCAL:** [`board_pins.h`](../ecu-firmware/common/board_pins.h) (IoHwAb), `can_driver.c/.h` (CanIf + bxCAN MCAL at 500 kbps, 83.3% SP), STM32 HAL.
- **OS Layer (FreeRTOS V11.2.0 Static Allocation):** Schedules SWC Runnables and BSW MainFunctions via dedicated FreeRTOS tasks:

| FreeRTOS Dispatcher Task | Period / Trigger | Executed Runnables / BSW Modules |
| :---- | :---- | :---- |
| **CanRx / Comm Dispatcher** | bxCAN FIFO0 ISR pushing to Queue | High priority: CanIf RX, E2E Receiver integrity check, PduR routing to COM / DCM |
| **CanTx / Cyclic Comm** | 20 ms and 100 ms (`vTaskDelayUntil`) | COM signal packing, E2E protection (counter + CRC-8), transmission via CanIf TX |
| **Sensor / IoHwAb Runnable** | 10 ms | ADC/GPIO sampling, encoder timer capture, sensor low-pass filtering |
| **Control / SWC Runnable** | 10 ms / 20 ms | SWC application logic (Window PID, anti-pinch, WSW slip math, Climate hysteresis) |
| **Diag / DCM UDS** | Event-driven (PduR notification from CanRx) | UDS services 0x10, 0x22, 0x19, 0x14; **not** placed in a slow 500 ms polling loop |
| **WdgM / Supervisor** | 100 ms | Alive Supervision validating task checkpoint counters before servicing hardware IWDG |

`configTICK_RATE_HZ` = 1000, 100% static allocation (dynamic heap disabled), stack overflow hooks and runtime stats enabled.

**Microcontroller Toolchain:** `arm-none-eabi-gcc`, CMake (with Presets), FreeRTOS-Kernel and STM32CubeF1 (HAL/CMSIS) as submodules, VS Code + OpenOCD + Cortex-Debug via ST-Link SWD, Unity + gcc for host unit testing (CTest), `cantools generate_c_source` for automated DBC C serialization.

**Master Host Toolchain:** OpenSTLinux (Yocto; Starter Package booted), ST **X-LINUX-QT (Qt6)** or custom Yocto image with Qt6, SocketCAN on FDCAN1 (PA11 is RX, PD1 is TX) via SN65HVD230, `can-utils`, `can-isotp`.

**DMS ECU Toolchain (Jetson):** JetPack (installed), Python 3, OpenCV, **`mediapipe==0.10.14` and `numpy<2`** (newer versions deprecate `mp.solutions` and lack Linux aarch64 wheels), `python-can`, `can-isotp`, native CAN0 via Jetson-IO and `mttcan` driver.

**Bit Timing Harmonization:** Classical CAN 2.0A, 500 kbps, 83.3% Sample Point across all three controllers (bxCAN, FDCAN, mttcan). Specifications detailed in `Yeu-cau-he-thong-ECU.md` Section 3B.

**Diagnostics:** Tailored ISO 14229 UDS: 0x22 (Read Data by Identifier), 0x19 (Read DTC Information), optional 0x14 (Clear Diagnostic Information). Master functions as the diagnostic tester. DTCs generated from physical and communication fault conditions. WSW includes snapshot freeze frames (DID 0x0120, stored in volatile RAM). Single-frame UDS or lightweight ISO-TP (`isotp-c`) confirmed in Week 5.

**Distributed Functions & Graceful Degradation:** Speed-dependent auto-lock (WSW $\rightarrow$ BCM), ajar door warning in motion (WSW + BCM), 3-level drowsiness alert (DMS $\rightarrow$ BCM buzzer, Climate fan purge, Master HMI), auto-window rollup on vehicle lock (BCM $\rightarrow$ Window). Signal faults (E2E corruption or timeout) trigger deterministic fail-safe degradation.

**Driver Monitoring System (DMS):** MediaPipe Face Mesh estimating EAR and head pose, 3-level escalated alerting based on eye closure duration; nice-to-have: PERCLOS (60 s sliding window) and driver baseline EAR calibration.

---

## 2. Five Pillars of the Platform

1. **Homogeneous Microcontroller Hardware:** 4 identical carrier PCBs sharing the same schematic layout, differentiated exclusively by firmware.
2. **Industry-Standard RTOS Architecture:** FreeRTOS + STM32 HAL across all 4 ECUs, built via CMake, utilizing layered modular code.
3. **CAN Bus Network Backbone:** All communication unified under a standardized DBC matrix, protected by AUTOSAR E2E mechanisms.
4. **Heterogeneous Multi-Domain Architecture:** Linux Master (MP157: HMI, UDS tester), DMS ECU (Jetson: Edge AI), and 4 Real-Time Microcontroller ECUs. Safety-critical warning paths (DMS $\rightarrow$ BCM, WSW $\rightarrow$ Master) operate autonomously without transiting the Linux host OS.
5. **Automotive Quality & Quantitative Validation:** E2E protection, fault management, timing response analysis, host unit testing, FMEA-lite, and controlled comparative experiments (E1–E9). This constitutes the core academic and engineering depth of the thesis.

---

## 3. High-Level Monthly Roadmap

| Month | Engineering Focus | Primary Milestone Deliverables |
| :---- | :---- | :---- |
| **Month 1** | System Architecture, DBC v0.5, PCB Carrier Design, FreeRTOS + CMake Toolchain Setup, CAN Bring-Up between Blue Pill and DK1 | Architectural Design Documentation + Fabricated PCBs ordered + Blue Pill transmitting CAN frames to DK1 |
| **Month 2** | FreeRTOS Firmware on 4 ECUs, Shared BSW Libraries (E2E, DTC, WdgM), Local ECU Domain Logic, Host Unit Tests | 4 ECUs operating standalone, transmitting/receiving CAN with E2E protection, core control logic operational |
| **Month 3** | DMS ECU (Jetson CAN0), Master Host (Qt6 HMI, E2E Checker, Logger), Physical Rig Assembly, Soak Testing, FMEA-lite | 6-node platform operational end-to-end, distributed cross-domain features verified |
| **Month 4** | UDS Diagnostics + DTCs, Quantitative Experiments (E1–E9), Verification Suite, Thesis Writing, Defense | Complete Thesis Manuscript + Experimental Dataset + Live Demonstration Rig + Defense Examination |

---

## 4. Detailed Weekly Plan (16 Weeks)

### 📅 MONTH 1 — Architecture Design, Carrier PCB, and Toolchain Bring-Up

| Week | Planned Tasks | Milestone Deliverable |
| :---- | :---- | :---- |
| **1** | Finalize system requirements and architecture based on `Yeu-cau-he-thong-ECU.md`; construct E/E block diagrams; finalize BOM (DK1, Jetson, and microSD on hand); **place Batch 1 procurement order** (Blue Pills, CAN transceiver modules, ST-Link, USB-UART adapters, logic analyzer, USB-CAN candleLight). | Requirements & Architecture Document + System Block Diagram + Batch 1 Order Invoices |
| **2** | KiCad schematic design for the **unified 4-ECU Carrier Board socketing the Blue Pill** (female headers, SN65HVD230, 12V input with LM2596 module, SWD, connectors, LEDs, user buttons, ECU_ID jumpers; see `PCB-Requirements-4ECU.md`); **pin mapping verification** for PWM and timers (BCM 2x PWM, Climate servo PWM, WSW encoder capture and motor PWM on separate timers, PA11/PA12 dedicated to CAN); freeze **DBC v0.5** (10 frames, each with Alive_Counter + CRC-8) and nominal bit timing. | Schematic Capture + Verified Pin Mapping + DBC Matrix + E2E Specification |
| **3** | Layout carrier PCB (4 identical boards, max 100 x 80 mm, 0805/SOIC-8 footprinting for hand soldering), DRC checks; **empirically measure physical pin row spacing on physical Blue Pill modules before finalizing header footprint**; **submit Gerbers for fab order this week**; place Batch 2 component order. Concurrently on DK1: Inspect FDCAN in device tree (`find /proc/device-tree -name 'can@*'`, `dmesg \| grep -i can`), verify PA11/PD1 pin routing from schematic MB1272, prepare SN65HVD230; execute `modetest -M stm` to test display output. | Gerbers dispatched to fab + Component Orders + DK1 FDCAN status log |
| **4** | While awaiting PCB delivery: Construct **CMake** firmware workspace (FreeRTOS-Kernel + STM32CubeF1 HAL, `arm-none-eabi-gcc`, VS Code + OpenOCD + Cortex-Debug) targeting Blue Pill: Blinky, UART logging, bxCAN loopback; automated **Unity host test** for CRC-8; **bring up physical CAN link (Blue Pill ↔ DK1)**: enable FDCAN1 in device tree, configure `ip link set can0 type can bitrate 500000 sample-point 0.833`, verify bidirectional communication via `candump`/`cansend` and confirm zero bus error count. | Operational FreeRTOS + CMake environment; Verified CAN frame transmission between Blue Pill and DK1 with 0 bus errors |

**✅ End of Month 1 Achievements:** Fully defined architecture, Carrier PCB in fabrication, DBC and E2E specifications frozen, FreeRTOS firmware toolchain verified on development hardware, inter-controller CAN communication between bxCAN and FDCAN validated.

---

### 📅 MONTH 2 — FreeRTOS Firmware Implementation for 4 ECUs

| Week | Planned Tasks | Milestone Deliverable |
| :---- | :---- | :---- |
| **5** | Refactor firmware into an **AUTOSAR Classic** layered architecture: multi-target CMake under `apps/ecu_*`, Virtual Function Bus interfaces in `rte/`, BSW modules in `common/`, platform drivers in `platform/`; configure `FreeRTOSConfig.h` (1000 Hz tick, 100% static allocation, stack overflow hooks, runtime stats); implement task dispatchers; automate DBC C serialization using `cantools generate_c_source` for **`can_com`**; implement core BSW modules: **`can_driver`** (CanIf + bxCAN MCAL), **`e2e_crc8`** (AUTOSAR E2E Profile 1, verified against `e2e_reference.py`), **`diag_dcm`** (ISO 14229 UDS), **`diag_dem`** (3-byte DTCs, 3-count debounce, RAM freeze frames), **`wdgm`** (Alive Supervision + IWDG); finalize ISO-TP strategy; run automated Unity unit tests for CRC-8, E2E state machine, and DCM/DEM via CTest. Upon PCB arrival: bring up power rails, SWD flashing, and CAN transceivers. | Layered AUTOSAR firmware base + validated BSW suite (`e2e_crc8`, `diag_dcm`, `diag_dem`, `wdgm`, `can_com`) passing tests; First carrier PCB brought up |
| **6** | Implement **BCM ECU** and **Window ECU** applications. BCM: Door/lock/LDR/PIR sensor acquisition, lock relays, lighting outputs, multi-level buzzer; **speed-dependent auto-lock** and **ajar door warning in motion** (FR-BCM-06, 07); nice-to-have: anti-theft state machine, headlight hysteresis, cabin dimming. Window: **DC motor dynamic simulation + position PID** with anti-windup and trapezoidal profiling, **simulated anti-pinch**, **auto-rollup upon locking**, out-of-range command rejection (FR-WIN-01..05); nice-to-have: child lock. Route all communication through `e2e`. | 2 ECUs operating standalone, broadcasting/receiving correct CAN IDs, demonstrating graceful degradation |
| **7** | Implement **Climate ECU** and **WSW ECU** applications. Climate: DHT/LDR sampling, bang-bang control with hysteresis, maximum ventilation override upon critical `DMS_ALERT` (FR-CLI-05), sensor fault handling; nice-to-have: median filter, dew point estimation, `Fog_Risk`. WSW: Measure **period between consecutive encoder pulses** (timer capture), moving average reference speed calculation, **slip and deceleration dual-criteria** with hysteresis (20% entry, 10% exit, 2-sample confirmation), **snapshot freeze frames** in RAM, standstill detection; **empirically calibrate thresholds**; host unit tests for slip math, deceleration filtering, and hysteresis. | All 4 ECUs running standalone on hardware + passing unit tests |
| **8** | Individual ECU hardware verification using CAN analyzer + **logic analyzer**; OpenOCD debugging; execute **fault injection cases #1–3 and #5–8**; profile CPU utilization and task stack watermarks; preliminary cyclic jitter measurement; execute **Experiment E3** (Simple IWDG vs. Alive Supervision); compile midterm thesis progress report. | 4 ECUs fully validated + Initial Fault Injection Matrix report + Experiment E3 results |

**✅ End of Month 2 Achievements:** All 4 microcontrollers running FreeRTOS reliably on physical carrier hardware, core application logic verified, 100% of cyclic CAN frames protected by AUTOSAR E2E mechanisms, active Watchdog Manager and DTC handling operational.

---

### 📅 MONTH 3 — DMS ECU, Master Host, and System Integration

| Week | Planned Tasks | Milestone Deliverable |
| :---- | :---- | :---- |
| **9** | **DMS ECU (Jetson):** Activate native CAN0 (Jetson-IO pinmux, `mttcan` driver, SN65HVD230 on pins 29 and 31), broadcast `DMS_STATUS` with E2E protection (using `e2e_reference.py`), camera video capture pipeline (JetPack installed, `mediapipe==0.10.14` validated at 43.0 FPS CPU baseline). **Master Host (DK1):** Configure Qt6 development environment (ST X-LINUX-QT or custom Yocto image), execute baseline "Hello Qt" application on HDMI display. | Jetson streaming `DMS_STATUS` to CAN bus, observable via `candump` on Master; Qt application running on DK1 display |
| **10** | **DMS ECU:** Calculate EAR + head pose angles, implement 3-level drowsiness escalation based on closure duration, broadcast `DMS_ALERT`; nice-to-have: PERCLOS calculation, driver baseline EAR calibration. **Master Host:** Develop Qt6 HMI dashboard (CAN decoding via DBC-generated stubs), **E2E verification monitor**, telemetry data logger (CSV/SQLite), render "N/A" for corrupted signals, flag "DMS Offline" upon 3-cycle timeout. | DMS detecting drowsiness and transmitting CAN alerts; Qt HMI rendering live vehicle telemetry |
| **11** | Physical Test Rig Integration: Mount Master DK1, Jetson Orin Nano, 4 ECUs, camera, and display onto aluminum frame; wire central CAN bus (common GND, dual 120 $\Omega$ split terminations, incremental node integration); execute end-to-end integration scenarios: locking car triggers auto-window rollup, driving triggers auto-locking, door ajar in motion triggers alarm, DMS drowsiness triggers buzzer + max fan, WSW triggers slip warning. | Fully integrated 6-node platform operational on physical test rig |
| **12** | System-wide verification testing; **continuous endurance soak test $\ge$ 4 hours (overnight recommended)** (Pass criteria: Zero unintended resets, zero nominal CRC errors); execute **Experiment E9** (disconnect WSW, DMS, and BCM to evaluate graceful degradation); compile initial draft of **FMEA-lite**; record high-resolution demonstration video footage. | Demonstration video + Continuous soak test logs + Experiment E9 results + Draft FMEA-lite |

**✅ End of Month 3 Achievements:** Complete 6-node distributed platform fully operational on test rig, vision-based DMS and cross-domain distributed features validated end-to-end, empirical multi-hour soak test logs collected.

---

### 📅 MONTH 4 — UDS Diagnostics, Evaluation, and Thesis Writing

| Week | Planned Tasks | Milestone Deliverable |
| :---- | :---- | :---- |
| **13** | Deploy full **UDS diagnostic stack** (Services 0x22, 0x19, optional 0x14) across all 4 FreeRTOS ECUs and UDS tester client on Master (`can-isotp` / Python); display active DTCs and WSW freeze frames on Qt HMI; develop Python analysis scripts for **cyclic jitter** and **Worst-Case Response-Time (WCRT)** analysis; run static analysis (`cppcheck`, `clang-tidy`); prepare test bench for **Experiment E1** (E2E on vs. off). | Operational UDS + DTC pipeline + Timing analysis scripts + Static analysis report |
| **14** | **Execute full experimental evaluation suite:** E1 (E2E on/off across 8 fault injection modes), E2 (bus utilization sweep at 2%, 30%, 60% comparing theoretical WCRT vs. empirical response times), E4 (safety-critical latencies), E5 (cyclic jitter and CPU load), E6 (Window PID/pinch performance), E7 (WSW confusion matrix and response times), E8 (DMS multi-lighting accuracy, false positives, FPS); finalize **Requirements Traceability Matrix**; re-verify complete fault injection suite. | Comprehensive empirical dataset + Completed Traceability Matrix + Final Fault Injection report |
| **15** | Draft thesis manuscript: Introduction, motivation, and FreeRTOS selection rationale; firmware architecture (layering, task model, Watchdog Manager); Linux Master (Yocto, Qt6, SocketCAN) and DMS ECU (Jetson, MediaPipe, CAN0); CAN bit timing harmonization across controllers; **E2E Protection & Fault Management Chapter**; **Real-Time Timing Analysis Chapter**; **Software Verification & Quality Chapter**; Driver Monitoring System Chapter; **Experimental Results & Evaluation (E1–E9)**; Scope limitations and future work; FMEA-lite Appendix; clean and organize repositories (firmware, master, dms) with professional READMEs. | Complete thesis manuscript draft + Clean repository codebase |
| **16** | Refine thesis text, prepare presentation slide deck, edit final high-definition demo video, conduct defense rehearsals; prepare live defense demonstration: **DMS detecting drowsiness triggering simultaneous buzzer + fan purge + HMI alert**, and **fault injection (corrupted CRC or disconnected ECU) demonstrating frame rejection, fail-safe degradation, and UDS DTC reporting on HMI**. | Final Thesis Manuscript + Defense Slide Deck + Demonstration Video + Examination Readiness |

**✅ Final Project Deliverables:**
- Fully functional **miniature automotive E/E platform**: 6 nodes communicating over 500 kbps CAN (Master MP157, DMS ECU Jetson, 4 FreeRTOS ECUs) conforming to a single DBC matrix.
- Structured, modular firmware adhering to AUTOSAR Classic layering principles, reusable BSW modules, host-based unit testing, built via CMake.
- ISO 14229 UDS diagnostics (Services 0x22, 0x19) with real-world DTC generation and RAM freeze-frame storage.
- **Automotive Quality Layer:** AUTOSAR E2E Profile 1, Watchdog Manager with Alive Supervision, timeout monitoring, bus-off recovery; automated fault injection and comparative analysis (E2E on vs. off); CAN response-time analysis across multiple bus load levels.
- **Distributed Cross-Domain Features with Fail-Safe Degradation** (speed-dependent auto-lock, door ajar warning, auto-window rollup, drowsiness response).
- Vision-based **Driver Monitoring System** powered by MediaPipe on Jetson, executing real-time cross-domain reactions with BCM and Climate ECUs.
- Comprehensive thesis manuscript with measurable objectives (G1–G4), empirical data from experiments E1–E9, and explicit discussion of engineering limitations.

---

## 5. Toolchains and Technologies

| Engineering Purpose | Tool / Technology Stack |
| :---- | :---- |
| **PCB Hardware Design** | KiCad EDA |
| **Microcontroller Firmware** | **FreeRTOS + STM32 HAL**, CMake, Ninja, `arm-none-eabi-gcc`, VS Code |
| **Hardware Debugging** | OpenOCD + Cortex-Debug via ST-Link SWD, logic analyzer, digital storage oscilloscope |
| **Master Host Platform** | OpenSTLinux (Yocto), ST X-LINUX-QT (Qt6), Qt Creator on host development machine, SocketCAN, `can-utils`, `can-isotp` |
| **DMS ECU Platform** | JetPack, Python 3, OpenCV, `mediapipe==0.10.14`, `numpy<2`, `python-can` |
| **CAN Bus Monitoring** | `candump`/`cansend`, USB-CAN candleLight adapter for sniffing and fault injection |
| **Version Control** | Git (structured repositories with FreeRTOS and STM32CubeF1 submodules) |
| **E2E Specification** | `can_matrix.dbc` + `e2e_reference.py` (authoritative test vector generator) |
| **Automated Testing** | Unity test framework + gcc executed on host PC (CTest); fault injection via `cansend` and `python-can` scripts |
| **Static & Timing Analysis** | `cppcheck`, `clang-tidy`; Python (`cantools`, `python-can`, `numpy`, `matplotlib`); FreeRTOS runtime statistics, ARM Cortex-M DWT cycle counter |
| **Driver Monitoring (AI)** | Python + OpenCV + MediaPipe Face Mesh (pretrained landmark model) |

---

## 6. Risk Management & Mitigation Strategies

| Identified Risk | Impact | Engineering Mitigation Strategy |
| :---- | :---- | :---- |
| **PCB delivery delays stall Month 2** | High | Submit Gerbers strictly by Week 3; develop and test firmware on raw Blue Pill development boards in parallel. |
| **Blue Pill hardware variations** (counterfeit chips, flash 64 vs 128 KB, pinout deviations) and reverse power hazards | High | Procure 6 units (+1 spare); execute Step 0 bring-up script reading Core ID and flash size; enforce `JP_5V` isolation before attaching USB; verify FT (5V tolerant) pins in datasheet prior to freezing schematics. |
| **Broad project scope** (PCB, 4 ECUs, Linux Master, Jetson AI, Quality Layer, 9 experiments) | High | Strictly prioritize "Core" requirements; treat "Nice-to-have" as droppable; orderly scope reduction sequence: complex HMI animations, head pose tracking, PERCLOS, multi-frame ISO-TP, theoretical response-time modeling, Cortex-M4 coprocessor, CAN OTA. |
| **FDCAN on DK1 requires device tree configuration** (only FDCAN1 on PA11/PD1 routed to header; potential "no clock found" errors) | Medium | Inspect device tree in Week 3; validate physical loopback with Blue Pill in Week 4. |
| **Qt6 setup on OpenSTLinux** | Medium | Utilize official ST X-LINUX-QT packages or established Yocto workflows; constrain HMI to a focused dashboard interface. |
| **Three heterogeneous CAN controllers** (bxCAN, FDCAN, mttcan): bit timing discrepancies | Medium | Enforce Classical CAN 500 kbps, 83.3% Sample Point globally; validate link incrementally in pairs; monitor `ip -details -statistics link show can0`. |
| **FreeRTOS lacks native ISO-TP, tests, and CAN drivers** | Medium | Finalize ISO-TP approach in Week 5; execute host-based tests via Unity; establish strict layered architecture from Day 1. |
| **CAN ID collisions, missing termination, GND loops** | Low | Freeze DBC in Week 2; enforce dual 120 $\Omega$ split terminations strictly at bus endpoints; route common ground across all nodes. |
| **Jetson CAN0 requires pinmux configuration and transceiver** | Medium | Verify CAN0 operation early in Week 9; maintain USB-CAN (`gs_usb`) as fallback. |
| **DMS sensitivity to illumination and camera perspective** | Medium | Evaluate multiple lighting conditions from Week 10; implement configurable EAR thresholds; provision optional IR LED illuminator. |
| **MediaPipe CPU execution bottlenecks** | Low | Baseline CPU inference measured at 43.0 FPS; re-measure under full integration; downscale resolution if end-to-end throughput drops below 15 FPS. |
| **Jitter of `DMS_STATUS` transmitted from Linux user space** | Low | Empirically measure jitter; establish relaxed timing tolerances specifically for the Linux/Python node if required. |
| **Thesis writing deferred to the final weeks** | High | Draft documentation chapters incrementally immediately following component completion. |
| **DMS instability near Week 10** | Medium | Fallback to simplified DMS pipeline (EAR only, dropping head pose and PERCLOS). |

---

## 7. Technical Specifications — Automotive Quality Layer

### 7.1. Concept and Motivation

Production automotive ECUs must do more than demonstrate basic functionality: they must **detect faults deterministically, transition into safe operational states, record diagnostic trouble codes, and substantiate dependability through quantitative empirical data and verification testing**. The Automotive Quality Layer realizes an **AUTOSAR Classic layered architecture (SWC, RTE, BSW Services: COM, E2E, DCM/DEM, WdgM, CanIf/IoHwAb, MCAL)** utilizing a **Spec-compliant Custom BSW** approach: rigorously implementing AUTOSAR E2E Profile 1, ISO 14229 UDS diagnostics, and multi-task Alive Supervision without relying on expensive commercial stacks. Detailed specifications reside in [Kien-truc-Firmware-AUTOSAR.md](Kien-truc-Firmware-AUTOSAR.md).

### 7.2. End-to-End (E2E) Communication Protection

| Parameter | Specification |
| :---- | :---- |
| **Protection Fields** | 4-bit `Alive_Counter` + 1-byte `CRC8` located at the final byte of **every** periodic message (10 frames in DBC v0.5). |
| **CRC Algorithm** | CRC-8/SAE-J1850 (polynomial `0x1D`, init `0xFF`, xorout `0xFF`) computed over `[DataID] + payload` excluding the CRC byte; unique `DataID` assigned per CAN ID. |
| **Receiver Validation** | Validates DLC, CRC checksum, and sequence counter continuity. Result classifications: INITIAL, OK, OK_SOME_LOST, REPEATED, WRONG_SEQUENCE, WRONG_CRC, WRONG_LENGTH. |
| **Debounce & Safe State** | 3 consecutive corrupted frames mark signal as INVALID, trigger DTC recording, and engage fail-safe degradation; 3 consecutive valid frames restore nominal operation. |
| **Reference Implementation** | Formally documented in `CAN-byte-layout.md` Section 4; Python implementation `e2e_reference.py` and C implementation verified with 100% parity across 450 test vectors. |

### 7.3. Fault Management and Fault Injection

| Mechanism | Description |
| :---- | :---- |
| **Watchdog Supervision** | **Independent Watchdog (IWDG) coupled with Watchdog Manager (WdgM) Alive Supervision:** Each critical task reports alive checkpoints; the supervisor task feeds the IWDG only if all checkpoints report within the period. If an individual task deadlocks while other tasks continue executing, the MCU triggers an automatic hardware reset (evaluated in Experiment E3). |
| **Reception Timeout** | Periodic frames: Absence of valid frames for 3 consecutive nominal periods flags signal as INVALID and records DTC 0xD00003. |
| **Bus-Off Recovery** | Automatic CAN peripheral re-initialization and recovery upon entering bus-off state; records DTC 0xD00001. |
| **Diagnostic Trouble Codes** | Onboard DEM storing DTC records accessible via UDS Service 0x19; WSW captures snapshot freeze frames (DID 0x0120). |
| **Fail-Safe Safe States** | Formally specified per ECU/frame in `CAN-byte-layout.md` Section 4 and degradation matrix in Section 7.9. |

**Fault Injection Matrix:**

| Test Case | Injected Fault Mode | Injection Methodology | Expected System Response |
| :----: | :-- | :-- | :-- |
| **1** | Corrupted CRC Checksum | Transmit frame with inverted CRC byte using `cansend`. | Drop frame; log DTC 0xD00002 after 3 consecutive corruptions. |
| **2** | Frame Replay Attack | Transmit duplicate frames with frozen `Alive_Counter`. | Flagged as REPEATED; frame dropped. |
| **3** | Counter Sequence Jump | Transmit frames with counter skipping +2, +3, then $\ge 4$. | Flagged as OK_SOME_LOST then WRONG_SEQUENCE; frame dropped. |
| **4** | Cyclic Frame Timeout | Physically disconnect target ECU or terminate DMS process. | After 3 missed periods: declare INVALID, display "N/A" on HMI, log DTC 0xD00003. |
| **5** | Sensor Hardware Disconnection | Disconnect DHT sensor signal wire from Climate ECU. | Assert `DHT_Fault` = 1; fan runs at 50% safe speed; log DTC 0xB20001. |
| **6** | Firmware Task Deadlock | Inject deliberate infinite loop into a test task build. | Alive supervision detects missing checkpoint; IWDG triggers MCU hardware reset. |
| **7** | Forced Bus-Off Condition | Connect development node transmitting at mismatched bitrate. | CAN controller recovers automatically; logs DTC 0xD00001. |
| **8** | Out-of-Range Command | Transmit `WINDOW_CMD` requesting an invalid 200° angle. | Reject command; window holds position; logs DTC 0xB30001. |

### 7.4. Real-Time Timing Analysis

| Measurement Metric | Methodology | Tooling |
| :---- | :---- | :---- |
| **Cyclic Jitter (p50, p99, max)** | Compute delta timestamps between consecutive frames of identical CAN ID. | `candump -L`, Python analysis script |
| **CPU Utilization & Stack Watermark** | Profile target execution under peak operating load. | FreeRTOS runtime stats (`configGENERATE_RUN_TIME_STATS`), `uxTaskGetStackHighWaterMark`, DWT cycle counter |
| **Event-to-Bus Propagation Latency** | Toggle GPIO upon physical event; capture concurrently with CAN differential signal. | Multi-channel logic analyzer (common reference clock) |
| **Worst-Case Response-Time (WCRT)** | Calculate theoretical bounds from DBC (ID, DLC, period); correlate with empirical latencies **at bus loads of 2%, 30%, 60%**. | Python analysis script + background load generator (`cangen`) |

**CAN Worst-Case Response-Time Formulation (Davis et al., based on Tindell):**  
For frame $m$: $R_m = J_m + w_m + C_m$, where $w_m$ is the iterative solution to:
$$w = B_m + \sum_{k \in hp(m)} \left\lceil \frac{w + J_k + \tau_{bit}}{T_k} \right\rceil \cdot C_k$$
where $B_m$ represents blocking latency from the longest lower-priority frame, and $C_m$ denotes transmission duration under worst-case bit stuffing. Aperiodic event messages are modeled using minimum inter-arrival times. Because nominal bus utilization is ~2%, **synthetic background traffic injection is required** to provide meaningful empirical validation.

### 7.5. Software Verification & Quality Assurance

| Verification Item | Specification |
| :---- | :---- |
| **Host Unit Testing** | Unity framework + gcc running on host PC via CTest targeting pure C modules: CRC-8, E2E state machine, wheel slip calculations, Climate hysteresis, Window PID, DTC manager. |
| **Static Code Analysis** | Automated scanning via `cppcheck` and `clang-tidy`; zero high-severity issues permitted. |
| **Coding Standards** | Aligned with MISRA C guidelines: dynamic memory allocation prohibited, recursion prohibited, return values checked; no formal certification claimed. |
| **Requirements Traceability** | Bidirectional Traceability Matrix mapping every Functional Requirement (FR) to corresponding software modules and test cases (target: 100% coverage). |

### 7.6. Safety Analysis: FMEA-lite

Targeted analysis applied specifically to the two safety-critical functions. Formally documented as an engineering deliverable:

| Safety Function | Failure Mode | System Impact | Detection Mechanism | Fail-Safe Mitigation |
| :---- | :---- | :---- | :---- | :---- |
| **`WSW_WARNING`** | Optical encoder fails to generate pulses while motor is driven. | Inability to compute wheel velocity; slip warning inoperative. | Monitor pulse absence over time window. | Assert DTC 0xC10001; illuminate physical warning lamp. |
| **`WSW_WARNING`** | Miscalibrated wheel slip threshold. | Nuisance false alarms or missed detection under braking. | Correlate empirical braking test logs. | Configurable threshold (20% nominal); AND-fusion with deceleration criterion. |
| **`WHEEL_SPEED`** | Low-speed quantization noise at low pulse frequencies. | Erratic velocity estimates causing false slip triggers. | Period measurement between pulses. | Measurements below 100 rpm logged without hard deadline guarantees. |
| **`WSW_WARNING`** | Electrical bit corruption on CAN bus. | Master ingests corrupted slip warning data. | E2E CRC-8 and Alive Counter check. | Discard corrupted frame; record DTC 0xD00002. |
| **`WHEEL_SPEED`** | Total packet loss or node disconnection. | BCM loses vehicle speed input for auto-locking and door ajar logic. | Reception timeout (60 ms). | Inhibit auto-locking; suppress acoustic alarms; record DTC 0xD00003. |
| **WSW ECU** | Microcontroller firmware deadlock. | Bus silent; slip warning inoperative. | Multi-task Alive Supervision; Master/BCM timeout. | IWDG resets MCU; Master flags status as "N/A". |
| **`DMS_ALERT`** | Camera obstructed or detached. | Failure to alert driver during actual drowsiness. | `Alert_Type = No_Face`, `Camera_OK = 0`. | Transmit `No_Face`; trigger mild acoustic chirp; record DTC 0xE00001. |
| **`DMS_ALERT`** | False alarms caused by adverse lighting. | Nuisance buzzer activation. | Measure false positive rate. | Configurable EAR thresholds; optional IR illumination. |
| **`DMS_ALERT`** | DMS ECU hangs during an active alarm. | Buzzer sounds continuously. | BCM 500 ms reception timeout. | BCM deactivates buzzer automatically. |
| **`DMS_ALERT`** | Corrupted CAN alert frame. | Spurious buzzer or fan override. | E2E CRC-8 and counter check. | Discard corrupted frame; record DTC 0xD00002. |

### 7.7. Stretch Goals (Optional, Post-Core Completion)

Custom CAN bootloader for Over-The-Air (OTA) firmware flashing; leveraging the STM32MP157 Cortex-M4 core for a secondary real-time coprocessor application. Excluded from the 16-week baseline schedule.

### 7.8. Presentation of Empirical Results

Final deliverables include: Fault Injection Matrix (Pass/Fail table), comparative E2E enabled vs. disabled metrics, cyclic jitter histograms, theoretical vs. empirical WCRT response-time plots under load sweeps, continuous endurance soak test report, Traceability Matrix, and FMEA-lite report. Live examination demonstration: Injecting corrupted CRC frames or disconnecting a node to prove frame rejection, fail-safe degradation, and UDS DTC reporting on the HMI.

### 7.9. Distributed Cross-Domain Functions & Degradation Matrix

| Distributed Function | Signal Origin | Executing Node | Fail-Safe Behavior Upon Corrupted / Lost Signal |
| :-- | :-- | :-- | :-- |
| **Speed-Dependent Auto-Lock** | `WHEEL_SPEED` (WSW ECU) | BCM ECU | Inhibit auto-locking; retain manual lock control; log DTC. |
| **Ajar Door Warning in Motion** | `WHEEL_SPEED` + `Door_Open` | BCM ECU | Suppress false acoustic alarms; log DTC. |
| **3-Level Drowsiness Reaction** | `DMS_ALERT` (DMS ECU) | BCM (buzzer), Climate (100% fan for 30 s), Master (HMI) | BCM deactivates buzzer after 500 ms without valid frames; Climate cancels fan override. |
| **Auto-Window Rollup on Lock** | `Lock_State` (BCM ECU) | Window ECU | Inhibit auto-closure; freeze current window position. |

### 7.10. Experimental Evaluation Plan Summary

| Experiment ID | Evaluation Focus | Comparative / Controlled Independent Variable |
| :----: | :-- | :-- |
| **E1** | E2E Protection Effectiveness | **E2E Enabled vs. Disabled** across all 8 fault injection modes; quantify undetected corruptions. |
| **E2** | CAN Bus Response-Time Analysis | **Background bus load sweep: 2%, 30%, 60%**; correlate theoretical WCRT with empirical measurements. |
| **E3** | Watchdog Alive Supervision | **Simple IWDG vs. Multi-task Alive Supervision** when an individual non-watchdog task deadlocks. |
| **E4** | Critical Safety Path Latencies | Event generation to bus transmission: WSW to `WSW_WARNING`; DMS to buzzer sound (common clock). |
| **E5** | Cyclic Jitter & Resource Utilization | 4-hour endurance soak test: p99 jitter, per-task CPU %, and stack high-water marks. |
| **E6** | Power Window Control & Anti-Pinch | Step response (30°, 90°, 150°): overshoot, settling time, anti-pinch reversal latency. |
| **E7** | Wheel Slip Warning Algorithm | Slip, deceleration, AND, OR evaluation; classification confusion matrix under braking. |
| **E8** | Driver Monitoring System | End-to-end FPS, detection accuracy, false positive rate across 3 distinct lighting environments. |
| **E9** | Distributed Graceful Degradation | Signal cutoff testing: disconnect WSW, DMS, and BCM to validate fail-safe degradation states. |

---

## 8. Technical Specifications — Driver Monitoring System (DMS)

### 8.1. Architecture & Concept

A driver-facing camera connected to the **Jetson Orin Nano (acting as the DMS ECU)** continuously extracts facial landmarks using **MediaPipe Face Mesh** (pretrained, no custom training required) to evaluate drowsiness and distraction metrics in real time. Upon threshold violation, the DMS broadcasts `DMS_ALERT` directly over native CAN0 to trigger acoustic alarms on the BCM ECU and ventilation overrides on the Climate ECU; concurrently, it broadcasts `DMS_STATUS` cyclically every 100 ms as a telemetry heartbeat for the Master host.

### 8.2. Calculated Facial Metrics

| Metric | Engineering Significance |
| :-- | :-- |
| **Eye Aspect Ratio (EAR)** | Quantifies prolonged eye closure indicating microsleep and driver fatigue. |
| **Head Pose (Yaw, Pitch)** | Detects prolonged head deflection or downward tilt indicating cognitive distraction. |
| **PERCLOS (Nice-to-have)** | Percentage of eye closure over a 60-second sliding window; industry standard fatigue metric. |
| **EAR Auto-Calibration (Nice-to-have)** | Establishes individualized EAR baselines during the initial 10 seconds of operation to minimize false positives. |
| **Blink Frequency** | Auxiliary metric differentiating involuntary blinks from fatigue-induced closure. |

### 8.3. Vision Processing Pipeline

Camera Frame Capture $\rightarrow$ MediaPipe Face Mesh Extraction $\rightarrow$ EAR & Head Pose Computation $\rightarrow$ Temporal Window Tracking $\rightarrow$ 3-Level Drowsiness Escalation (Low > 1.0 s, High > 2.0 s, Critical > 4.0 s; head deflection > 3.0 s) $\rightarrow$ Broadcast `DMS_ALERT` over native CAN0 with E2E protection $\rightarrow$ BCM sounds buzzer, Climate ramps fan to 100%, Master visualizes alert. If camera feed drops, broadcast `No_Face` with `Camera_OK = 0` and log DTC 0xE00001.

### 8.4. Verification Methodology

- Controlled testing across two behavioral states (alert, simulated drowsiness via prolonged eye closure) evaluated under three distinct ambient lighting conditions.
- Record classification accuracy and false positive rates to construct the performance matrix (Experiment E8).

### 8.5. Performance Metrics Reporting

End-to-end FPS throughput (explicitly documenting CPU execution; standalone baseline measured at 43.0 FPS on CPU with `mediapipe==0.10.14`), detection-to-alert latency, accuracy and false positive tables across lighting conditions, and live demonstration video.

---

## 9. Thesis Positioning Statement

The thesis is positioned as a **rigorously evaluated, miniature automotive E/E platform**, rather than an ad-hoc collection of microcontroller circuits. The narrative maintains consistent structural alignment:

1. **Homogeneous Microcontroller Hardware:** 4 identical carrier PCBs sharing a unified schematic, differentiated solely by software.
2. **Industry-Standard RTOS:** FreeRTOS + STM32 HAL, modular layered architecture, shared BSW libraries, host-based unit testing.
3. **CAN Bus Backbone:** Unified DBC matrix, AUTOSAR E2E protection across all frames, harmonized bit timing across three controller architectures.
4. **Heterogeneous Multi-Domain Architecture:** Linux Master (HMI, UDS tester), Edge AI DMS ECU (Jetson), and 4 Real-Time Microcontroller ECUs; safety-critical alerting operates independently of the Linux operating system.
5. **Automotive Quality & Empirical Validation:** E2E protection, fault management, timing response analysis, verification testing, FMEA-lite, and controlled comparative experiments (E1–E9).

**Explicit Thesis Limitations:** Cryptographic encryption is omitted on CAN; power window actuation is simulated in software; the DMS utilizes a pretrained model; snapshot freeze frames are buffered in volatile RAM; firmware implements an AUTOSAR Classic layered architecture via a Spec-compliant Custom BSW approach without commercial ISO 26262 or Full-Stack AUTOSAR certification (due to 64 KB Flash / 20 KB RAM constraints); the platform is an experimental tabletop prototype.

---

## 10. Engineering Career and Portfolio Alignment

By demonstrating proficiency across **FreeRTOS + CAN + UDS Diagnostics + Linux Master (Yocto, Qt) + Jetson AI + Automotive Quality Mechanisms**, paired with prior background in Zephyr (Cortex-M4) and Yocto (Cortex-A7), the thesis establishes credentials across both MCU RTOS and Embedded Linux domains:

- **Automotive Embedded Software Engineer** (CAN networking, ISO 14229 UDS, RTOS, AUTOSAR E2E, fault handling): Primary qualification.
- **Embedded Software Engineer** (FreeRTOS, Embedded C, STM32 HAL, host-based unit testing, static code analysis).
- **Embedded Linux / Edge AI Engineer** (Yocto Project, Qt6, SocketCAN, Jetson platform): Substantial complementary strength.

---

## 11. Key Performance Indicators (KPIs) for Reporting, Defense, and Portfolio

Empirical metrics to be captured and logged starting from Month 2:

| Ref | Key Performance Indicator | System Attribute Demonstrated | Measurement Methodology | Tooling Stack |
| :----: | :---- | :---- | :---- | :---- |
| **1** | CAN bus utilization (%) under 6-node load | Bus bandwidth margin and headroom | Bit counter script across 1.0 s windows divided by 500 kbps | `candump` + Python script |
| **2** | End-to-end CAN transmission latency (ms) | Real-time deterministic responsiveness | Transmitter event GPIO timestamp vs. receiver ingestion timestamp (common clock) | Multi-channel logic analyzer |
| **3** | Wheel slip warning reaction time (ms), target < 100 ms from 100 rpm | Real-time safety response deadline | Elapsed time from slip threshold breach to `WSW_WARNING` bus transmission | Logic analyzer, CAN logs |
| **4** | UDS diagnostic round-trip latency (ms) | Diagnostic responsiveness | Time delta between Service 0x22/0x19 request and response frame | Python script over SocketCAN |
| **5** | E2E error detection rate (E2E on vs. off) and latency to DTC | Communication integrity mechanism efficacy | Controlled Fault Injection (Experiment E1) | `cansend`, Python, UDS 0x19 |
| **6** | Cyclic transmission jitter (p50, p99, max) | Deterministic task scheduling | Delta timestamps between consecutive frames of identical ID | `candump -L`, `numpy` |
| **7** | Worst-case response time: theoretical vs. empirical under 2%, 30%, 60% load | Bus scheduling validation | Sweep background traffic injection (Experiment E2) | Python script + `cangen` |
| **8** | CPU utilization (%) and peak stack usage per task | Microcontroller resource headroom | FreeRTOS runtime stats, `uxTaskGetStackHighWaterMark` | FreeRTOS runtime profiling |
| **9** | Endurance soak test: hours, dropped packets, CRC errors, resets | Long-term operational stability | Continuous $\ge$ 4-hour execution log | Master Linux logs, UDS 0x19 |
| **10** | Test pass rate (%) and functional requirement test coverage (%) | Software engineering quality | Traceability Matrix and CTest execution reports | CTest, Traceability Matrix |
| **11** | Alive Supervision vs. simple hardware IWDG | Fault containment efficacy | Deadlock injection in non-watchdog task (Experiment E3) | Logic analyzer, serial logs |
| **12** | Window: overshoot %, settling time, anti-pinch reaction latency | Control performance and safety deadline | Closed-loop step command logging (Experiment E6) | Internal logs + CAN sniffer |
| **13** | WSW: alert classification confusion matrix (slip, decel, AND, OR) | Suppression of false alarms | Empirical braking test runs (Experiment E7) | Statistical evaluation table |
| **14** | DMS: End-to-end pipeline FPS | Real-time vision processing capability | Frame counter logging | Python script instrumentation |
| **15** | DMS: Detection-to-alert latency (ms) | Driver fatigue alert timeliness | Timestamp delta from condition breach to `DMS_ALERT` dispatch | Timestamped logs |
| **16** | DMS: Accuracy and false positive rate under 3 lighting conditions | Environmental robustness | Multi-lighting trials (Experiment E8) | Statistical classification table |
| **17** | Distributed functions: safe degradation conformance | Fail-safe fault handling compliance | Signal disconnection injection (Experiment E9) | Pass/Fail compliance table |

### Target Acceptance Criteria Summary

| Target Attribute | Specified Engineering Threshold | Engineering Notes |
| :---- | :---- | :---- |
| **Nominal CAN Bus Utilization** | < 10% | Projected at ~2% - 3%; empirical measurement reported. |
| **Wheel Slip Warning Reaction Time** | < 100 ms at speeds > 100 rpm | Speed derived via period between encoder pulses. |
| **UDS Round-Trip Latency** | < 100 ms | Handled event-driven within communication task. |
| **Cyclic Frame Jitter** | Within ±10% | `configTICK_RATE_HZ` = 1000; DMS ECU (Python) evaluated separately. |
| **DMS Pipeline Throughput** | Minimum 15 FPS, Target 25 FPS | Standalone CPU inference baseline: 43.0 FPS on CPU. |
| **DMS Eye Closure / Head Deflection** | 1.0 s / 2.0 s / 4.0 s (3 levels) / 3.0 s | Configurable EAR and angle thresholds. |
| **Endurance Soak Test** | Minimum 4.0 hours | Zero unexpected resets; zero nominal CRC errors. |
| **WSW Wheel Slip Thresholds** | 20% entry, 10% exit, 2-sample confirmation | Empirically calibrated in Week 7. |
| **Power Window Control** | Overshoot ≤ 5%, settling time ≤ 2.0 s (90° step), anti-pinch < 100 ms | Empirically calibrated parameters. |
| **E2E Protection Deadlines** | Timeout 3× period, 3 consecutive errors, MAX_DELTA = 3 | Conforms to AUTOSAR E2E Profile 1 specifications. |

**Instrumentation Notice:** Incorporate high-resolution timestamp logging directly into the firmware codebase during Month 2 development (specifically for KPIs #2, #3, and #6); retrofitting timing measurement hooks into completed firmware causes unnecessary rework.
