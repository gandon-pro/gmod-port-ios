"""Apply narrowly scoped iOS platform changes to the disposable deps checkout.

Never modifies game/client/gmod, game/server/gmod or game/shared/gmod.
"""
import argparse
import json
from pathlib import Path
import re
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
signature='void FileOpenDialog::OnOpenInExplorer()\n{'
change('vgui2/vgui_controls/FileOpenDialog.cpp', signature,
       '#if defined(SOURCE_IOS)\nextern void GModPort_OpenFolder(const char* path);\n#endif\n'+signature)
change('vgui2/vgui_controls/FileOpenDialog.cpp',
       '#elif defined( OSX )\n\tchar szCmd[ MAX_PATH * 2];',
       '#elif defined(SOURCE_IOS)\n\tGModPort_OpenFolder(pCurrentDirectory);\n#elif defined( OSX )\n\tchar szCmd[ MAX_PATH * 2];')

path='vgui2/src/system_posix.cpp'
file=root/path
text=file.read_text(encoding='utf-8')
updated=text.replace('#ifdef OSX', '#if defined(OSX) && !defined(SOURCE_IOS)')
if updated != text:
    file.write_text(updated,encoding='utf-8',newline='\n')
    changes.append(path)
change(path, '#include "SDL_error.h"', '#include "SDL_error.h"\n#include "SDL_misc.h"')
change(path, '#if defined(OSX) && !defined(SOURCE_IOS)\n\tconst char *szCommand = "open";',
       '#if defined(SOURCE_IOS)\n\tif (SDL_OpenURL(file) != 0)\n\t\tWarning("Open URL failed: %s\\n", SDL_GetError());\n#else\n#if defined(OSX) && !defined(SOURCE_IOS)\n\tconst char *szCommand = "open";')
change(path, '\t\tAssert( !"execlp failed" );\n\t}\n}',
       '\t\tAssert( !"execlp failed" );\n\t}\n#endif // SOURCE_IOS: UIKit URL handler, no fork/exec\n}')

signature='void\tCGLMFileMirror::OpenInEditor( bool foreground )\n{'
change('togles/linuxwin/glmgrbasics.cpp', signature,
       signature+'\n#if defined(SOURCE_IOS)\n\textern void GModPort_RevealFile(const char* path);\n\tGModPort_RevealFile(m_path);\n#else')
change('togles/linuxwin/glmgrbasics.cpp', '\tsystem( temp );\n}',
       '\tsystem( temp );\n#endif // SOURCE_IOS\n}')

# The iOS source list builds SDL audio, not the Mac AudioQueue recorder or
# desktop hardware mixer controls. Select those same SDL/generic branches.
path='engine/audio/voice.cpp'
change(path, '#elif defined( OSX )', '#elif defined(OSX) && !defined(SOURCE_IOS)')
change(path, '#ifdef OSX', '#if defined(OSX) && !defined(SOURCE_IOS)')
path='engine/audio/voice_mixer_controls_openal.cpp'
change(path, '#ifdef OSX', '#if defined(OSX) && !defined(SOURCE_IOS)')
change(path, '#ifndef OSX', '#if !defined(OSX) || defined(SOURCE_IOS)')
change('engine/sys_dll.cpp', '#include <Carbon/Carbon.h>',
       '#if defined(SOURCE_IOS)\n#include <malloc/malloc.h>\n#else\n#include <Carbon/Carbon.h>\n#endif')
change('engine/sys_dll.cpp', '#elif OSX\n\tstruct mstats memstats = mstats( );',
       '#elif defined(SOURCE_IOS)\n\tmalloc_statistics_t memstats;\n\tmalloc_zone_statistics(malloc_default_zone(), &memstats);\n\tMsg("Allocated %.2f MB, Used: %.2f MB, #mallocs = %lu\\n",\n\t\tmemstats.size_allocated / (1024.0 * 1024.0),\n\t\tmemstats.size_in_use / (1024.0 * 1024.0), (unsigned long)memstats.blocks_in_use);\n#elif OSX\n\tstruct mstats memstats = mstats( );')
