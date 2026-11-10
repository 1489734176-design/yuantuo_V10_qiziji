"""Run numbered cases independently so a preexisting failure does not hide another."""
import sys
from run_control_tests import runner

cases = [int(case) for case in sys.argv[1:]] or list(range(35, 47))
failures = []
for case in cases:
    sys.argv = [__file__, '--case', str(case)]
    try:
        runner.main()
    except AssertionError as error:
        failures.append(str(error))
        print(f'FAIL: {error}')
if failures:
    raise SystemExit(1)
