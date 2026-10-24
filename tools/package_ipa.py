import argparse
from pathlib import Path
import plistlib
import subprocess
import zipfile

parser=argparse.ArgumentParser()
parser.add_argument('--app',required=True,type=Path)
parser.add_argument('--output',required=True,type=Path)
args=parser.parse_args()
app=args.app.resolve()
info=plistlib.loads((app/'Info.plist').read_bytes())
binary=app/info['CFBundleExecutable']
subprocess.run(['xcrun','lipo','-info',str(binary)],check=True)
subprocess.run(['xcrun','lipo','-verify_arch','arm64',str(binary)],check=True)
assert info['UIFileSharingEnabled'] is True
assert not list(app.rglob('libclient.dylib'))
args.output.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(args.output,'w',compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(app.rglob('*')):
        if path.is_file():archive.write(path,Path('Payload')/app.name/path.relative_to(app))
print(args.output, args.output.stat().st_size)
