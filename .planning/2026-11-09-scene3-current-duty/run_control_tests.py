"""Reuse the existing ARM test runner with this task's isolated output directory."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('scene3_control_runner', ROOT / 'tests/tool_mode/run_tests.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
runner.OUT = Path(__file__).resolve().parent / 'control-tests'

if __name__ == '__main__':
    runner.main()
