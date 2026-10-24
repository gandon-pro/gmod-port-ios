"""Apply narrowly scoped iOS platform changes to the disposable deps checkout.

Never modifies game/client/gmod, game/server/gmod or game/shared/gmod.
"""
import argparse
import json
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('engine',type=Path)
args=parser.parse_args()
root=args.engine.resolve()
pin='ed8209cc35c61fbd8ddff8480962a01c981eef2f'
assert subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==pin
changes=[]
def change(path,before,after):
    assert not path.startswith('game/')
    file=root/path
    text=file.read_text(encoding='utf-8')
    if after in text:return
    if text.count(before)!=1:raise SystemExit('Unexpected source context: '+path)
    file.write_text(text.replace(before,after),encoding='utf-8',newline='\n')
    changes.append(path)

signature='CSysModule *CBaseFileSystem::LoadModule( const char *pFileName, const char *pPathID, bool bValidatedDllOnly )\n{'
change('filesystem/basefilesystem.cpp',signature,signature+'\n#if defined(SOURCE_IOS)\n\t// Static modules do not exist as files in Documents/bin.\n\treturn Sys_LoadModule(pFileName);\n#endif\n')
for path in ['vguimatsurface/MatSystemSurface.cpp','vguimatsurface/FontTextureCache.cpp']:
    file=root/path
    text=file.read_text(encoding='utf-8')
    text=text.replace('#include <Carbon/Carbon.h>','#if !defined(SOURCE_IOS)\n#include <Carbon/Carbon.h>\n#endif')
    file.write_text(text,encoding='utf-8',newline='\n')
    changes.append(path)
print(json.dumps({'engine_commit':pin,'platform_files_patched':changes,'game_code_modified':False},indent=2))
