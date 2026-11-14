cmake_minimum_required(VERSION 3.22)

# Toolchain đặt trong CMakePresets.json (hoặc -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake)
project(ecu_firmware C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BRINGUP_CAN_LOOPBACK "Bring-up: CAN loopback (không cần bus)" OFF)

set(TP ${CMAKE_SOURCE_DIR}/third_party)
set(HAL_DIR    ${TP}/stm32f1xx_hal_driver)
set(CMSIS_DEV  ${TP}/cmsis_device_f1)
set(CMSIS_CORE ${TP}/cmsis_core)
set(RTOS_DIR   ${TP}/FreeRTOS-Kernel)

foreach(d ${HAL_DIR} ${CMSIS_DEV} ${CMSIS_CORE} ${RTOS_DIR})
  if(NOT EXISTS ${d})
    message(FATAL_ERROR "Thiếu ${d}. Chạy scripts/fetch_deps.sh (Linux/macOS/Git-Bash) hoặc scripts/fetch_deps.ps1 (Windows).")
  endif()
endforeach()

set(MCU_FLAGS -mcpu=cortex-m3 -mthumb)

# ---------------- Nền tảng dùng chung cho mọi ECU (OBJECT library) ----------------
# Dùng OBJECT thay vì STATIC để handler ngắt/hàm override không bị bỏ sót khi link.
add_library(ecu_platform OBJECT
  # CMSIS + startup
  ${CMSIS_DEV}/Source/Templates/system_stm32f1xx.c
  ${CMSIS_DEV}/Source/Templates/gcc/startup_stm32f103xb.s
  # HAL
  ${HAL_DIR}/Src/stm32f1xx_hal.c
  ${HAL_DIR}/Src/stm32f1xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_dma.c
  ${HAL_DIR}/Src/stm32f1xx_hal_exti.c
  ${HAL_DIR}/Src/stm32f1xx_hal_flash.c
  ${HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_iwdg.c
  ${HAL_DIR}/Src/stm32f1xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32f1xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_tim.c
  ${HAL_DIR}/Src/stm32f1xx_hal_tim_ex.c
  ${HAL_DIR}/Src/stm32f1xx_hal_uart.c
  ${HAL_DIR}/Src/stm32f1xx_hal_can.c
  # FreeRTOS (cấp phát tĩnh: không cần heap_x.c)
  ${RTOS_DIR}/tasks.c
  ${RTOS_DIR}/queue.c
  ${RTOS_DIR}/list.c
  ${RTOS_DIR}/portable/GCC/ARM_CM3/port.c
  # Nền tảng của ta
  platform/stm32f1xx_it.c
  platform/hal_timebase_tim2.c
  platform/freertos_hooks.c
  platform/syscalls.c
  # Mã dùng chung
  common/can_com.c
  common/can_driver.c
  common/diag_dcm.c
  common/diag_dem.c
  common/e2e_crc8.c
  common/log.c
  common/wdgm.c
  # Tầng RTE
  rte/rte.c
)

target_include_directories(ecu_platform PUBLIC
  platform
  common
  rte
  ${HAL_DIR}/Inc
  ${CMSIS_DEV}/Include
  ${CMSIS_CORE}/Include
  ${RTOS_DIR}/include
  ${RTOS_DIR}/portable/GCC/ARM_CM3
)

target_compile_definitions(ecu_platform PUBLIC USE_HAL_DRIVER STM32F103xB)

target_compile_options(ecu_platform PUBLIC
  ${MCU_FLAGS}
  -ffunction-sections -fdata-sections
  -Wall -Wextra
  $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
  $<$<CONFIG:Debug>:-Og -g3>
  $<$<CONFIG:Release>:-Os>
)
target_compile_options(ecu_platform PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>)

# ---------------- Hàm dựng một ECU ----------------
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/STM32F103C8Tx_FLASH.ld)

function(add_ecu name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE ecu_platform)
  target_link_options(${name} PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,-Map=${name}.map,--cref
    -Wl,--print-memory-usage
  )
  set_target_properties(${name} PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${LINKER_SCRIPT})
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${name}> ${name}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${name}> ${name}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${name}>
    COMMENT "Tạo ${name}.bin / .hex")
endfunction()

# ---------------- Các ứng dụng ----------------
add_ecu(bringup_bluepill apps/bringup_bluepill/main.c)
target_include_directories(bringup_bluepill PRIVATE apps/bringup_bluepill)
if(BRINGUP_CAN_LOOPBACK)
  target_compile_definitions(bringup_bluepill PRIVATE BRINGUP_CAN_LOOPBACK_DEFAULT=1)
endif()

add_ecu(ecu_bcm apps/ecu_bcm/main.c apps/ecu_bcm/swc_bcm.c)
target_include_directories(ecu_bcm PRIVATE apps/ecu_bcm)

add_ecu(ecu_climate apps/ecu_climate/main.c apps/ecu_climate/swc_climate.c)
target_include_directories(ecu_climate PRIVATE apps/ecu_climate)

add_ecu(ecu_window apps/ecu_window/main.c apps/ecu_window/swc_window.c)
target_include_directories(ecu_window PRIVATE apps/ecu_window)

add_ecu(ecu_wsw apps/ecu_wsw/main.c apps/ecu_wsw/swc_wsw.c)
target_include_directories(ecu_wsw PRIVATE apps/ecu_wsw)

