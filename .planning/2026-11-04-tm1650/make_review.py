from pathlib import Path
import difflib
root=Path(__file__).resolve().parents[2]
task=root/'.planning/2026-11-04-tm1650'
files=['USER/board.c','USER/inc/board.h','USER/led.c','USER/inc/led.h','USER/main.c',
       'USER/user_control.c','USER/flash_data_save.c','USER/inc/flash_data_save.h',
       'USER/inc/parameter.h','USER/tool_mode.c','USER/inc/tool_mode.h',
       'TM1650/tm1650.c','TM1650/tm1650.h','KEIL_PRJ/MM32SPIN0230_Demo.uvprojx']
def read(p):
    if not p.exists(): return ''
    b=p.read_bytes()
    try: return b.decode('utf-8')
    except UnicodeDecodeError: return b.decode('gb18030',errors='replace')
report=[]
for name in files:
    report.extend(difflib.unified_diff(read(task/'original'/name).splitlines(True),read(root/name).splitlines(True),
                  fromfile='original/'+name,tofile=name,n=8))
(task/'review-diff.txt').write_text(''.join(report),encoding='utf-8')
print('Wrote review-diff.txt for 14 product files, including new modules.')
