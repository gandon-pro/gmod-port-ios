"""Type-check actual patched GameUI units with the iPhone SDK and exported Waf flags."""
import json
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
engine=root/'deps/engine'
with tempfile.TemporaryDirectory() as tmp:
    manifest=Path(tmp)/'source.json'
    subprocess.run(['python3',str(root/'tools/export_engine.py'),'--engine',str(engine),'--sdl-include',str(root/'deps/sdl/include'),'--output',str(manifest)],check=True)
    data=json.loads(manifest.read_text())
    projects=data['projects'] if isinstance(data,dict) else data
    module=next(p for p in projects if p['folder']=='gameui')
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    flags=['xcrun','--sdk','iphoneos','clang++','-std=c++17','-fsyntax-only','-target','arm64-apple-ios15.0','-isysroot',sdk,
           '-fno-strict-aliasing','-fsigned-char','-Wno-register','-Wno-c++11-narrowing','-Wno-deprecated-declarations',
           '-Wno-inconsistent-missing-override']
    for inc in module['includes']+[str(engine/'common'),str(root/'build-gluavm/ios-arm64/include')]:flags+=['-I',inc]
    for define in module['defines']:
        if define not in ('PROTECTED_THINGS_ENABLE','USE_BREAKPAD_HANDLER'):flags+=['-D'+define]
    for file in ('gameui/BasePanel.cpp','gameui/GameUI_Interface.cpp'):
        print('Checking',file,flush=True)
        subprocess.run(flags+[str(engine/file)],check=True)
