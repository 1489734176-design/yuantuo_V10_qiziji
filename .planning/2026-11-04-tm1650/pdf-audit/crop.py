from pathlib import Path
from PIL import Image
out=Path(__file__).parent
for key in ['old','new']:
    img=Image.open(out/f'{key}-1.png')
    print(key, img.size)
    w,h=img.size
    crops={
        'mcu': (0.08*w,0.30*h,0.32*w,0.62*h),
        'io': (0.02*w,0.64*h,0.45*w,0.99*h),
        'tm': (0.65*w,0.48*h,0.98*w,0.78*h),
        'power': (0.025*w,0.125*h,0.56*w,0.32*h),
        'adc': (0.40*w,0.825*h,0.97*w,0.985*h),
        'current': ((0.415 if key=='new' else 0.460)*w,(0.53 if key=='new' else 0.535)*h,(0.57 if key=='new' else 0.65)*w,(0.695 if key=='new' else 0.705)*h),
    }
    for name,box in crops.items():
        img.crop(tuple(map(int,box))).resize((int((box[2]-box[0])*2),int((box[3]-box[1])*2))).save(out/f'{key}-{name}.png')
