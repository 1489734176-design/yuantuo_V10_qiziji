"""Run existing ARM regression suites with isolated outputs for this iteration."""
import importlib.util
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.planning/2026-11-05-mode-fixes/trigger-led-timing-tests'
SUITES = {
    'seven_led': 'tests/seven_led/run_tests.py',
    'tool_mode': 'tests/tool_mode/run_tests.py',
    'tm1650': 'tests/tm1650/run_tests.py',
    'integration': 'tests/tm1650/run_mode_integration.py',
    'battery_com': 'tests/battery_com/run_tests.py',
}

name = sys.argv[1]
path = ROOT / SUITES[name]
spec = importlib.util.spec_from_file_location('isolated_' + name, path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.OUT = OUT / name
sys.argv = [str(path)]
if name == 'battery_com':
    sys.argv += ['--keil', 'D:/Users/mym02/AppData/Local/Keil_v5',
                 '--build-dir', str(OUT / name)]
module.main()
