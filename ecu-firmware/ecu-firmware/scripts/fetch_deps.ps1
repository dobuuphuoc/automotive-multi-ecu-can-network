# Windows PowerShell: Download third-party dependencies into third_party/
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot "..\third_party") | Out-Null
Set-Location (Join-Path $PSScriptRoot "..\third_party")
function Clone($name, $url, $tag) {
  if (Test-Path $name) { Write-Host "$name already exists, skipping"; return }
  git clone --depth 1 --branch $tag $url $name
}
Clone "FreeRTOS-Kernel"      "https://github.com/FreeRTOS/FreeRTOS-Kernel.git"                 "V11.2.0"
Clone "cmsis_core"           "https://github.com/STMicroelectronics/cmsis_core.git"            "v5.9.0"
Clone "cmsis_device_f1"      "https://github.com/STMicroelectronics/cmsis_device_f1.git"       "v4.3.4"
Clone "stm32f1xx_hal_driver" "https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git" "v1.1.10"
Clone "Unity"                "https://github.com/ThrowTheSwitch/Unity.git"                      "v2.6.0"
Write-Host "Done."
