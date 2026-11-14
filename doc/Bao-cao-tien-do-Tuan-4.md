# PROJECT PROGRESS REPORT — WEEK 4

**Thesis Title:** Design and Evaluation of a Miniature Automotive E/E Platform over CAN with End-to-End Protection and Fault Management  
**Student / Researcher:** [Student Name]  
**Execution Period:** Week 4 (Month 1)  
**Completion Date:** October 31, 2026  

---

## 1. Week 4 Objectives (According to Plan v3)

In accordance with [Ke-hoach-do-an-4-thang-v3.md](file:///c:/Study/Thesis_BEng/doc/Ke-hoach-do-an-4-thang-v3.md#L97), while awaiting the manufacturing and fabrication of the 4-ECU Carrier Board PCB at the factory, the primary objectives for Week 4 included:
1. Fully establish the microcontroller firmware development environment (Arm GNU Toolchain, CMake, Ninja, FreeRTOS-Kernel, STM32 HAL, VS Code).
2. Construct and execute the Host Unit Test suite (Unity + CTest) for the E2E CRC-8 algorithm on the host computer prior to target deployment.
3. Successfully bring up a standalone FreeRTOS sample application on the STM32F103 (Blue Pill) board in CAN Loopback and Normal modes.
4. Resolve the Device Tree configuration and SCMI secure clock controller permissions on the Linux Master host (**STM32MP157D-DK1**) to activate **FDCAN1 (SocketCAN `can0`)**.
5. Connect the physical 500 kbps CAN bus across nodes (Blue Pill ↔ DK1) and validate bidirectional **End-to-End (E2E) Protection** comprising a 4-bit `Alive_Counter` and `CRC-8/SAE-J1850`.

---

## 2. Completed Work & Results

### 2.1. Development Environment & Automated Host Testing
- **Firmware Toolchain:** Successfully established the `arm-none-eabi-gcc` cross-compilation toolchain integrated with `CMake Presets` and `Ninja` targeting the STM32F103C8T6 (Cortex-M3).
- **Embedded Software Stack:** Successfully integrated `FreeRTOS-Kernel (V11.2.0)` configured for 100% static allocation (`configSUPPORT_DYNAMIC_ALLOCATION 0`), synchronized SysTick interrupt for the RTOS scheduler, and dedicated `TIM2` as the timebase for the STM32 HAL.
- **Unit Testing (Unity + CTest):** Automated execution of CRC-8/SAE-J1850 test vectors (polynomial `0x1D`, init `0xFF`, xorout `0xFF`), achieving 100% parity with the Python reference implementation `e2e_reference.py`.

### 2.2. Resolving FDCAN1 on the Master STM32MP157D-DK1 (Linux 6.6)
During the deployment of SocketCAN on OpenSTLinux v5.0.17 (kernel 6.6-yocto-scarthgap), two low-level hardware configuration issues were diagnosed and resolved:
1. **Missing FDCAN1 Node in Device Tree:** Since the Discovery kit does not feature an onboard CAN transceiver, the `can@4400e000` node is set to `disabled` by default. The team decompiled `stm32mp157d-dk1.dtb` into `.dts`, appended pin control `m-can1-1` (phandle `<0xd6>` mapped to pin pair `PD1_TX` [CN13 Pin 9 / D14 SDA] and `PA11_RX` [CN13 Pin 10 / D15 SCL] on Arduino header CN13), and updated its status to `okay`.
2. **Fixing `"m_can_platform 4400e000.can: no clock found"`:** Under modern OpenSTLinux architectures utilizing OP-TEE, the reference clock `CK_HSE` is isolated in the secure world and managed by the SCMI Clock Controller (`protocol@14` phandle `<0x04>`). The team updated the device tree clock binding:
   ```dts
   clocks = <0x04 0x00 0x0b 0x9d>;
   clock-names = "hclk\0cclk";
   ```
   Result: Interface **`can0`** successfully enumerated in the Linux OS and operates in standard SocketCAN mode.

### 2.3. Hardware Bring-up & Inter-Node 500 kbps CAN Validation
- **Physical Hardware Setup:**
  - Node 1: STM32F103 (Blue Pill) + SN65HVD230 transceiver module (3.3V).
  - Node 2: STM32MP157D-DK1 (CN13 Pin 9 SDA / Pin 10 SCL) + SN65HVD230 transceiver module.
  - Bus: Differential lines `CAN_H` and `CAN_L`, common `GND`, and $60\,\Omega$ equivalent termination ($2 \times 120\,\Omega$).
- **Bit Timing Harmonization:** 500 kbps nominal bitrate with 83.3% Sample Point configured identically across two distinct controllers (bxCAN on F103 and Bosch M_CAN/FDCAN1 on MP157).
- **Quantitative E2E Protection Verification:**
  - The STM32F103 node periodically broadcasts CAN frame ID `0x6F0` at 100 ms intervals (10 frames/s) protected with a 4-bit `Alive_Counter` and 1-byte `CRC-8`.
  - The DK1 node runs a SocketCAN reception script that decodes, parses, and monitors packet integrity in real time.
- **Hardware Bring-up Video Demonstration:**
  - Video Demonstration Link (Google Drive): [CAN E2E Hardware Bring-up Video](https://drive.google.com/file/d/1pLO-NHE3jXmwbYgq4MFZFooruoL_e69R/view?usp=sharing)  
    *(Demonstrates live frame transmission, zero-packet-loss reception on the Linux SocketCAN console, and oscilloscope/logic analyzer signal verification).*

---

## 3. Empirical Acceptance & Validation Data Table

Metrics recorded concurrently via Serial UART Monitor (COM4 - F103) and Linux Serial Console (COM5 - DK1):

| Evaluation Metric | STM32F103 Node (Blue Pill - COM4) | STM32MP157D-DK1 Master (COM5) | Technical Conclusion |
| :--- | :---: | :---: | :---: |
| **Transmitted / Received Frames** | `TX ok = 638` | `ok = 638` | **100% match (638/638 frames)**, zero packet loss |
| **Actual Transmission Rate** | 10.0 frames/second | 10.01 frames/second | Strictly adheres to 100 ms cyclic period |
| **Data Integrity Errors (CRC Error)** | — | `crc_err = 0` | Hardware CRC-8 matches Python reference 100% |
| **Sequence Counter Errors (Alive Error)** | — | `alive_err = 0` | No dropped frames, no sequence jumps |
| **Foreign / Rogue Frames** | — | `other = 0` | Clean bus with zero electrical noise or bit errors |
| **Microcontroller Error Counters** | `TEC = 0, REC = 0` | `state ERROR-ACTIVE` | Bus operating in optimal state (Error Active) |
| **Bus-Off Events** | **0 (No Bus-Off)** | **0 (No Bus-Off)** | Completely resolved initialization ACK absence issue |

---

## 4. Technical Troubleshooting Log

1. **Persistent `BUS-OFF` and Monotonic `TX fail` Counter Increments on Blue Pill:**
   - *Root Cause:* When operating standalone prior to DK1 connection or before `can0` interface activation, no receiving node asserted a dominant bit in the ACK slot; simultaneously, a floating RX pin rapidly drove `TEC > 255`.
   - *Resolution:* Utilized the `debug-loopback` preset to verify internal firmware execution, then connected the physical bus and executed `ip link set can0 up` on the DK1 to provide hardware ACKs.
2. **Interface `can0` Missing on Linux:**
   - *Root Cause:* Default device tree for STM32MP157D-DK1 left `m_can1` in a `disabled` state.
   - *Resolution:* Decompiled `/boot/stm32mp157d-dk1.dtb` using `dtc`, reconfigured pinctrl `m-can1-1`, and updated `status = "okay"`.
3. **Error `no clock found` During `m_can_platform` Driver Probe:**
   - *Root Cause:* OpenSTLinux 5.0+ routes `CK_HSE` clock management through OP-TEE SCMI (`protocol@14`, phandle `<0x04>`), whereas older device tree bindings referenced the `rcc` controller (`0x0b`) directly.
   - *Resolution:* Updated the CAN node clock property to `<0x04 0x00 0x0b 0x9d>`.
4. **`IndentationError` when Transferring Python Scripts via Serial Terminal:**
   - *Root Cause:* UART serial receive buffer dropped leading whitespace characters during high-throughput text streaming.
   - *Resolution:* Encoded the script as a Base64 string and decoded directly on the board via `base64 -d`.

---

## 5. Conclusions & Week 5 Plan

- **Overall Assessment:** Completed **100% of Week 4 scope** strictly on schedule and satisfied all project acceptance criteria. The firmware toolchain and 500 kbps E2E-protected CAN communication link between microcontroller and Linux embedded host have been verified stable.
- **Week 5 Plan:**
  1. Receive the batch of 4 fabricated PCB Carrier Boards from the manufacturer; perform visual inspection, power rail validation, short-circuit checks, and component soldering.
  2. Bring up the Carrier Board hardware (LM2596 12V $\rightarrow$ 5V Buck regulator, Blue Pill socket headers, SWD header, integrated CAN transceiver).
  3. Refactor firmware into an AUTOSAR Classic Layered Architecture: multi-target CMake under `apps/` (`ecu_bcm`, `ecu_window`, `ecu_climate`, `ecu_wsw`), Virtual Function Bus `rte/`, and BSW modules under `common/` (`can_driver`, `can_com`, `e2e_crc8`, `diag_dcm`, `diag_dem`, `wdgm`).
  4. Implement application SWC logic for the first two nodes (BCM ECU and Window ECU).
