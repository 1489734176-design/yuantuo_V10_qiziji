"""Exercise real TM1650 ARM code against an edge-level TM1650 bus model.

Mutations caught: reversed board pins, ACK sampled after the ninth rising
edge, LSB-first command/data, missing read NACK/STOP, discarded failed dirty
writes, lost requested display configuration, and more than one transaction
in FlushStep. The slave drives ACK at the eighth falling edge and releases it
at the ninth rising edge, as TM-TM1650.pdf page 3 explicitly specifies.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.planning/2026-11-04-tm1650/driver-tests'
KEIL = Path('D:/Users/mym02/AppData/Local/Keil_v5')
sys.path.insert(0, str(ROOT / '.tmp_protocol/test_deps'))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    image = OUT / 'tm1650_tests.axf'
    command = [str(KEIL / 'ARM/ARMCLANG/bin/armclang.exe'), '--target=arm-arm-none-eabi',
               '-mcpu=cortex-m0', '-std=c99', '-O1', '-g', '-fshort-enums',
               '-ffunction-sections', '-fdata-sections', '-DUSE_STDPERIPH_DRIVER',
               '-Wall', '-Wextra', '-Werror', '-Wno-invalid-source-encoding',
               '-Wno-nonportable-include-path',
               '-Wl,--ro-base=0x08000000,--rw-base=0x20000000,--entry=TM1650_Init,--no_remove',
               '-o', str(image), str(ROOT / 'tests/tm1650/test_support.c'),
               str(ROOT / 'TM1650/tm1650.c'),
               str(ROOT / 'MM32SPIN0230/HAL_Lib/src/hal_gpio.c'),
               str(ROOT / 'MM32SPIN0230/HAL_Lib/src/hal_rcc.c')]
    for folder in ['TM1650', 'SYSTEM/inc', 'MM32SPIN0230/Include',
                   'MM32SPIN0230/HAL_Lib/inc', 'CMSIS/Core/Include']:
        command += ['-I', str(ROOT / folder)]
    subprocess.run(command, check=True, cwd=ROOT)
    return image


class Board:
    BASE = 0x48000000

    def __init__(self, image, release_on_ack_rise=True):
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        for base, size in [(0x08000000, 0x100000), (0x20000000, 0x100000),
                           (0x40000000, 0x40000), (self.BASE, 0x10000)]:
            self.uc.mem_map(base, size)
        with image.open('rb') as stream:
            elf = ELFFile(stream)
            for seg in elf.iter_segments():
                if seg['p_type'] == 'PT_LOAD' and seg['p_filesz']:
                    self.uc.mem_write(seg['p_vaddr'], seg.data())
            self.symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
        # Communication cases use the compiled pin assignment so the baseline
        # exposes protocol failures separately; test_board_gpio enforces new IO.
        self.sda_pin = self.word('test_sda_pin')
        self.scl_pin = self.word('test_scl_pin')
        self.release_on_ack_rise = release_on_ack_rise
        self.master_sda = self.scl = self.slave_sda = 1
        self.active = None
        self.phase = 'idle'
        self.bit_count = self.byte = 0
        self.ack = True
        self.ack_queue = []
        self.key = 0x47
        self.transactions = []
        self.idr_reads = []
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self.gpio_write)
        self.uc.mem_write(self.BASE + 0x0c, struct.pack('<I', self.sda_pin | self.scl_pin))
        self.update_idr()

    def word(self, symbol):
        return struct.unpack('<I', self.uc.mem_read(self.symbols[symbol], 4))[0]

    def update_idr(self):
        value = (self.scl_pin if self.scl else 0) | (self.sda_pin if self.master_sda and self.slave_sda else 0)
        self.uc.mem_write(self.BASE + 0x08, struct.pack('<I', value))

    def gpio_write(self, uc, access, address, size, value, _):
        if address not in (self.BASE + 0x10, self.BASE + 0x14):
            return
        odr = struct.unpack('<I', uc.mem_read(self.BASE + 0x0c, 4))[0]
        odr = ((odr | (value & 0xffff)) & ~(value >> 16)) if address == self.BASE + 0x10 else odr & ~value
        uc.mem_write(self.BASE + 0x0c, struct.pack('<I', odr))
        new_sda = bool(odr & self.sda_pin)
        new_scl = bool(odr & self.scl_pin)
        if self.scl and new_scl and new_sda != self.master_sda and self.slave_sda:
            if not new_sda:
                assert self.active is None, 'unexpected repeated START'
                self.active = {'write': [], 'write_acks': [], 'read': [], 'nack': None, 'stop': False}
                self.phase = 'write'
                self.bit_count = self.byte = 0
            elif self.active is not None:
                self.active['stop'] = True
                self.transactions.append(self.active)
                self.active = None
                self.phase = 'idle'
        self.master_sda = int(new_sda)
        if new_scl != self.scl:
            self.scl = int(new_scl)
            if self.active is not None:
                if new_scl:
                    self.rising()
                else:
                    self.falling()
        self.update_idr()

    def rising(self):
        if self.phase == 'write':
            if self.bit_count < 8:
                self.byte = (self.byte << 1) | self.master_sda
                self.bit_count += 1
                if self.bit_count == 8:
                    self.active['write'].append(self.byte)
            else:
                self.active['write_acks'].append(self.ack)
                self.phase = 'write_ack_high'
                if self.release_on_ack_rise:
                    self.slave_sda = 1
        elif self.phase == 'read':
            self.active['read'].append(self.master_sda & self.slave_sda)
            self.bit_count += 1
        elif self.phase == 'read_nack':
            self.active['nack'] = self.master_sda
            self.phase = 'tail'

    def falling(self):
        if self.phase == 'write' and self.bit_count == 8:
            self.ack = self.ack_queue.pop(0) if self.ack_queue else True
            self.slave_sda = 0 if self.ack else 1
        elif self.phase == 'write_ack_high':
            self.slave_sda = 1
            if len(self.active['write']) == 1 and self.byte == 0x49 and self.ack:
                self.phase = 'read'
                self.bit_count = 0
                self.slave_sda = (self.key >> 7) & 1
            else:
                self.phase = 'write' if len(self.active['write']) == 1 else 'tail'
                self.bit_count = self.byte = 0
        elif self.phase == 'read':
            if self.bit_count < 8:
                self.slave_sda = (self.key >> (7 - self.bit_count)) & 1
            else:
                self.slave_sda = 1
                self.phase = 'read_nack'

    def call(self, name, *args):
        assert name in self.symbols, f'missing public API: {name}'
        stop = 0x080f0000
        self.uc.reg_write(UC_ARM_REG_SP, 0x200ffff0)
        self.uc.reg_write(UC_ARM_REG_LR, stop | 1)
        for register, value in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2], args):
            self.uc.reg_write(register, value)
        self.uc.emu_start(self.symbols[name] | 1, stop, timeout=10_000_000, count=10_000_000)
        assert self.uc.reg_read(UC_ARM_REG_PC) == stop, f'{name} did not return'
        assert self.active is None, f'{name} omitted STOP'
        return self.uc.reg_read(UC_ARM_REG_R0)

    def ready(self):
        self.call('TM1650_Init')
        # Drain init retries where a deliberately failing Init case did not.
        for _ in range(8):
            self.call('TM1650_FlushStep')
        self.transactions.clear()


def single(board, want):
    assert len(board.transactions) == 1, f'expected one transaction, got {board.transactions}'
    tx = board.transactions[0]
    assert tx['write'] == want, f'wrong MSB-first bytes: {tx}'
    assert tx['stop'], f'missing STOP: {tx}'
    return tx


def test_board_gpio(image):
    board = Board(image)
    board.call('TM1650_Init')
    crh = struct.unpack('<I', board.uc.mem_read(board.BASE + 4, 4))[0]
    assert (crh >> 12) & 15 == 7, 'PA11 must be SDA open drain with external 4.7K pull-up'
    assert (crh >> 16) & 15 == 3, 'PA12 must be SCL push-pull'


def test_ack_before_ninth_rising_edge(image):
    board = Board(image)
    board.ready()
    assert board.call('TM1650_SetDisplay', 0x31) == 1, 'ACK was sampled after slave released it'
    single(board, [0x48, 0x31])


def test_read_key_msb_nack_stop(image):
    for raw in [0x46, 0x47, 0x07, 0x00, 0xA5]:
        board = Board(image)
        board.ready()
        board.key = raw
        pointer = 0x200f0000
        board.uc.mem_write(pointer, b'\xcc')
        assert board.call('TM1650_ReadKey', pointer) == 1, 'acknowledged key read must succeed'
        assert board.uc.mem_read(pointer, 1) == bytes([raw]), f'wrong MSB-first key: {raw:#x}'
        tx = single(board, [0x49])
        assert tx['read'] == [(raw >> shift) & 1 for shift in range(7, -1, -1)], 'wrong receive clocks'
        assert tx['nack'] == 1, 'last byte must be followed by released SDA / NACK'


def test_read_nack_preserves_output(image):
    board = Board(image)
    board.ready()
    board.ack_queue = [False]
    pointer = 0x200f0000
    board.uc.mem_write(pointer, b'\xcc')
    assert board.call('TM1650_ReadKey', pointer) == 0, 'missing command ACK must fail'
    assert board.uc.mem_read(pointer, 1) == b'\xcc', 'failed read overwrote caller output'
    tx = single(board, [0x49])
    assert not tx['read'], 'must not clock key data after command NACK'
    board.transactions.clear()
    assert board.call('TM1650_ReadKey', 0) == 0, 'null key pointer must fail'
    assert not board.transactions, 'null pointer must not consume bus time'


def test_flush_step_failed_digit_retries(image):
    for acknowledgements in [[False], [True, False]]:
        board = Board(image, release_on_ack_rise=False)
        board.ready()
        board.call('TM1650_SetDigit', 2, 0xA5)
        assert not board.transactions, 'shadow update must not access bus'
        board.ack_queue = list(acknowledgements)
        assert board.call('TM1650_FlushStep') == 0, 'failed display write must report failure'
        assert len(board.transactions) == 1, 'one step must attempt only one transaction'
        board.transactions.clear()
        assert board.call('TM1650_FlushStep') == 1, 'failed digit lost dirty state'
        single(board, [0x6C, 0xA5])
        board.transactions.clear()
        assert board.call('TM1650_FlushStep') == 0, 'acknowledged digit must become clean'
        assert not board.transactions, 'clean shadow must not access bus'


def test_failed_configuration_retries_latest(image):
    board = Board(image, release_on_ack_rise=False)
    board.ready()
    board.ack_queue = [True, False]
    assert board.call('TM1650_SetDisplay', 0x34) == 0, 'failed configuration must report failure'
    single(board, [0x48, 0x34])
    board.transactions.clear()
    assert board.call('TM1650_FlushStep') == 1, 'failed configuration lost retry state'
    single(board, [0x48, 0x34])
    board.transactions.clear()
    assert board.call('TM1650_FlushStep') == 0
    assert not board.transactions
    board.ack_queue = [False]
    board.call('TM1650_SetDisplay', 0x01)
    board.ack_queue = [False]
    board.call('TM1650_SetDisplay', 0x04)
    board.transactions.clear()
    assert board.call('TM1650_FlushStep') == 1
    single(board, [0x48, 0x04])


def test_init_failures_remain_dirty(image):
    board = Board(image, release_on_ack_rise=False)
    board.ack_queue = [False] * 20
    board.call('TM1650_Init')
    board.ack_queue = []
    board.transactions.clear()
    for _ in range(5):
        assert board.call('TM1650_FlushStep') == 1, 'init failure was incorrectly marked clean'
    assert sorted(tx['write'] for tx in board.transactions) == [[0x48, 0x01], [0x68, 0], [0x6A, 0], [0x6C, 0], [0x6E, 0]]
    board.transactions.clear()
    assert board.call('TM1650_FlushStep') == 0
    assert not board.transactions


def test_flush_and_refresh_retry_failures(image):
    for method in ['TM1650_Flush', 'TM1650_Refresh']:
        board = Board(image, release_on_ack_rise=False)
        board.ready()
        assert board.call('TM1650_SetDisplay', 0x34) == 1
        board.transactions.clear()
        board.call('TM1650_SetDigit', 0, 0x15)
        board.call('TM1650_SetDigit', 1, 0x21)
        board.ack_queue = [False] * 20
        board.call(method)
        board.ack_queue = []
        board.transactions.clear()
        for _ in range(5):
            board.call('TM1650_FlushStep')
        addresses = {tx['write'][0] for tx in board.transactions}
        assert {0x68, 0x6A} <= addresses, f'{method} discarded failed digits: {board.transactions}'
        if method == 'TM1650_Refresh':
            assert [0x48, 0x34] in [tx['write'] for tx in board.transactions], 'refresh lost requested standby/brightness state'


def test_multiple_dirty_steps_and_shadow_bits(image):
    board = Board(image)
    board.ready()
    board.call('TM1650_SetDigit', 0, 0x81)
    board.call('TM1650_SetBit', 0, 0, 0)  # Clear A while preserving DP.
    board.call('TM1650_SetBit', 1, 1, 0)
    assert board.call('TM1650_GetDigit', 0) == 0x80
    assert board.call('TM1650_GetDigit', 1) == 0
    board.call('TM1650_SetDigit', 1, 0x02)
    for _ in range(2):
        before = len(board.transactions)
        assert board.call('TM1650_FlushStep') == 1
        assert len(board.transactions) == before + 1, 'step wrote more than one digit'
    assert sorted(tx['write'] for tx in board.transactions) == [[0x68, 0x80], [0x6A, 0x02]]
    board.transactions.clear()
    board.call('TM1650_Clear')
    board.call('TM1650_Flush')
    assert sorted(tx['write'] for tx in board.transactions) == [[0x68, 0], [0x6A, 0]]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--case', action='append', help='Run named test(s) only')
    args = parser.parse_args()
    image = build()
    failures = []
    tests = [(name, func) for name, func in globals().items() if name.startswith('test_') and callable(func)]
    for name, func in tests:
        if args.case and name not in args.case:
            continue
        try:
            func(image)
            print(f'PASS: {name}')
        except AssertionError as error:
            failures.append(f'{name}: {error}')
            print(f'FAIL: {failures[-1]}')
    if failures:
        raise SystemExit(f'{len(failures)} TM1650 regression failures')
    print('PASS: real ARM driver and GPIO/RCC HAL; edge-level slave, no physical bench test')


if __name__ == '__main__':
    main()
