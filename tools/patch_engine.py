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
    change(path, '#include <Carbon/Carbon.h>',
           '#if !defined(SOURCE_IOS)\n#include <Carbon/Carbon.h>\n#endif')
path='appframework/sdlmgr.cpp'
file=root/path
text=file.read_text(encoding='utf-8')
if '// GModPort iOS native GLES fallback' not in text:
    text=text.replace('#ifdef TOGLES\n#include <EGL/egl.h>', '#if defined(TOGLES) && !defined(SOURCE_IOS)\n#include <EGL/egl.h>')
    text=text.replace('#if defined ANDROID || defined TOGLES', '#if (defined(ANDROID) || defined(TOGLES)) && !defined(SOURCE_IOS)')
    before='#ifdef TOGLES\n\tl_egl = dlopen("libEGL.so", RTLD_LAZY);'
    after='''#if defined(SOURCE_IOS)
    // GModPort iOS native GLES fallback: SDL/UIKit owns the EAGL context.
    // Use SDL_GL_GetProcAddress, never Android libEGL.so/libGLESv3.so.
    SET_GL_ATTR(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SET_GL_ATTR(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SET_GL_ATTR(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(TOGLES)
\tl_egl = dlopen("libEGL.so", RTLD_LAZY);'''
    assert text.count(before)==1
    text=text.replace(before,after)
    file.write_text(text,encoding='utf-8',newline='\n')
    changes.append(path)
print(json.dumps({'engine_commit':pin,'platform_files_patched':changes,'game_code_modified':False},indent=2))
