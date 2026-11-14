# Requirement Gathering — Common PCB for 4 ECUs (v0.2, Option A: Blue Pill Carrier Board)

This carrier board design is shared across all 4 ECUs: **BCM, Window, Climate, and WSW**. All four boards are physically identical; each ECU differs only in its flashed firmware and the external peripheral modules plugged into it. Each ECU node comprises **one custom-designed carrier board + one Blue Pill module (STM32F103C8T6)** mounted on socket headers. Figures marked **(proposed)** represent baseline engineering assumptions subject to bench validation and measurement.

Source requirements: `System-Requirements-ECU.md` (Section 2 per-ECU requirements, Section 3B bit timing), `can_matrix.dbc`, `BOM-ECU-Project.xlsx`.

---

## 0. Key Architectural Decisions

| No. | Decision | Status | Description |
| :-- | :-- | :-- | :-- |
| D1 | MCU mounting scheme | **Finalized** | Carrier board with socketed Blue Pill (no direct LQFP48 soldering on carrier). Rationale: Custom PCB manufacturing poses schedule risk; manual LQFP48 assembly carries high defect risk; primary thesis focus is automotive firmware architecture and quality verification. |
| D2 | Assembly methodology | **Finalized** | Manual soldering with standard iron: SMD passives use **0805** footprints or through-hole components; the sole SMD IC on the carrier is the SN65HVD230 CAN transceiver in SOIC-8. |
| D3 | 5 V power regulation | **Finalized** (default) | LM2596 buck converter module mounted via a **2.54 mm breakout grid** + short jumper leads (avoids locking the layout to a single vendor's physical module dimension). |
| D4 | Connector standards | **Finalized** (default) | 2.54 mm pin headers for sensor and actuator breakout; 5.08 mm screw terminal blocks for 12 V power and CAN bus connections. |
| D5 | Board dimensions | **Finalized** (default) | Maximum dimensions: 100 x 80 mm; four corner M3 mounting holes. |
| D6 | Node identification (ECU_ID) | **Finalized** (default) | Hardware-selectable via 2 solder jumpers (PA4, PA5); firmware reads pins at boot and outputs identity to UART log. |

---

## 1. Scope and Design Principles

- **Unified Hardware Design:** A single PCB design serves all 4 ECUs. **No assembly variants**: every board is populated with the complete carrier circuitry; each ECU node simply interfaces with the specific peripherals it requires.
- **Functional Allocation:**
  - *Carrier Board:* 12 V input protection, 5 V buck regulator, CAN physical layer (transceiver, 120 Ω termination jumper, ESD protection), I/O level shifting / conditioning, field connectors, ECU ID strapping, and test points.
  - *Blue Pill Module:* STM32F103 MCU, 8 MHz HSE crystal, reset circuit, BOOT configuration, and onboard 3.3 V LDO regulator.
- **Logic Levels:** 3.3 V system logic level throughout. Analog sensors must be powered exclusively from 3.3 V (STM32 ADC input pins are not 5 V tolerant).
- **Prototyping Priority:** Hand-solderability, signal probing access, and ease of hardware debugging take priority over ultra-compact routing density.
- **Pin Mapping Constraint:** **All allocated MCU pins must map to standard Blue Pill headers**, enabling firmware development to begin immediately on breadboards and transition seamlessly to the carrier board without code modification.

---

## 2. Functional Requirements

| ID | Priority | Requirement Description |
| :-- | :-- | :-- |
| PCB-F01 | Core | Blue Pill socket interface: **Two 1x20 female pin header rows**, 2.54 mm pitch, row spacing ~22.86 mm (0.9 in) (verify physical module sample before layout freeze). |
| PCB-F02 | Core | 1 CAN port (bxCAN) utilizing an onboard **SN65HVD230** 3.3 V transceiver, configured for 500 kbps operation. |
| PCB-F03 | Core | Unobstructed mechanical access to the 4-pin SWD programming header on the Blue Pill edge for ST-Link ribbon connection. |
| PCB-F04 | Core | Dedicated 3-pin header breakout for USART1 UART serial debugging console. |
| PCB-F05 | Core | Carrier-mounted status indicators: Fault LED (PA2) and User Pushbutton (PA3); MCU reset, BOOT jumpers, and PC13 indicator remain on the Blue Pill module. |
| PCB-F06 | Core | Power input from 12 V DC; onboard buck step-down provides 5 V to Blue Pill 5V pin; 3.3 V logic rail is sourced from Blue Pill onboard 3V3 output pin. |
| PCB-F07 | Core | Comprehensive I/O peripheral capacity supporting all 4 ECUs (Section 4): 4 DI, 4 DO, 2 ADC, 4 PWM (2 fast, 2 slow), 1 timer capture, 1 DHT 1-wire pin. |
| PCB-F08 | Recommended | Two solder jumpers for ECU_ID selection to allow firmware auto-detection of board identity. |
| PCB-F09 | Recommended | Auxiliary expansion header (SPI1 on PA4-PA7 plus spare GPIOs) for hardware testing and expansion. |

---

## 3. Power Supply Requirements

**Power Tree Architecture:** 12 V external input passes through a series reverse-polarity Schottky diode, resettable polyfuse, and transient TVS diode, feeding the LM2596 step-down buck module. The regulated 5 V output routes through isolation jumper `JP_5V` into the Blue Pill 5V rail. The Blue Pill onboard LDO steps 5 V down to 3.3 V to power the Cortex-M3 core, the CAN transceiver, and low-power sensor headers.

| ID | Requirement | Parameter / Implementation |
| :-- | :-- | :-- |
| PCB-P01 | Input voltage range (proposed) | 9 V to 15 V DC (nominal 12 V battery/bench supply). |
| PCB-P02 | Reverse polarity protection | Series Schottky barrier diode (SS34 per BOM, SMA package, hand-solderable); forward voltage drop ~0.3-0.5 V accepted. |
| PCB-P03 | Overcurrent protection | Resettable PTC polyfuse; hold current finalized after load bench testing (BOM baseline 2 A); through-hole or 1812/2920 footprint. |
| PCB-P04 | Voltage surge suppression (recommended) | 15 V to 18 V unidirectional TVS diode installed across the 12 V input terminal. |
| PCB-P05 | 5 V buck regulation | LM2596 buck converter module (>=3 A rated current), pre-calibrated to 5.0 V with a multimeter **prior to** inserting Blue Pill; >=470 µF filter capacitor on carrier 5 V output rail. |
| PCB-P06 | 3.3 V logic rail | Sourced directly from Blue Pill onboard LDO. Total external 3.3 V draw (transceiver, LDR divider, DHT, speed encoder) should remain under ~200 mA (proposed); higher-draw sensors must use 5 V. |
| PCB-P07 | **Isolation Jumper JP_5V** | Series jumper on the 5 V feed to the Blue Pill. **Never connect the Blue Pill micro-USB cable while carrier 12 V power is applied** (inexpensive Blue Pill modules connect USB VBUS directly to the 5V header without back-feed protection diode): disconnect `JP_5V` before connecting USB. |
| PCB-P08 | High-power 12 V loads (fan, door solenoid) | **Do not route through carrier board copper traces**: supply directly from 12 V terminal block via external relay contacts. |
| PCB-P09 | Grounding architecture | Continuous, low-impedance ground plane; CAN bus ground reference tied together across all ECU nodes. |

**Estimated Power Consumption Budget (subject to physical bench validation):**

| Rail | Estimated Current | Peripheral Load Details |
| :-- | :-- | :-- |
| 3.3 V Rail | ~60 to 120 mA | STM32F103 MCU, SN65HVD230 transceiver, LDR divider, DHT22, optical encoder. |
| 5 V, BCM Node | ~250 mA | Dual 5 V relay coils, active buzzer, PIR motion sensor. |
| 5 V, Climate Node | ~0.9 A (peak) | SG90 vane servo under aerodynamic drag, cooling fan relay coil. |
| 5 V, WSW Node | ~1.2 A (peak) | TT DC motor during stall/inrush, TB6612 motor driver logic. |
| 12 V Main Input | ~0.6 A (worst-case) | Total reflected board load through buck converter (efficiency ~80-85%). |

The 3 A buck module provides healthy operating headroom. For motor starting inrush and servo transients, the 470 µF capacitor on the 5 V rail mitigates voltage dips and MCU brownout resets.

---

## 4. STM32F103C8T6 Pin Allocation

### 4.1. Pins Handled Directly on Blue Pill (Not Routed on Carrier)

| Function | Location on Blue Pill | Implementation Notes |
| :-- | :-- | :-- |
| SWD (PA13 SWDIO, PA14 SWCLK) | Dedicated 4-pin header on board edge | **Not routed to main pin rows**. Connected via ST-Link flying leads; preserve clear vertical clearance. Firmware must disable JTAG via `__HAL_AFIO_REMAP_SWJ_NOJTAG()` to release PA15, PB3, and PB4 for GPIO/timer use. |
| NRST | Tactile button on module + NRST pin on header | System reset. |
| BOOT0, BOOT1 (PB2) | 3-pin configuration jumpers on module | Configure boot source (Flash vs. System Memory bootloader). |
| High/Low-Speed Crystals | 8 MHz HSE and 32.768 kHz LSE crystals | PC14 and PC15 drive the 32 kHz crystal and must not be used as general GPIOs. |
| PC13 | Onboard user LED | **PC13-PC15 drive strength is limited to ~3 mA sink only (no source)**; cannot drive external loads directly. |
| USB Port | micro-USB connector | Reserved for bench flashing; do not connect while carrier is 5 V powered (see PCB-P07). |

### 4.2. Carrier Board Pin Routing (Mapped to Blue Pill Headers)

| Pin | Net Name | Peripheral / Mode | 5 V Tolerant (FT) | Functional Description |
| :-- | :-- | :-- | :-- | :-- |
| PA9, PA10 | UART_TX, UART_RX | USART1 | Yes (FT) | Debug console logging |
| PA11, PA12 | CAN_RX, CAN_TX | bxCAN | Yes (FT) | Default peripheral mapping |
| PB4 | CAN_STBY | GPIO Output | Yes (FT) | Transceiver standby control (Rs pin); tied to GND by default solder jumper (high-speed mode) |
| PA0 | ADC_A | ADC1_IN0 | No (non-FT) | Analog input: LDR light sensor (3.3 V range only) |
| PA1 | ADC_B | ADC1_IN1 | No (non-FT) | Auxiliary / spare analog input |
| PA2 | LED_FAULT | GPIO Output | No (non-FT) | Carrier fault warning LED |
| PA3 | USER_BTN | GPIO Input | No (non-FT) | User input push button (pulled up to 3.3 V) |
| PA4, PA5 | ECU_ID0, ECU_ID1 | GPIO Input | No (non-FT) | Hardware node identification solder jumpers |
| PA6, PA7 | SPARE | SPI1 | No (non-FT) | Expansion / general spare header |
| PA8 | ENC_CAP | TIM1_CH1 Input Capture | Yes (FT) | Wheel speed optical encoder capture (WSW) |
| PA15 | DO_4 | GPIO Output | Yes (FT) | Available after disabling JTAG remap |
| PB0 | PWM_H1 | TIM3_CH3 | No (non-FT) | High-speed PWM, 3.3 V push-pull |
| PB1 | PWM_H2 | TIM3_CH4 | No (non-FT) | High-speed PWM, 3.3 V push-pull |
| PB3, PB5 | SPARE | GPIO | See note | PB3 available after disabling JTAG remap |
| PB6 | PWM_L1 | TIM4_CH1 | Yes (FT) | 50 Hz PWM output for servo control |
| PB7 | PWM_L2 | TIM4_CH2 | Yes (FT) | Auxiliary slow PWM output |
| PB8 | DO_3 | GPIO Output | Yes (FT) | Digital Output 3 |
| PB9 | DHT_DATA | Bidirectional GPIO | Yes (FT) | DHT22 1-wire data (requires 4.7-10 kΩ pull-up to 3.3 V) |
| PB10 | DO_1 | GPIO Output | Yes (FT) | Digital Output 1 |
| PB11 | DO_2 | GPIO Output | Yes (FT) | Digital Output 2 |
| PB12 | DI_1 | GPIO Input (EXTI12) | Yes (FT) | Digital Input 1 |
| PB13 | DI_2 | GPIO Input (EXTI13) | Yes (FT) | Digital Input 2 |
| PB14 | DI_3 | GPIO Input (EXTI14) | Yes (FT) | Digital Input 3 |
| PB15 | DI_4 | GPIO Input (EXTI15) | Yes (FT) | Digital Input 4 |

> [!NOTE]
> **Regarding 5 V Tolerance (FT):** PA0-PA7, PB0, and PB1 are strictly non-5 V tolerant (3.3 V absolute maximum). Published documentation contains discrepancies regarding PB5; to ensure hardware safety, do not apply 5 V signals to PB5. Always cross-check the FT column in the STMicroelectronics STM32F103x8/xB datasheet before manufacturing.

**Functional Mapping per ECU Node:**

| Carrier Signal | BCM Node | Window Node | Climate Node | WSW Node |
| :-- | :-- | :-- | :-- | :-- |
| DI_1 (PB12) | Door Reed Switch | Obstacle Button | — | Brake Pedal Switch |
| DI_2 (PB13) | Lock/Unlock Button | — | — | — |
| DI_3 (PB14) | PIR Motion Sensor | — | — | — |
| ADC_A (PA0) | Ambient Light LDR | — | Solar Intensity LDR | — |
| DHT (PB9) | — | — | DHT22 Temp/Humidity | — |
| ENC_CAP (PA8) | — | — | — | Speed Encoder Capture |
| DO_1 (PB10) | Door Lock Relay | — | Fan Power Relay | Warning Indicator LED |
| DO_2 (PB11) | Door Unlock Relay | — | — | Warning Lamp Driver |
| DO_3 (PB8) | Alarm Buzzer | — | — | TB6612 Motor Driver IN1 |
| DO_4 (PA15) | — | — | — | TB6612 Motor Driver IN2 |
| PWM_H1 (PB0) | Headlight LED PWM | — | Auxiliary PWM Fan | TB6612 Motor Driver PWMA |
| PWM_H2 (PB1) | Cabin Interior LED PWM | — | — | — |
| PWM_L1 (PB6) | — | — | Flap Vane Servo (50 Hz) | — |

**Resource Consumption Summary:** Peak usage across all nodes requires: DI: 3 (BCM), DO: 4 (WSW), Fast PWM: 2 (BCM), Slow PWM: 1 (Climate), ADC: 1, Timer Capture: 1. Because the carrier provides 4 DI, 4 DO, 4 PWM, and 2 ADC, **every ECU has at least 1 spare channel per peripheral class**.

**Hardware Timer Allocation:**

| Hardware Timer | Allocated Role | Implementation Details |
| :-- | :-- | :-- |
| TIM1 | Speed Encoder Input Capture (PA8) | 16-bit capture timer, 1 µs tick resolution (prescaler 71 at 72 MHz clock), rolls over at ~65 ms; handled via timer overflow update interrupt with 500 ms zero-speed timeout logic (FR-WSW-07). |
| TIM3 | Fast PWM Generation (PB0, PB1) | Drives high-frequency actuators (LED dimming, motor drive) at 1 to 20 kHz. |
| TIM4 | Slow PWM Generation (PB6, PB7) | Standard 50 Hz PWM for SG90 / analog RC servos. |
| TIM2 | Dedicated STM32 HAL Timebase | Hardware 1 kHz timer dedicated to HAL timebase, eliminating interrupt preemption conflicts between HAL delays and the FreeRTOS scheduler tick. |

The ARM Cortex-M3 DWT (Data Watchpoint and Trace) cycle counter can also be leveraged for microsecond-precision benchmarking without consuming hardware timer peripherals.

---

## 5. Interface and Connector Specifications

| Identifier | Interface Connector | Pin Count & Signal Definition | Implementation Notes |
| :-- | :-- | :-- | :-- |
| PCB-C01 | **J_PWR** (12 V Main Input) | 2-pin 5.08 mm screw terminal | Bold polarity labeling on PCB silkscreen |
| PCB-C02 | **J_CAN1, J_CAN2** (CAN Bus) | 3-pin (CAN_H, CAN_L, GND) each, wired in parallel | Dual ports enabling clean daisy-chain bus cabling without drop stubs |
| PCB-C03 | **J_BP_L, J_BP_R** (Blue Pill Socket) | Two 1x20 female header strips | Matching standard Blue Pill 2.54 mm pinout |
| PCB-C04 | **J_UART** (Debug Console) | 3-pin header (TX, RX, GND) | Compatible with external 3.3 V USB-to-UART bridge |
| PCB-C05 | **J_AIN** (Analog Inputs) | 4-pin header (3V3, GND, ADC_A, ADC_B) | Strictly restricted to 3.3 V sensor supplies |
| PCB-C06 | **J_DIN** (Digital Inputs) | 6-pin header (DI_1 to DI_4, VCC_SEL [3V3/5V], GND) | Integrated series current limiting and RC filter capacitor |
| PCB-C07 | **J_DOUT** (Digital Outputs) | 6-pin header (DO_1 to DO_4, 5V, GND) | See Section 6 regarding 5 V open-drain pull-up configuration |
| PCB-C08 | **J_PWM** (PWM Outputs) | 4 independent 3-pin ports (GND, 5V, Signal) | Direct pinout match for standard 3-wire RC servos |
| PCB-C09 | **J_DHT** (Environmental Sensor) | 4-pin header (3V3, GND, DHT_DATA, Spare) | Dedicated breakout for DHT22 sensor |
| PCB-C10 | **J_ENC** (Speed Encoder) | 3-pin header (VCC_SEL [3V3/5V], GND, ENC_CAP) | PA8 is 5 V tolerant; accepts 5 V or 3.3 V optical encoders |
| PCB-C11 | **J_SPARE** (Expansion) | 6-pin header (PA6, PA7, PB3, PB5, 3V3, GND) | Unused GPIO and SPI1 pins for testing |

---

## 6. Circuit Protection and Logic Level Conditioning

| ID | Priority | Requirement Description |
| :-- | :-- | :-- |
| PCB-S01 | Core | **Interfacing to 5 V Modules (Relays, Buzzers):** Standard automotive/hobbyist relay modules utilize active-low optocouplers referenced to 5 V. Driving these from 3.3 V push-pull outputs may fail to turn off the optocoupler completely. Configure the 5 V-tolerant MCU outputs as **open-drain** with pull-ups to 5 V; provide solder-jumper footprints for optional 5 V pull-up resistors on each output (left open by default). |
| PCB-S02 | Core | **External Digital Inputs:** Route through a 1 kΩ series resistor and 100 nF ceramic capacitor to GND for contact debouncing, RF noise attenuation, and clamping protection. |
| PCB-S03 | Core | **Direct LED Indicators:** Sized series ballast resistors to limit forward drive current to <=8 mA per pin (well within the STM32 20 mA per-pin and 150 mA aggregate package ratings). |
| PCB-S04 | Core | **Analog Sensor Safeguards:** Power analog sensors strictly from the 3.3 V rail to eliminate the possibility of overvoltage damage to non-FT ADC inputs. |
| PCB-S05 | Recommended | **CAN ESD Protection:** Place an automotive-grade bidirectional ESD protection diode array (e.g., PESD1CAN or NUP2105L) directly at the CAN bus connector. |
| PCB-S06 | Recommended | **Common-Mode Filtering:** Include an optional footprint for a surface-mount common-mode choke on the CAN differential pair (bypassed with zero-ohm jumpers by default). |

---

## 7. Physical Layer CAN Requirements

| ID | Requirement Description |
| :-- | :-- |
| PCB-N01 | SN65HVD230 (SOIC-8) transceiver powered from Blue Pill 3.3 V supply; TXD connected to PA12, RXD to PA11; 100 nF 0805 decoupling capacitor placed adjacent to VCC pin. |
| PCB-N02 | Transceiver mode selection: Rs pin connected to ground via default solder jumper (high-speed operational mode) or optionally routed to PB4 for software standby control. |
| PCB-N03 | **120 Ω bus termination resistor** with enable jumper; populated and jumpered only on the two physical end nodes of the CAN network. |
| PCB-N04 | CAN_H and CAN_L routed as a tight, symmetrical differential pair, maintained as short as possible and isolated from the switching buck converter. |
| PCB-N05 | Dual CAN connectors wired in direct parallel to implement clean daisy-chain trunk bus wiring with minimal stub length. |
| PCB-N06 | Continuous CAN ground reference tied across all bus nodes (reference `System-Requirements-ECU.md` Section 3B). |

---

## 8. Board Layout and Mechanical Specifications

| ID | Specification Description |
| :-- | :-- |
| PCB-L01 | 2-layer FR-4 construction, 1.6 mm board thickness, 1 oz copper; bottom layer dedicated primarily to a continuous ground plane. |
| PCB-L02 | Position Blue Pill socket flush along one long board edge so that the **micro-USB port, 4-pin SWD header, and reset button** remain completely unobstructed. |
| PCB-L03 | Maintain overhead vertical clearance above the Blue Pill footprint; do not place tall components or connectors spanning across the module. |
| PCB-L04 | High-current power traces (12 V and 5 V rails) routed with minimum 1.0 mm trace width (1.5 mm recommended for 5 V). |
| PCB-L05 | Buck regulator isolated in a corner of the PCB, separated from the CAN transceiver and analog ADC circuitry; keep switching inductor away from CAN differential lines. |
| PCB-L06 | Universal 2.54 mm breakout grid for buck module integration, minimum pad area ~45 x 25 mm (verified against physical buck module). |
| PCB-L07 | Dedicated, exposed probe test points: 12V, 5V, 3V3, GND (x2), CAN_H, CAN_L, ADC_A, and one PWM channel. |
| PCB-L08 | Mechanical outline not exceeding 100 x 80 mm; four 3.2 mm diameter corner holes for M3 standoff mounting. |
| PCB-L09 | Peripherals grouped logically along board perimeters: Power and CAN bus on one edge, sensor and actuator headers on opposite edges. |
| PCB-L10 | Silkscreen labeling: Pin names, Blue Pill insertion keying/orientation, jumper functions, hardware revision, and an open text box for writing ECU identity. |
| PCB-L11 | Hand-soldering friendly footprints: 0805 or through-hole passives, SOIC-8 package for the SN65HVD230 transceiver. |

---

## 9. Fabrication, Assembly, and Test Specifications

| ID | Requirement Description |
| :-- | :-- |
| PCB-M01 | Generate industry-standard RS-274X Gerber files, Excellon drill files, and consolidated Bill of Materials (BOM). |
| PCB-M02 | 100% clean Design Rule Check (DRC) and Electrical Rule Check (ERC) prior to manufacturing release. |
| PCB-M03 | Fabricate a batch of 5 boards (4 for active ECU nodes, 1 backup prototype). |
| PCB-M04 | Provide complete assembly documentation and component placement map indexed by RefDes. |
| PCB-M05 | **Perform staged bring-up on a single prototype board first** (power supply validation, CAN communication) before assembling the remaining three units. |

**Component Assembly Breakdown on Carrier Board:**

| Component Category | Included Devices | Assembly Method |
| :-- | :-- | :-- |
| SMD Components | SN65HVD230DR (SOIC-8), SS34 (SMA), 0805 capacitors and resistors, 0805 LEDs, TVS diode | Manual iron with rosin flux |
| Through-Hole Parts | 1x20 female Blue Pill headers, terminal blocks, DC barrel jack, polyfuse, pushbuttons, 470 µF electrolytic cap | Standard hand soldering |
| Modular Sub-boards | LM2596 buck converter (via 2.54 mm grid), Blue Pill module | Header socketed / wired |

**Comprehensive Bring-Up Checklist & Acceptance Criteria (Executed per board):**

| Step | Verification Procedure | Acceptance Criteria (Proposed) |
| :-- | :-- | :-- |
| 0 | **Blue Pill Screening:** Query chip ID and Flash memory size using ST-Link utility | Verified genuine STM32F103, 64 KB (or 128 KB) Flash; screen out defective clones |
| 1 | Unpowered continuity check between 12V, 5V, 3V3, and GND (multimeter, board unpopulated) | Zero short circuits detected |
| 2 | Apply 12 V from current-limited DC bench supply; verify buck regulator output | 4.75 V to 5.25 V DC output |
| 3 | Populate Blue Pill, close jumper `JP_5V`, measure 3.3 V output rail | 3.3 V ± 3% (3.20 V to 3.40 V) |
| 4 | Measure quiescent idle current from 12 V supply | Consistent with baseline budget (~30-50 mA) |
| 5 | Connect ST-Link programmer to Blue Pill SWD pins | Target Cortex-M3 core detected without errors |
| 6 | Flash standard Blinky test firmware; observe PC13 onboard LED | LED toggles cleanly at programmed 1 Hz frequency |
| 7 | Connect USB-to-UART bridge to J_UART; verify serial terminal output | Boot banner and system clock frequency printed correctly |
| 8 | Run internal bxCAN loopback self-test | Transmitted test frames received with matching data |
| 9 | Multi-node CAN network communication test (pair with second node / DK1) | 500 kbps transmission, zero error frames; ~60 Ω bus impedance across dual terminators |
| 10 | ADC validation: Apply 3.3 V and GND to PA0 test point | ADC counts reflect expected rail boundaries (0 to 4095) |
| 11 | PWM validation: Probe PWM outputs using oscilloscope / logic analyzer | Frequency and duty cycle correspond exactly to configured timer registers |
| 12 | Digital I/O validation: Cycle DI and DO channels | State transitions verified; open-drain DO with 5 V pull-up achieves valid 5 V logic high |
| 13 | Timer capture validation: Inject square wave into ENC_CAP (PA8) | Input capture timer ISR accurately reports signal frequency and period |
| 14 | Reverse polarity stress test (current-limited supply) | Series Schottky diode blocks current; zero component damage |

---

## 10. Bill of Materials Impact (Option A vs. Discrete IC Option)

| Modification | Engineering Impact Details |
| :-- | :-- |
| Removed Components | Discrete STM32F103C8T6 chip, 8 MHz crystal, AMS1117-3.3 regulator (retained in BOM with quantity 0). |
| Added Components | Blue Pill modules (4 for active ECUs, 2 for early software dev, 1 spare); 1x20 female header strips. |
| Footprint Adjustments | Upgraded small SMD passives from 0603 to **0805** to facilitate high-yield manual hand-soldering. |
| Retained Components | SN65HVD230DR, SS34 Schottky diode, polyfuse, LM2596 buck module, screw terminals, DC barrel jack. |
| Cost Variation | Estimated budget variance of ~150,000 to 250,000 VND (well within thesis contingency limits). |

---

## 11. Final Engineering Deliverables

| Deliverable | Detailed Content |
| :-- | :-- |
| KiCad Schematics | Modular hierarchical sheets: Blue Pill socket, Power conditioning, CAN transceiver, I/O level conditioning, Field headers |
| Pin Mapping Matrix | Consolidated Section 4 table, verified against STMicroelectronics datasheet |
| KiCad PCB Layout | Fully routed 2-layer layout passing 100% design rule checks |
| Manufacturing Files | RS-274X Gerbers, Excellon drill files, pick-and-place files, and structured BOM |
| Bring-Up Test Report | Executed Section 9 checklist for all 4 boards with measurement data and scope captures |

---

## 12. Risk Management and Design Notes

| Identified Technical Risk | Engineering Mitigation Strategy |
| :-- | :-- |
| Low-quality Blue Pill clones or defective onboard LDOs | Source modules with reputable markings; verify chip ID and Flash size at receipt (Step 0); maintain spare modules. |
| Dimensional mismatch across Blue Pill pin header rows | Physically measure received Blue Pill module headers with digital calipers prior to freezing KiCad socket footprint (PCB-F01). |
| Obstructed SWD programming header or micro-USB port | Maintain strict vertical and horizontal clearance per PCB-L02 and PCB-L03. |
| Dual power supply conflict (simultaneous USB and 12 V carrier power) | Implement isolation jumper `JP_5V` (PCB-P07); mandate procedure to remove `JP_5V` before connecting USB programming cable. |
| Pin assignment conflict or timer resource collision after board fabrication | Rigorously audit Section 4 pinout matrix across all 4 ECU functional specifications before ordering; break out spare GPIOs to headers for rework jumpers. |
| Relay module fails to de-energize when driven by 3.3 V logic | Implement PCB-S01: configure 5 V-tolerant pins as open-drain with optional 5 V pull-up resistors. |
| 5 V voltage dip during motor starting inrush or servo slewing | Provide >=470 µF electrolytic bulk capacitance on carrier 5 V rail; isolate high-current actuator returns. |
| Buck converter dimensional variance across manufacturers | Provide a flexible 2.54 mm breakout hole grid with flying lead solder points rather than a rigid proprietary footprint (D3). |
| CAN bit timing mismatch with STM32MP157 and Jetson nodes | Strictly follow `System-Requirements-ECU.md` Section 3B; bring up bus incrementally node-by-node. |
| Inadvertent 5 V applied to non-5 V tolerant MCU pins (PA0-PA7, PB0, PB1) | Restrict analog sensor connectors strictly to 3.3 V rail; verify pull-up voltage sources prior to power-on. |