for path, call in [
    ('engine/sv_steamauth.cpp', 'SteamAPI_SetBreakpadAppID( GetSteamInfIDVersionInfo().ServerAppID );'),
    ('engine/sys_dll2.cpp', 'SteamAPI_SetBreakpadAppID( AppId + 1 );')]:
    change(path, call, '#if defined(USE_BREAKPAD_HANDLER)\n'+call+'\n#endif')
change('engine/sys_mainwind.cpp', '#ifdef OSX\n\tid nsWindow',
       '#if defined(SOURCE_IOS)\n\treturn (void*)pInfo.info.uikit.window;\n#elif defined(OSX)\n\tid nsWindow')
change('launcher/launcher.cpp', '\t\t\t\tsystem( szOpenLine );',
       '#if defined(SOURCE_IOS)\n\t\t\t\tSDL_OpenURL(szCmd);\n#else\n\t\t\t\tsystem( szOpenLine );\n#endif')
change('materialsystem/shaderapidx9/shaderdevicedx8.cpp',
       '#include <Carbon/Carbon.h>',
       '#if !defined(SOURCE_IOS)\n#include <Carbon/Carbon.h>\n#endif')

# OSX is retained for Darwin's low-level platform implementation. Within ToGLES,
# however, it selects desktop CGL, NSOpenGL and obsolete Mac GL workarounds.
# iOS uses the renderer's existing SDL/GLES paths instead. Keep the broader
# POSIX selection (BSD || OSX || LINUX) and the GLX exclusion unchanged.
renderer_files=[root/'appframework/sdlmgr.cpp']
for folder in ['public/togles', 'public/togl', 'togles']:
    renderer_files.extend(sorted((root/folder).rglob('*')))
for file in renderer_files:
        if file.suffix not in ('.h', '.cpp', '.inl'): continue
        text=file.read_text(encoding='utf-8')
        lines=[]
        for line in text.splitlines(keepends=True):
            if 'SOURCE_IOS' not in line:
                line=re.sub(r'^(\s*)#ifdef\s+OSX\b',
                            r'\1#if defined(OSX) && !defined(SOURCE_IOS)', line)
                line=re.sub(r'^(\s*)#ifndef\s+OSX\b',
                            r'\1#if !defined(OSX) || defined(SOURCE_IOS)', line)
                if 'SOURCE_IOS' not in line and re.match(r'^\s*#if\s+defined\s*\(\s*OSX\s*\)', line):
                    line=re.sub(r'defined\s*\(\s*OSX\s*\)',
                                '(defined(OSX) && !defined(SOURCE_IOS))', line, count=1)
                elif 'SOURCE_IOS' not in line and re.match(r'^\s*#if\s+!defined\s*\(\s*OSX\s*\)', line):
                    line=re.sub(r'!defined\s*\(\s*OSX\s*\)',
                                '(!defined(OSX) || defined(SOURCE_IOS))', line, count=1)
            lines.append(line)
        updated=''.join(lines)
        if updated != text:
            file.write_text(updated,encoding='utf-8',newline='\n')
            changes.append(file.relative_to(root).as_posix())

# SDL_opengl.h intentionally declares no desktop GL types on iOS. ToGLES uses
# Source's common/GL compatibility declarations (as on Android), resolving its
# entry points from the actual GLES context rather than linking desktop GL.
files=[root/'appframework/sdlmgr.cpp']
for folder in ['public/togles', 'public/togl']:
    files.extend((root/folder).rglob('*.h'))
for file in files:
    text=file.read_text(encoding='utf-8')
    marker='// GModPort: ToGLES GL compatibility declarations on iOS'
    before='#include "SDL_opengl.h"'
    if before in text and marker not in text:
        after=marker+'\n#if defined(SOURCE_IOS)\n#include <GL/gl.h>\n#include <GL/glext.h>\n#else\n'+before+'\n#endif'
        file.write_text(text.replace(before,after),encoding='utf-8',newline='\n')
        changes.append(file.relative_to(root).as_posix())
print(json.dumps({'engine_commit':pin,'platform_files_patched':changes,'game_code_modified':False},indent=2))
