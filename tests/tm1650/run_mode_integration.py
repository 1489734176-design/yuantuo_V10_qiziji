"""Exercise real TM1650 key reads, debounce, modes and LED writes together.

The existing edge-level slave models the physical bus; no key-read or display
API is mocked. Board K3 is DIG3/A-KI1 (0x46), K2 is DIG4/A-KI1 (0x47).
Display expectations use the user-confirmed board colors and speed order.
"""
import importlib.util
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.planning/2026-11-05-reverse-scene-memory/integration-tests'
spec = importlib.util.spec_from_file_location('tm1650_bus_runner', ROOT / 'tests/tm1650/run_tests.py')
bus = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bus)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    image = OUT / 'mode_integration.axf'
    command = [str(bus.KEIL / 'ARM/ARMCLANG/bin/armclang.exe'), '--target=arm-arm-none-eabi',
               '-mcpu=cortex-m0', '-std=c99', '-O1', '-g', '-fshort-enums',
               '-ffunction-sections', '-fdata-sections', '-DUSE_STDPERIPH_DRIVER', '-DUSE_5V',
               '-Wall', '-Wextra', '-Werror', '-Wno-invalid-source-encoding',
               '-Wno-nonportable-include-path',
               '-Wl,--ro-base=0x08000000,--rw-base=0x20000000,--entry=test_mode_init,--no_remove',
               '-o', str(image), str(ROOT / 'tests/tm1650/mode_integration_support.c'),
               str(ROOT / 'TM1650/tm1650.c'), str(ROOT / 'USER/tool_mode.c'),
               str(ROOT / 'USER/led.c'),
               str(ROOT / 'MM32SPIN0230/HAL_Lib/src/hal_gpio.c'),
               str(ROOT / 'MM32SPIN0230/HAL_Lib/src/hal_rcc.c')]
    for folder in ['USER/inc', 'TM1650', 'MOTOR_CONTROL', 'MC_CORE', 'DRIVE/inc', 'SYSTEM/inc',
                   'MM32SPIN0230/Include', 'MM32SPIN0230/HAL_Lib/inc', 'CMSIS/Core/Include']:
        command += ['-I', str(ROOT / folder)]
    subprocess.run(command, check=True, cwd=ROOT)
    return image


