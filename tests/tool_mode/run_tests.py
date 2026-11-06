"""Compile real Cortex-M0 mode/storage code and exercise it in Unicorn."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.planning/2026-11-05-reverse-scene-memory/control-tests'
KEIL = Path('D:/Users/mym02/AppData/Local/Keil_v5')
spec = importlib.util.spec_from_file_location('battery_runner', ROOT / 'tests/battery_com/run_tests.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    parser.add_argument('--case', type=int, default=0, help='run only the numbered case; default runs all')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'tm1650.h').write_text('#include <stdint.h>\nuint8_t TM1650_ReadKey(uint8_t *key);\n', encoding='utf-8')
    prefix = ROOT / '.planning/2026-11-04-tm1650/original' if args.baseline else ROOT
    code = ['#include "main.h"', '#include "board.h"', '#include "user_control.h"',
            '#include "parameter.h"', '#include "motor_control.h"', '#include "motor_config.h"', '#include "flash_data_save.h"', '#include "drv_pwm.h"',
            'User_list_Struct user_list;', 'Machine_ERROR_HOLD machine_error_hold;',
            'Mc_core_list mc_core_list;', 'Motor_Control_list motor_control_list;']
    if not args.baseline:
        code.append('#include "tool_mode.h"')
    for signature in ['void user_para_init(void)', 'void pwm_duty_aim_deal(void)', 'void user_gears_handle(void)']:
        code.append(runner.extract_function(prefix / 'USER/user_control.c', signature))
    if not args.baseline:
        code.append(runner.extract_function(ROOT / 'USER/user_control.c', 'void user_direction_handle(void)'))
        code.append(runner.extract_function(ROOT / 'MOTOR_CONTROL/motor_control.c', 'void pwm_set(void)'))
    code += ['#define TEST_BASELINE ' + str(int(args.baseline)),
             '#define TEST_ONLY_CASE ' + str(args.case),
             '#include "' + (ROOT / 'tests/tool_mode/test_tool_mode.c').as_posix() + '"']
    source = OUT / ('baseline.c' if args.baseline else 'test_main.c')
    source.write_text('\n'.join(code), encoding='utf-8')
    image = OUT / ('baseline.axf' if args.baseline else 'tool_mode_tests.axf')
    command = [str(KEIL / 'ARM/ARMCLANG/bin/armclang.exe'), '--target=arm-arm-none-eabi',
               '-mcpu=cortex-m0', '-std=c99', '-O1', '-g', '-fshort-enums', '-ffunction-sections',
               '-fdata-sections', '-DUSE_STDPERIPH_DRIVER', '-DUSE_5V', '-Wall', '-Wextra', '-Werror',
               '-Wno-invalid-source-encoding', '-Wno-nonportable-include-path', '-Wno-unused-variable',
               '-Wl,--ro-base=0x08000000,--rw-base=0x20000000,--entry=run_tests',
               '-o', str(image), str(source), str(prefix / 'USER/flash_data_save.c')]
    if not args.baseline:
        command.append(str(ROOT / 'USER/tool_mode.c'))
    for path in [OUT, ROOT / 'USER/inc', ROOT / 'MOTOR_CONTROL', ROOT / 'MC_CORE', ROOT / 'DRIVE/inc',
                 ROOT / 'SYSTEM/inc', ROOT / 'MM32SPIN0230/Include', ROOT / 'MM32SPIN0230/HAL_Lib/inc',
                 ROOT / 'CMSIS/Core/Include']:
        command += ['-I', str(path)]
    subprocess.run(command, check=True, cwd=ROOT)
    execute(image, args.baseline, args.case)


def execute(image, baseline, only_case=0):
    import struct
    sys.path.insert(0, str(ROOT / '.tmp_protocol/test_deps'))
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
    from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 0x100000)
    uc.mem_map(0x20000000, 0x100000)
    with image.open('rb') as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD' and segment['p_filesz']:
                uc.mem_write(segment['p_vaddr'], segment.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    stop = 0x080f0000
    uc.reg_write(UC_ARM_REG_SP, 0x200ffff0)
    uc.reg_write(UC_ARM_REG_LR, stop | 1)
    uc.emu_start(symbols['run_tests'] | 1, stop, timeout=30_000_000, count=50_000_000)
    assert uc.reg_read(UC_ARM_REG_PC) == stop, 'test did not return'
    def word(name):
        return struct.unpack('<I', uc.mem_read(symbols[name], 4))[0]
    line, case, checks = map(word, ['test_failure_line', 'test_case', 'test_checks'])
    if line:
        raise AssertionError(f'{"old firmware" if baseline else "mode/storage"} case {case}, test_tool_mode.c:{line}, after {checks} checks')
    label = f'case {only_case}' if only_case else f'{case} cases'
    print(f'PASS: {label}, {checks} checks (real ARM C, simulated key/Flash hardware; not a bench test)')


if __name__ == '__main__':
    main()
