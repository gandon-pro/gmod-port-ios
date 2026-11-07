"""Validate legacy iPhone/iPad app icons using only Python's standard library."""
from pathlib import Path
import struct
SIZES={'AppIcon60@2x.png':120,'AppIcon60@3x.png':180,'AppIcon76.png':76,'AppIcon76@2x.png':152,'AppIcon83.5@2x.png':167,'AppIcon1024.png':1024}
def validate_icons(folder,info):
    for key in ('CFBundleIcons','CFBundleIcons~ipad'):
        primary=info.get(key,{}).get('CFBundlePrimaryIcon',{})
        if not primary.get('CFBundleIconFiles'):raise ValueError('Missing '+key)
    for name,side in SIZES.items():
        data=(Path(folder)/name).read_bytes()
        if data[:8]!=b'\x89PNG\r\n\x1a\n' or struct.unpack('>II',data[16:24])!=(side,side):raise ValueError('Invalid icon: '+name)
        if data[25]!=2:raise ValueError('App icon must be RGB without alpha: '+name)
