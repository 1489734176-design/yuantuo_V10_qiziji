param([string[]]$TaskVariants = @('default', 'override'))
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$taskCompiler = 'D:\Users\mym02\AppData\Local\Keil_v5\ARM\ARMCLANG\bin\armclang.exe'
$taskPython = 'D:\Program Files\Python314\python.exe'
$taskExecuteCode = 'import sys; from pathlib import Path; sys.path.insert(0,str(Path(sys.argv[1])/"tests/seven_led")); import run_tests; run_tests.execute(Path(sys.argv[2]),source_name="test_mode_config.c")'
foreach($taskVariant in $TaskVariants)
{
    $taskOut = Join-Path $PSScriptRoot ('mode-config-tests\' + $taskVariant)
    New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
    $taskImage = Join-Path $taskOut 'mode_config_tests.axf'
    $taskArgs = @('--target=arm-arm-none-eabi', '-mcpu=cortex-m0', '-std=c99', '-O1', '-g', '-fshort-enums', '-ffunction-sections', '-fdata-sections', '-DUSE_STDPERIPH_DRIVER', '-DUSE_5V', '-Wall', '-Wextra', '-Werror', '-Wno-invalid-source-encoding', '-Wno-nonportable-include-path', '-Wl,--ro-base=0x08000000,--rw-base=0x20000000,--entry=run_tests', '-o', $taskImage)
    if($taskVariant -eq 'override') { $taskArgs += @('-DTEST_MODE_CONFIG_OVERRIDE', '-include', (Join-Path $taskRoot 'tests\tool_mode\config_override.h')) }
    foreach($taskFile in @('tests\tool_mode\test_mode_config.c', 'USER\led.c', 'USER\tool_mode.c', 'MM32SPIN0230\HAL_Lib\src\hal_gpio.c', 'MM32SPIN0230\HAL_Lib\src\hal_rcc.c')) { $taskArgs += (Join-Path $taskRoot $taskFile) }
    foreach($taskInclude in @('USER\inc', 'TM1650', 'MOTOR_CONTROL', 'MC_CORE', 'DRIVE\inc', 'SYSTEM\inc', 'MM32SPIN0230\Include', 'MM32SPIN0230\HAL_Lib\inc', 'CMSIS\Core\Include')) { $taskArgs += @('-I', (Join-Path $taskRoot $taskInclude)) }
    & $taskCompiler @taskArgs
    if($LASTEXITCODE -ne 0) { throw "Configuration test compile failed: $taskVariant" }
    & $taskPython -c $taskExecuteCode $taskRoot $taskImage
    if($LASTEXITCODE -ne 0) { throw "Configuration behavior failed: $taskVariant" }
    Write-Output "PASS: $taskVariant mode configuration"
}