class ModeBoard(bus.Board):
    def __init__(self, image):
        self.digits = [None] * 4
        super().__init__(image)
        self.ready()
        self.call('test_mode_init')

    def gpio_write(self, *args):
        before = len(self.transactions)
        super().gpio_write(*args)
        if len(self.transactions) > before:
            tx = self.transactions[-1]
            if len(tx['write']) == 2 and all(tx['write_acks']):
                address, data = tx['write']
                if address in (0x68, 0x6a, 0x6c, 0x6e):
                    self.digits[(address - 0x68) // 2] = data

    def state(self):
        value = self.call('test_mode_state')
        return value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff

    def key_ms(self, raw, milliseconds):
        self.key = raw
        before = len(self.transactions)
        for _ in range(milliseconds):
            self.call('tool_mode_key_handle')
        reads = self.transactions[before:]
        assert all(tx['write'] == [0x49] for tx in reads), 'key task performed a display write'
        expected_bits = [(raw >> shift) & 1 for shift in range(7, -1, -1)]
        for tx in reads:
            assert tx['read'] == expected_bits, f'key bits differ from physical raw {raw:#x}'
            assert tx['nack'] == 1 and tx['stop'], 'key transaction omitted NACK/STOP'
        return len(reads)

    def press(self, raw):
        assert self.key_ms(0, 30) == 3
        assert self.key_ms(raw, 30) == 3

    def display(self, expected):
        self.call('UO_Led_Handler')
        shadow = [self.call('TM1650_GetDigit', i) for i in range(4)]
        assert shadow == expected, f'display shadow {shadow} != {expected}; state={self.state()}'
        for _ in range(8):
            before = len(self.transactions)
            self.call('led_tm1650_handle')
            assert len(self.transactions) - before <= 1, 'display task used multiple transactions'
        assert self.digits == expected, f'physical display RAM {self.digits} != {expected}'


def test_physical_k3_to_scene_display(image):
    board = ModeBoard(image)
    assert board.state() == (0, 1, 1)
    assert board.key_ms(0x46, 29) == 2
    assert board.state() == (0, 1, 1), 'K3 activated before three stable samples'
    assert board.key_ms(0x46, 1) == 1
    assert board.state() == (1, 1, 1), 'physical K3 did not enter SCENE'
    board.display([0, 0x40, 0, 0])
    assert board.key_ms(0x46, 200) == 20
    assert board.state() == (1, 1, 1), 'holding K3 repeated the event'
    board.press(0x46)
    assert board.state() == (1, 1, 2)
    board.display([0, 0x20, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 1, 3)
    board.display([0, 0x10, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 1, 1)
    board.display([0, 0x40, 0, 0])


def test_physical_k2_speed_order_and_mode_memory(image):
    """验证K2累计点亮调速灯，并保留同方向的模式档位记忆。"""
    board = ModeBoard(image)
    board.display([0x40, 0, 0, 0])
    for gear, mask in [(2, 0x60), (3, 0x70), (4, 0x78), (1, 0x40), (2, 0x60)]:
        board.press(0x47)
        assert board.state() == (0, gear, 1)
        board.display([mask, 0, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 2, 1), 'switching modes changed remembered speed gear'
    board.display([0, 0x40, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 2, 2)
    board.display([0, 0x20, 0, 0])
    board.press(0x47)
    assert board.state() == (0, 2, 2), 'K2 failed to restore the speed gear'
    board.display([0x60, 0, 0, 0])
    board.call('test_power_down')
    board.display([0, 0, 0, 0])
    assert board.state() == (0, 2, 2), 'power-down changed mode memory'


def test_physical_keys_inhibited_by_trigger(image):
    board = ModeBoard(image)
    board.call('test_trigger', 1)
    board.key_ms(0x46, 100)
    assert board.state() == (0, 1, 1), 'K3 changed mode while trigger was pressed'
    board.call('test_trigger', 0)
    board.key_ms(0x46, 100)
    assert board.state() == (0, 1, 1), 'held K3 replayed after trigger release'
    board.press(0x46)
    assert board.state() == (1, 1, 1)
    board.display([0, 0x40, 0, 0])
    board.call('test_trigger', 1)
    board.key_ms(0x47, 100)
    assert board.state() == (1, 1, 1), 'K2 changed mode while trigger was pressed'
    board.call('test_trigger', 0)
    board.key_ms(0x47, 100)
    assert board.state() == (1, 1, 1), 'held K2 replayed after trigger release'
    board.press(0x47)
    assert board.state() == (0, 1, 1)
    board.display([0x40, 0, 0, 0])
    board.key_ms(0x47, 20)
    board.call('test_trigger', 1)
    board.key_ms(0x47, 1)
    board.call('test_trigger', 0)
    board.key_ms(0x47, 100)
    assert board.state() == (0, 1, 1), 'partial key debounce survived a trigger press'
    board.press(0x47)
    assert board.state() == (0, 2, 1)


def test_physical_reverse_k3_led5_and_direction_memory(image):
    """验证反转K3情景单灯、双调速交接，以及涉及情景时保留原方向记忆。"""
    board = ModeBoard(image)
    board.call('test_mode_direction', 1)
    assert board.state() == (0, 1, 4)
    board.display([0x40, 0, 0, 0])
    board.press(0x47)
    assert board.state() == (0, 2, 4)
    board.display([0x60, 0, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 2, 4), 'reverse K3 did not select fixed scene4'
    board.display([0, 0x08, 0, 0])
    board.key_ms(0x46, 200)
    assert board.state() == (1, 2, 4), 'held reverse K3 repeated the toggle'
    board.press(0x46)
    assert board.state() == (0, 2, 4), 'reverse K3 did not restore remembered speed2'
    board.display([0x60, 0, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 2, 4)
    board.display([0, 0x08, 0, 0])
    board.press(0x47)
    assert board.state() == (0, 2, 4), 'reverse K2 changed remembered gear when leaving scene'
    board.call('test_mode_direction', 0)
    assert board.state() == (0, 2, 1), 'forward speed did not inherit the leaving reverse gear'
    board.display([0x60, 0, 0, 0])
    board.press(0x46)
    board.press(0x46)
    assert board.state() == (1, 2, 2)
    board.display([0, 0x20, 0, 0])
    board.call('test_mode_direction', 1)
    assert board.state() == (0, 2, 4), 'reverse remembered speed2 was lost'
    board.press(0x46)
    board.display([0, 0x08, 0, 0])
    board.call('test_trigger', 1)
    board.key_ms(0x46, 100)
    assert board.state() == (1, 2, 4), 'reverse K3 toggled while trigger pressed'
    board.call('test_trigger', 0)
    board.key_ms(0x46, 100)
    assert board.state() == (1, 2, 4), 'held reverse K3 replayed after trigger release'
    board.press(0x46)
    assert board.state() == (0, 2, 4)
    board.display([0x60, 0, 0, 0])
    board.call('test_mode_direction', 0)
    assert board.state() == (1, 2, 2), 'forward scene2 was lost after reverse toggles'
    board.display([0, 0x20, 0, 0])


def test_physical_speed_direction_handoff_and_scene_memory(image):
    """验证双调速往返沿用当前档位，任一方向为情景时各自保留档位和单灯。"""
    board = ModeBoard(image)
    board.press(0x47)
    board.press(0x47)
    assert board.state() == (0, 3, 1)
    board.display([0x70, 0, 0, 0])
    board.call('test_mode_direction', 1)
    assert board.state() == (0, 3, 4), 'reverse speed did not inherit forward speed3'
    board.display([0x70, 0, 0, 0])
    board.press(0x47)
    assert board.state() == (0, 4, 4)
    board.display([0x78, 0, 0, 0])
    board.call('test_mode_direction', 0)
    assert board.state() == (0, 4, 1), 'forward speed did not inherit reverse speed4'
    board.display([0x78, 0, 0, 0])
    board.press(0x46)
    board.press(0x46)
    assert board.state() == (1, 4, 2)
    board.display([0, 0x20, 0, 0])
    board.call('test_mode_direction', 1)
    board.press(0x47)
    assert board.state() == (0, 1, 4)
    board.display([0x40, 0, 0, 0])
    board.call('test_mode_direction', 0)
    assert board.state() == (1, 4, 2), 'speed-to-scene switch overwrote forward speed4 or scene2'
    board.display([0, 0x20, 0, 0])
    board.call('test_mode_direction', 1)
    assert board.state() == (0, 1, 4), 'scene-to-speed switch overwrote reverse speed1'
    board.display([0x40, 0, 0, 0])
    board.press(0x46)
    assert board.state() == (1, 1, 4)
    board.display([0, 0x08, 0, 0])
    board.call('test_mode_direction', 0)
    assert board.state() == (1, 4, 2), 'scene-to-scene switch overwrote forward memory'
    board.display([0, 0x20, 0, 0])
    board.press(0x47)
    assert board.state() == (0, 4, 2)
    board.display([0x78, 0, 0, 0])
    board.call('test_mode_direction', 1)
    assert board.state() == (1, 1, 4), 'speed-to-reverse-scene switch overwrote reverse memory'
    board.display([0, 0x08, 0, 0])


def main():
    """编译真实模式显示代码并运行五组总线级行为回归。"""
    image = build()
    failures = []
    for test in [test_physical_k3_to_scene_display, test_physical_k2_speed_order_and_mode_memory,
                 test_physical_keys_inhibited_by_trigger,
                 test_physical_reverse_k3_led5_and_direction_memory,
                 test_physical_speed_direction_handoff_and_scene_memory]:
        try:
            test(image)
            print(f'PASS: {test.__name__}')
        except AssertionError as error:
            failures.append(f'{test.__name__}: {error}')
            print(f'FAIL: {failures[-1]}')
    if failures:
        raise SystemExit(f'{len(failures)} mode integration regression failures')
    print('PASS: real ARM TM1650/debounce/mode/LED pipeline, edge-level slave; not a bench test')


if __name__ == '__main__':
    main()
