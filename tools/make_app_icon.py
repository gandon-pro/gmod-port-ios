"""Generate the port's blue/white g icon at the iPhone/iPad bundle sizes."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import argparse

def generate(out):
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    fontfile=next((Path(p) for p in ['/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf','/System/Library/Fonts/Supplemental/Arial Bold.ttf','C:/Windows/Fonts/arialbd.ttf'] if Path(p).is_file()),None)
    if fontfile is None:raise SystemExit('Install DejaVu Sans Bold or Arial Bold to regenerate icons')
    image=Image.new('RGB',(1024,1024),(0,149,255)); d=ImageDraw.Draw(image)
    font=ImageFont.truetype(str(fontfile),930)
    box=d.textbbox((0,0),'g',font=font)
    d.text(((1024-box[2]+box[0])/2-box[0],(1024-box[3]+box[1])/2-box[1]-20),'g',font=font,fill='white')
    for name,size in [('AppIcon60@2x',120),('AppIcon60@3x',180),('AppIcon76',76),('AppIcon76@2x',152),('AppIcon83.5@2x',167),('AppIcon1024',1024)]:
        image.resize((size,size),Image.Resampling.LANCZOS).save(out/(name+'.png'))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('output',nargs='?',default=str(Path(__file__).resolve().parents[1]/'app/icons'));generate(p.parse_args().output)
