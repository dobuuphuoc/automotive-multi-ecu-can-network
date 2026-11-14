#!/usr/bin/env bash
# Download third-party dependencies into third_party/ (pinned versions).
set -euo pipefail
mkdir -p "$(dirname "$0")/../third_party"
cd "$(dirname "$0")/../third_party"
clone() { # name url tag
  if [ -d "$1" ]; then echo "$1 already exists, skipping"; return; fi
  git clone --depth 1 --branch "$3" "$2" "$1"
}
clone FreeRTOS-Kernel        https://github.com/FreeRTOS/FreeRTOS-Kernel.git                  V11.2.0
clone cmsis_core             https://github.com/STMicroelectronics/cmsis_core.git              v5.9.0
clone cmsis_device_f1        https://github.com/STMicroelectronics/cmsis_device_f1.git         v4.3.4
clone stm32f1xx_hal_driver   https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git    v1.1.10
clone Unity                  https://github.com/ThrowTheSwitch/Unity.git                       v2.6.0
echo "Done."
