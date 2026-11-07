from pathlib import Path
import plistlib,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from check_app_icons import validate_icons
root=Path(__file__).resolve().parents[2]
validate_icons(root/'app/icons',plistlib.loads((root/'app/Info.plist.in').read_bytes()))
print('PASS: iPhone/iPad icon entries, required dimensions and opaque RGB PNGs')
