"""Inspect an already compiled test image; never edits production sources."""
import struct
import sys
from pathlib import Path

task_root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(task_root / '.tmp_protocol/test_deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_INVALID
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC

uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
uc.mem_map(0x08000000, 0x100000)
uc.mem_map(0x20000000, 0x100000)
with (Path(__file__).parent / 'root-control-tests/tool_mode_tests.axf').open('rb') as stream:
    elf = ELFFile(stream)
    for segment in elf.iter_segments():
        if segment['p_type'] == 'PT_LOAD' and segment['p_filesz']:
            uc.mem_write(segment['p_vaddr'], segment.data())
    symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}

def word(name):
    return struct.unpack('<I', uc.mem_read(symbols[name], 4))[0]

def invalid_memory(instance, access, address, size, value, data):
    pc = instance.reg_read(UC_ARM_REG_PC)
    print(f'Invalid memory access={access}, address=0x{address:08x}, size={size}, value={value}')
    print(f'PC=0x{pc:08x}, SP=0x{instance.reg_read(UC_ARM_REG_SP):08x}, LR=0x{instance.reg_read(UC_ARM_REG_LR):08x}')
    print(f'case={word("test_case")}, line={word("test_failure_line")}, checks={word("test_checks")}')
    closest = sorted(((abs((v & ~1)-pc), n, v) for n, v in symbols.items() if 0x08000000 <= v < 0x08100000))[:5]
    print('Nearest symbols:', closest)
    return False

uc.hook_add(UC_HOOK_MEM_INVALID, invalid_memory)
stop = 0x080f0000
uc.reg_write(UC_ARM_REG_SP, 0x200ffff0)
uc.reg_write(UC_ARM_REG_LR, stop | 1)
uc.emu_start(symbols['run_tests'] | 1, stop, timeout=30_000_000, count=50_000_000)
print(f'Completed case={word("test_case")}, line={word("test_failure_line")}, checks={word("test_checks")}')
