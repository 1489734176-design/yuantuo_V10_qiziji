"""Compile and emulate the real calibration function; never rewrite production sources."""
import importlib.util
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.planning/2026-11-06-scene-current-check/calibration-tests'
KEIL = Path('D:/Users/mym02/AppData/Local/Keil_v5')
spec = importlib.util.spec_from_file_location('battery_runner', ROOT / 'tests/battery_com/run_tests.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / 'test_translation_unit.c'
    code = '#include "' + (ROOT / 'tests/current_offset/test_current_offset.c').as_posix() + '"\n'
    code += runner.extract_function(ROOT / 'MOTOR_CONTROL/motor_control.c', 'void CurrentOffsetCalibration(void)')
    source.write_text(code, encoding='utf-8')
    image = OUT / 'current_offset_tests.axf'
    command = [str(KEIL / 'ARM/ARMCLANG/bin/armclang.exe'), '--target=arm-arm-none-eabi',
               '-mcpu=cortex-m0', '-std=c99', '-O1', '-g', '-fshort-enums', '-ffunction-sections',
               '-fdata-sections', '-DUSE_STDPERIPH_DRIVER', '-DUSE_5V', '-Wall', '-Wextra', '-Werror',
               '-Wno-invalid-source-encoding', '-Wno-nonportable-include-path',
               '-Wl,--ro-base=0x08000000,--rw-base=0x20000000,--entry=run_tests',
               '-o', str(image), str(source)]
    for path in ['USER/inc', 'TM1650', 'MOTOR_CONTROL', 'MC_CORE', 'DRIVE/inc', 'SYSTEM/inc',
                 'MM32SPIN0230/Include', 'MM32SPIN0230/HAL_Lib/inc', 'CMSIS/Core/Include']:
        command += ['-I', str(ROOT / path)]
    subprocess.run(command, check=True, cwd=ROOT)
    execute(image)


def execute(image):
    import struct
    sys.path.insert(0, str(ROOT / '.tmp_protocol/test_deps'))
    from elftools.elf.elffile import ELFFile
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
    from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0x08000000, 0x100000)
    uc.mem_map(0x20000000, 0x100000)
    uc.mem_map(0x40000000, 0x40000)
    with image.open('rb') as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD' and segment['p_filesz']:
                uc.mem_write(segment['p_vaddr'], segment.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    stop = 0x080f0000
    uc.reg_write(UC_ARM_REG_SP, 0x200ffff0)
    uc.reg_write(UC_ARM_REG_LR, stop | 1)
    uc.emu_start(symbols['run_tests'] | 1, stop, timeout=10_000_000, count=5_000_000)
    assert uc.reg_read(UC_ARM_REG_PC) == stop, 'calibration test did not return'
    def word(name):
        return struct.unpack('<I', uc.mem_read(symbols[name], 4))[0]
    line, case, checks = map(word, ['test_failure_line', 'test_case', 'test_checks'])
    if line:
        raise AssertionError(f'calibration case {case}, test_current_offset.c:{line}, after {checks} checks')
    print(f'PASS: {case} cases, {checks} checks (real ARM calibration, simulated ADC conversions; not a bench test)')


if __name__ == '__main__':
    main()
