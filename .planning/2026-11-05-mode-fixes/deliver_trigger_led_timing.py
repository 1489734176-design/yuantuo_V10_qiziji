"""Verify the Keil HEX/BIN and deliver this iteration without older-output churn."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / '.planning/2026-11-05-mode-fixes/trigger-led-timing-build'
DEST = ROOT / 'output/firmware/trigger-led-timing-20261105'

memory = {}
base = 0
eof = False
for line in (BUILD / 'Objects/SPIN0230_DEMO.hex').read_text(encoding='ascii').splitlines():
    assert line.startswith(':'), 'Malformed Intel HEX line'
    record = bytes.fromhex(line[1:])
    assert len(record) == record[0] + 5 and sum(record) % 256 == 0, 'HEX checksum/length failed'
    length, address, kind = record[0], int.from_bytes(record[1:3], 'big'), record[3]
    if kind == 0:
        for i, value in enumerate(record[4:4 + length]):
            offset = base + address + i
            assert offset not in memory, 'Overlapping HEX records'
            memory[offset] = value
    elif kind == 4:
        base = int.from_bytes(record[4:6], 'big') << 16
    elif kind == 2:
        base = int.from_bytes(record[4:6], 'big') << 4
    elif kind == 1:
        eof = True
assert eof and memory, 'Missing HEX data/EOF'
start, end = min(memory), max(memory) + 1
assert start == 0x08000000 and end <= 0x08007800, 'Image crosses parameter Flash pages'
binary = (BUILD / 'SPIN0230_DEMO.bin').read_bytes()
assert len(binary) == end - start, 'HEX/BIN size mismatch'
assert all(binary[address - start] == value for address, value in memory.items()), 'HEX/BIN bytes differ'

DEST.mkdir(parents=True, exist_ok=True)
for source, name in [
    (BUILD / 'Objects/SPIN0230_DEMO.hex', 'SPIN0230_DEMO.hex'),
    (BUILD / 'Objects/SPIN0230_DEMO.axf', 'SPIN0230_DEMO.axf'),
    (BUILD / 'SPIN0230_DEMO.bin', 'SPIN0230_DEMO.bin'),
    (BUILD / 'rebuild.log', 'rebuild.log'),
]:
    shutil.copy2(source, DEST / name)
    assert (DEST / name).read_bytes() == source.read_bytes(), 'Delivery copy differs'

manifest = {'flash_start': hex(start), 'flash_end_exclusive': hex(end),
            'binary_bytes': len(binary), 'artifacts': {}}
for name in ['SPIN0230_DEMO.hex', 'SPIN0230_DEMO.bin', 'SPIN0230_DEMO.axf']:
    data = (DEST / name).read_bytes()
    manifest['artifacts'][name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
(DEST / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')

# Restore only generated files overwritten by this iteration's first baseline runs.
# Subsequent verified regression outputs live in trigger-led-timing-tests/.
for relative in ['.planning/2026-11-04-tm1650/board-tests/seven_led_tests.axf',
                 '.planning/2026-11-04-tm1650/board-tests/test_main.c',
                 '.planning/2026-11-04-tm1650/control-tests/tool_mode_tests.axf']:
    data = subprocess.run(['git', 'show', 'HEAD:' + relative], cwd=ROOT,
                          check=True, stdout=subprocess.PIPE).stdout
    (ROOT / relative).write_bytes(data)
print(json.dumps(manifest, indent=2))
print('Verified HEX checksums, HEX/BIN agreement, parameter Flash boundary, and delivered copies.')
