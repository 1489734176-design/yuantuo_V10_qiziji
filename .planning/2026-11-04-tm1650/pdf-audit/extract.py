from pathlib import Path
from pypdf import PdfReader

out = Path(__file__).parent
sources = {
    'old': Path(r'D:\mym\NXN\9.henbangtai\V25_50Adianzuan\资料\NX32F002-28T-方灯-7LED.pdf'),
    'new': Path(r'D:\mym\NXN\3.yuantuo\V10_qizhiji\NX32F002TS-飞机板-绿林\NX32F002TS-飞机板-绿林.pdf'),
    'tm1650': Path(r'D:\mym\NXN\3.yuantuo\V10_qizhiji\NX32F002TS-飞机板-绿林\TM-TM1650.pdf'),
}
for key, src in sources.items():
    reader = PdfReader(src)
    texts = [f'PAGE {i+1}\n{p.extract_text(extraction_mode="layout")}' for i,p in enumerate(reader.pages)]
    (out / f'{key}.txt').write_text('\n\n'.join(texts), encoding='utf-8')
    print(f'{key}: {len(reader.pages)} pages, {len("".join(texts))} chars')
