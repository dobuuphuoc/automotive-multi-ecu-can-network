# Test chạy trên máy (host), không cần board. Dựng: cmake -S tests -B build/tests && cmake --build build/tests && ctest --test-dir build/tests --output-on-failure
cmake_minimum_required(VERSION 3.22)
project(ecu_host_tests C)
enable_testing()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_library(unity STATIC ${ROOT}/third_party/Unity/src/unity.c)
target_include_directories(unity PUBLIC ${ROOT}/third_party/Unity/src)

add_executable(test_crc8 test_crc8.c ${ROOT}/common/e2e_crc8.c)
target_include_directories(test_crc8 PRIVATE ${ROOT}/common)
target_link_libraries(test_crc8 PRIVATE unity)
add_test(NAME crc8 COMMAND test_crc8)

add_executable(test_e2e_state_machine test_e2e_state_machine.c ${ROOT}/common/e2e_crc8.c)
target_include_directories(test_e2e_state_machine PRIVATE ${ROOT}/common)
target_link_libraries(test_e2e_state_machine PRIVATE unity)
add_test(NAME e2e_state_machine COMMAND test_e2e_state_machine)

add_executable(test_com_packing test_com_packing.c ${ROOT}/common/can_com.c ${ROOT}/common/e2e_crc8.c)
target_include_directories(test_com_packing PRIVATE ${ROOT}/common)
target_link_libraries(test_com_packing PRIVATE unity)
add_test(NAME com_packing COMMAND test_com_packing)

add_executable(test_diag_wdgm test_diag_wdgm.c
  ${ROOT}/common/diag_dem.c
  ${ROOT}/common/diag_dcm.c
  ${ROOT}/common/wdgm.c
)
target_include_directories(test_diag_wdgm PRIVATE ${ROOT}/common)
target_link_libraries(test_diag_wdgm PRIVATE unity)
add_test(NAME diag_wdgm COMMAND test_diag_wdgm)

add_executable(test_swc_runnables test_swc_runnables.c
  ${ROOT}/rte/rte.c
  ${ROOT}/common/can_com.c
  ${ROOT}/common/e2e_crc8.c
  ${ROOT}/common/diag_dem.c
  ${ROOT}/apps/ecu_bcm/swc_bcm.c
  ${ROOT}/apps/ecu_climate/swc_climate.c
  ${ROOT}/apps/ecu_window/swc_window.c
  ${ROOT}/apps/ecu_wsw/swc_wsw.c
)
target_include_directories(test_swc_runnables PRIVATE
  ${ROOT}/rte
  ${ROOT}/common
  ${ROOT}/apps/ecu_bcm
  ${ROOT}/apps/ecu_climate
  ${ROOT}/apps/ecu_window
  ${ROOT}/apps/ecu_wsw
)
target_link_libraries(test_swc_runnables PRIVATE unity)
add_test(NAME swc_runnables COMMAND test_swc_runnables)




