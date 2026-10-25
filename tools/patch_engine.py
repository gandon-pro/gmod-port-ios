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
    assert '/gmod/' not in path  # Aero's GMod layer is never patched here
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
change('engine/audio/snd_win.cpp', '#elif defined(OSX)\n', '#elif defined(OSX) && !defined(SOURCE_IOS)\n')
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
bp='\t\tSteamAPI_SetBreakpadAppID( BreakpadAppId );\n\t\tSteamAPI_UseBreakpadCrashHandler( pchVersion, __DATE__, __TIME__, bFullMemoryDump, pvMiniDumpContext, pfnPreMinidumpCallback );\n'
change('engine/sys_dll2.cpp', bp, '#if defined(USE_BREAKPAD_HANDLER)\n'+bp+'#endif\n')
change('engine/sys_mainwind.cpp', '#ifdef OSX\n\tid nsWindow',
       '#if defined(SOURCE_IOS)\n\treturn (void*)pInfo.info.uikit.window;\n#elif defined(OSX)\n\tid nsWindow')
change('launcher/launcher.cpp', '\t\t\t\tsystem( szOpenLine );',
       '#if defined(SOURCE_IOS)\n\t\t\t\tSDL_OpenURL(szCmd);\n#else\n\t\t\t\tsystem( szOpenLine );\n#endif')
# UIKit SDL loads OpenGLES.framework itself and rejects any other library path
# ("OpenGL library already loaded"); NULL means "use the loaded one".
change('appframework/sdlmgr.cpp', '#if defined( TOGLES )\n\t\tif (SDL_GL_LoadLibrary("libGLESv3.so") == -1)',
       '#if defined( TOGLES ) && !defined( SOURCE_IOS )\n\t\tif (SDL_GL_LoadLibrary("libGLESv3.so") == -1)')
# The sandbox cannot create /tmp/source_engine_*.lock and iOS runs one instance anyway.
change('launcher/launcher.cpp', '#ifdef ANDROID\n\treturn true;\n#elif defined (LINUX)',
       '#if defined(ANDROID) || defined(SOURCE_IOS)\n\treturn true;\n#elif defined (LINUX)')
# CheckParm returns the "-basedir" token itself, not its value, so the launcher would
# chdir("-basedir") and the relative -game garrysmod/gameinfo.txt lookup fails.
change('launcher/launcher.cpp', '\t\tchar const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );',
       '#if defined(SOURCE_IOS)\n\t\tchar const *pOverrideDir = CommandLine()->ParmValue( "-basedir", (const char *)NULL );\n#else\n\t\tchar const *pOverrideDir = CommandLine()->CheckParm( "-basedir" );\n#endif')
# video_services (Bink/QuickTime/WebM loaders) is not built for iOS; every g_pVideo
# user in the engine checks for NULL, so leave it out of the app system group.
change('launcher/launcher.cpp', '\t\t{ "video_services" DLL_EXT_STRING,  VIDEO_SERVICES_INTERFACE_VERSION },\n',
       '#if !defined(SOURCE_IOS)\n\t\t{ "video_services" DLL_EXT_STRING,  VIDEO_SERVICES_INTERFACE_VERSION },\n#endif\n')
# serverbrowser/friends are separate dlls that iOS does not build (static modules only),
# and a missing one is a fatal Error(). Initializing the empty module list still marks the
# platform ready, which BasePanel waits for before fading the main menu in.
change('gameui/VGuiSystemModuleLoader.cpp', '\tif ( IsX360() )\n\t{\n\t\t// not valid for 360\n\t\treturn false;\n\t}\n\n\tbool bSuccess = true;\n\n\t// load platform menu',
       '\tif ( IsX360() )\n\t{\n\t\t// not valid for 360\n\t\treturn false;\n\t}\n#if defined(SOURCE_IOS)\n\treturn InitializeAllModules(factorylist, factorycount);\n#endif\n\n\tbool bSuccess = true;\n\n\t// load platform menu')
# ImagePanel prefixes "vgui/", so "../gmp/logo" becomes "vgui/../gmp/logo": Windows resolves the
# '..' through a missing directory, POSIX and VPK lookups do not. Collapse it before the lookup.
change('vguimatsurface/TextureDictionary.cpp',
       'void CMatSystemTexture::SetMaterial( const char *pFileName )\n{\n\t// Get a pointer to the new material\n\tIMaterial *pMaterial',
       'void CMatSystemTexture::SetMaterial( const char *pFileName )\n{\n#if defined(SOURCE_IOS)\n'
       '\tchar szCollapsed[MAX_PATH];\n\tV_strncpy( szCollapsed, pFileName, sizeof( szCollapsed ) );\n'
       '\tV_FixSlashes( szCollapsed, \'/\' );\n\tV_RemoveDotSlashes( szCollapsed, \'/\' );\n\tpFileName = szCollapsed;\n#endif\n'
       '\t// Get a pointer to the new material\n\tIMaterial *pMaterial')
# Apple GLES stops at 3.0: no glDrawRangeElementsBaseVertex (ES 3.2). FlushDrawStates
# already receives the base vertex, so fold it into the vertex attrib pointer offsets
# and draw with plain glDrawRangeElements (indices are then relative to the base).
change('public/togles/linuxwin/glfuncs.h',
       'GL_FUNC_VOID(OpenGL,true,glDrawRangeElementsBaseVertex,(GLenum a,GLuint b,GLuint c,GLsizei d,GLenum e,const GLvoid *f, GLenum g),(a,b,c,d,e,f,g))\n',
       '#if !defined(SOURCE_IOS)\nGL_FUNC_VOID(OpenGL,true,glDrawRangeElementsBaseVertex,(GLenum a,GLuint b,GLuint c,GLsizei d,GLenum e,const GLvoid *f, GLenum g),(a,b,c,d,e,f,g))\n#endif\n')
draw='\n\t\tgGL->glDrawRangeElementsBaseVertex( mode, start, end, count, type, indicesActual, baseVertex );'
for path in ['public/togles/linuxwin/glmgr.h','togles/linuxwin/glmgr.cpp']:
    change(path, draw, '\n#if defined(SOURCE_IOS)\n\t\tgGL->glDrawRangeElements( mode, start, end, count, type, indicesActual ); // base vertex is in the attrib pointers\n#else'+draw+'\n#endif')
change('public/togles/linuxwin/glmgr.h', '\t\t\tuint m_nTotalBufferRevision;\n\t\t\tIDirect3DVertexDeclaration9\t*m_pVertDecl;',
       '\t\t\tuint m_nTotalBufferRevision;\n\t\t\tuint m_nBaseVertex;\n\t\t\tIDirect3DVertexDeclaration9\t*m_pVertDecl;')
change('public/togles/linuxwin/glmgr.h', '\t\t\tm_CurAttribs.m_nTotalBufferRevision = 0;\n',
       '\t\t\tm_CurAttribs.m_nTotalBufferRevision = 0;\n\t\t\tm_CurAttribs.m_nBaseVertex = 0;\n')
flush='togles/linuxwin/glmgr_flush.inl'
change(flush, '\tif ( ( nCurTotalBufferRevision != m_CurAttribs.m_nTotalBufferRevision ) ||\n',
       '\tif ( ( nCurTotalBufferRevision != m_CurAttribs.m_nTotalBufferRevision ) ||\n#if defined(SOURCE_IOS)\n\t\t( m_CurAttribs.m_nBaseVertex != nBaseVertex ) ||\n#endif\n')
change(flush, '\t\tm_CurAttribs.m_nTotalBufferRevision = nCurTotalBufferRevision;\n',
       '\t\tm_CurAttribs.m_nTotalBufferRevision = nCurTotalBufferRevision;\n\t\tm_CurAttribs.m_nBaseVertex = nBaseVertex;\n')
change(flush, '\t\t\t\tnBufOffset += pBuf->m_nPersistentBufferStartOffset;\n\t\t\t}\n',
       '\t\t\t\tnBufOffset += pBuf->m_nPersistentBufferStartOffset;\n\t\t\t}\n#if defined(SOURCE_IOS)\n\t\t\tnBufOffset += (int)( nBaseVertex * pStream->m_stride );\n#endif\n')
# Report the 3.0 context but keep going; the only 3.2 requirement is emulated above.
change('togles/linuxwin/glentrypoints.cpp',
       '\t\t\tmajor, minor, patch, NEED_MAJOR, NEED_MINOR, NEED_PATCH);\n\t\treturn false;\n',
       '\t\t\tmajor, minor, patch, NEED_MAJOR, NEED_MINOR, NEED_PATCH);\n#if !defined(SOURCE_IOS)\n\t\treturn false;\n#endif\n')
change('materialsystem/shaderapidx9/shaderdevicedx8.cpp',
       '#include <Carbon/Carbon.h>',
       '#if !defined(SOURCE_IOS)\n#include <Carbon/Carbon.h>\n#endif')
change('appframework/sdlmgr.cpp', '\tretval = SDL_GL_GetProcAddress(fn);',
       '#if defined(SOURCE_IOS)\n\textern void* GModPort_GLProcAddress(const char* name);\n\tretval = GModPort_GLProcAddress(fn);\n#else\n\tretval = SDL_GL_GetProcAddress(fn);\n#endif')

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
# hl2mp MOTD uses vgui/gfx/vgui/* art that GMod content lacks: it covers the
# screen with error textures on join (seen on the Windows dev build). Opt-in.
path='game/server/hl2mp/hl2mp_client.cpp'
change(path, 'void FinishClientPutInServer( CHL2MP_Player *pPlayer )\n{',
       'static ConVar sv_gmp_show_motd( "sv_gmp_show_motd", "0", 0, "GModPort: show the hl2mp MOTD panel on join" );\n\n'
       'void FinishClientPutInServer( CHL2MP_Player *pPlayer )\n{')
change(path, '\tconst ConVar *hostname = cvar->FindVar( "hostname" );',
       '\tif ( !sv_gmp_show_motd.GetBool() )\n\t\treturn;\n\n\tconst ConVar *hostname = cvar->FindVar( "hostname" );')
# GMod's sky materials use the g_sky shader (+ SkyPaint proxy driven by env_skypaint). Their
# $basetexture is a painted fallback that the stock sky shader draws.
change('materialsystem/stdshaders/sky_dx9.cpp', '\nEND_SHADER\n',
       "\nEND_SHADER\n\n// GModPort: GMod's procedural sky shader falls back to its painted $basetexture.\n"
       'DEFINE_FALLBACK_SHADER( g_sky, Sky_DX9 )\n')
proxy='EXPOSE_INTERFACE( CDummyMaterialProxy, IMaterialProxy, "Dummy" IMATERIAL_PROXY_INTERFACE_VERSION );'
change('game/client/dummyproxy.cpp', proxy, proxy+'''

// GModPort: GMod's skybox/painted* materials (g_sky) are driven by env_skypaint. Until that
// entity exists they draw their fallback $basetexture, so the proxy does nothing.
class CSkyPaintProxy : public IMaterialProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues ) { return true; }
	virtual void OnBind( void *pC_BaseEntity ) {}
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial() { return NULL; }
};
EXPOSE_INTERFACE( CSkyPaintProxy, IMaterialProxy, "SkyPaint" IMATERIAL_PROXY_INTERFACE_VERSION );''')

# weapon_physgun + test spawn menu (engine_src/game/shared/hl2mp/gmp_physgun.inl), included
# into weapon_physcannon.cpp for CGrabController. It replaces the HL2MP default weapon, whose
# scripts GMod content does not have.
src=Path(__file__).resolve().parent.parent/'engine_src'
for file in sorted(src.rglob('*')):
    if file.is_file():
        target=root/file.relative_to(src)
        data=file.read_bytes()
        if not target.exists() or target.read_bytes()!=data:
            target.parent.mkdir(parents=True, exist_ok=True)  # game/shared/gmod is new
            target.write_bytes(data)
            changes.append(target.relative_to(root).as_posix())
change('game/shared/hl2mp/weapon_physcannon.cpp',
       'DECLARE_CLIENT_EFFECT( "PhyscannonImpact", CallbackPhyscannonImpact );\n\n#endif\n',
       'DECLARE_CLIENT_EFFECT( "PhyscannonImpact", CallbackPhyscannonImpact );\n\n#endif\n\n'
       '// GModPort: weapon_physgun and the test spawn menu reuse CGrabController.\n#include "gmp_physgun.inl"\n')
path='game/server/hl2mp/hl2mp_player.cpp'
change(path, '\tGiveNamedItem( "weapon_physcannon" );\n\n\tconst char *szDefaultWeaponName',
       '\tGiveNamedItem( "weapon_physcannon" );\n\tGiveNamedItem( "weapon_physgun" );\n\n\tconst char *szDefaultWeaponName')
change(path, '\t\tWeapon_Switch( Weapon_OwnsThisType( "weapon_physcannon" ) );\n\t}\n}',
       '\t\tWeapon_Switch( Weapon_OwnsThisType( "weapon_physgun" ) );\n\t}\n}')
change('game/client/hl2mp/c_hl2mp_player.cpp', 'static ConVar cl_defaultweapon( "cl_defaultweapon", "weapon_physcannon"',
       'static ConVar cl_defaultweapon( "cl_defaultweapon", "weapon_physgun"')
# Visible round joystick for the _move zone (radius touch_forwardzone of the screen height).
path='game/client/touch.cpp'
change(path, '\t\t\t\tf = ( move_start_y - y ) / touch_forwardzone.GetFloat();\n'
             '\t\t\t\ts = ( move_start_x - x ) / touch_sidezone.GetFloat();\n'
             '\t\t\t\tforward = bound( -1, f, 1 );\n'
             '\t\t\t\tside = bound( -1, s, 1 );\n',
       '\t\t\t\t// GModPort: round joystick, radius = touch_forwardzone of the screen height on both axes.\n'
       '\t\t\t\tf = ( move_start_y - y ) / touch_forwardzone.GetFloat();\n'
       '\t\t\t\ts = ( move_start_x - x ) * screen_w / screen_h / touch_forwardzone.GetFloat();\n'
       '\t\t\t\tfloat len = sqrtf( f * f + s * s );\n'
       '\t\t\t\tif( len > 1.f )\n'
       '\t\t\t\t{\n'
       '\t\t\t\t\tf /= len;\n'
       '\t\t\t\t\ts /= len;\n'
       '\t\t\t\t}\n'
       '\t\t\t\tforward = f;\n'
       '\t\t\t\tside = s;\n')
change(path, '\tmeshBuilder.End();\n\tm_pMesh->Draw();\n\n\n\tif( m_flHideTouch < gpGlobals->curtime )',
       '\tmeshBuilder.End();\n\tm_pMesh->Draw();\n\n'
       '\t// GModPort: draw the _move joystick. It appears under the finger; idle it rests bottom-left.\n'
       '\tif( state != state_edit )\n'
       '\t{\n'
       '\t\tconst int radius = touch_forwardzone.GetFloat() * screen_h;\n'
       '\t\tconst bool active = move_finger != -1;\n'
       '\t\tconst int cx = active ? move_start_x * screen_w : 0.16f * screen_w;\n'
       '\t\tconst int cy = active ? move_start_y * screen_h : 0.70f * screen_h;\n'
       '\t\tconst int kx = cx - side * radius, ky = cy - forward * radius;\n'
       '\t\tvgui::surface()->DrawSetColor( 255, 255, 255, active ? 150 : 60 );\n'
       '\t\tfor( int r = radius - 2; r <= radius + 1; r++ )\n'
       '\t\t\tvgui::surface()->DrawOutlinedCircle( cx, cy, r, 64 );\n'
       '\t\tvgui::surface()->DrawSetColor( 255, 255, 255, active ? 110 : 45 );\n'
       '\t\tfor( int r = 1; r <= radius * 2 / 5; r++ )\n'
       '\t\t\tvgui::surface()->DrawOutlinedCircle( kx, ky, r, 32 );\n'
       '\t}\n\n'
       '\tif( m_flHideTouch < gpGlobals->curtime )')
# 1920x1080 backbuffer (-w/-h) is blitted onto a window measured in points; SDL touch->mouse
# events arrive in points too, so vgui needs them scaled to the backbuffer. Windowed mode
# leaves m_flMouseXScale at 1.0.
for axis in ('motion', 'button'):
    before='\t\t\t\ttheEvent.m_MousePos[0] = event.%s.x * m_flMouseXScale;' % axis
    change('appframework/sdlmgr.cpp', before,
           '#if defined(SOURCE_IOS)\n'
           '\t\t\t\t{\n'
           '\t\t\t\t\tint ww = 0, wh = 0;\n'
           '\t\t\t\t\tSDL_GetWindowSize( m_Window, &ww, &wh );\n'
           '\t\t\t\t\tif ( ww > 0 && wh > 0 && m_WindowWidth > 0 && m_WindowHeight > 0 )\n'
           '\t\t\t\t\t{\n'
           '\t\t\t\t\t\tm_flMouseXScale = ( float )m_WindowWidth / ( float )ww;\n'
           '\t\t\t\t\t\tm_flMouseYScale = ( float )m_WindowHeight / ( float )wh;\n'
           '\t\t\t\t\t}\n'
           '\t\t\t\t}\n'
           '#endif\n'+before)
# Without ALLOW_HIGHDPI the UIKit drawable is in points (iPhone 14: 844x390), so the 2338x1080
# backbuffer was shrunk to it and upscaled again by the compositor. Render to native pixels.
change('appframework/sdlmgr.cpp', '\tflags |= SDL_WINDOW_OPENGL;\n',
       '\tflags |= SDL_WINDOW_OPENGL;\n#if defined(SOURCE_IOS)\n\tflags |= SDL_WINDOW_ALLOW_HIGHDPI;\n#endif\n')
# togl presents with Blit2 onto DisplayedSize(), which SDL reports in points: with HighDPI the
# 2532x1170 renderbuffer got the frame in its 844x390 corner. Use the renderbuffer's pixel size.
# (Declared at the call site: the SDL_opengl.h include sits in a non-iOS #else branch.)
change('appframework/sdlmgr.cpp', '\tint w, h;\n\tSDL_GetWindowSize(m_Window, &w, &h);\n\twidth = (uint) w;\n',
       '\tint w, h;\n#if defined(SOURCE_IOS)\n\textern void GModPort_DrawableSize(int* w, int* h);\n'
       '\tGModPort_DrawableSize(&w, &h);\n#else\n'
       '\tSDL_GetWindowSize(m_Window, &w, &h);\n#endif\n\twidth = (uint) w;\n')
# FixUpPath lowercases relative names; only Linux/BSD then retry with a case-insensitive directory
# scan. iOS is case-sensitive too: GMod's garrysmod/resource/ClientScheme.res was never found and
# HL2's sourceengine/resource/clientscheme.res (no BgColorHud etc.) won, so HUD animations drew
# their colours as the scheme-miss error pink.
for path, count in [('filesystem/filesystem_stdio.cpp', 3), ('filesystem/basefilesystem.cpp', 1)]:
    file = root/path
    text = file.read_text(encoding='utf-8')
    before = '#if defined(LINUX) || defined(PLATFORM_BSD)\n'
    after = '#if defined(LINUX) || defined(PLATFORM_BSD) || defined(SOURCE_IOS)\n'
    if text.count(after) != count:
        if text.count(before) != count: raise SystemExit('Unexpected source context: ' + path)
        file.write_text(text.replace(before, after), encoding='utf-8', newline='\n')
        changes.append(path)
# Diagnostic for the solid pink HUD boxes (health/ammo/suit): log what DrawBox is asked to draw.
change('vgui2/vgui_controls/Panel.cpp',
       '\t\t m_nBgTextureId4 == -1 )\n\t{\n\t\treturn;\n\t}\n\n\tcolor[3] *= normalizedAlpha;\n',
       '\t\t m_nBgTextureId4 == -1 )\n\t{\n\t\treturn;\n\t}\n\n\tcolor[3] *= normalizedAlpha;\n'
       '#if defined(SOURCE_IOS)\n\tstatic int s_nBoxLogs;\n\tif ( s_nBoxLogs < 12 )\n\t{\n\t\ts_nBoxLogs++;\n'
       '\t\tint cw, ct, tw, tt;\n\t\tGetCornerTextureSize( cw, ct );\n\t\tsurface()->DrawGetTextureSize( m_nBgTextureId1, tw, tt );\n'
       '\t\tchar file[MAX_PATH] = "";\n\t\tsurface()->DrawGetTextureFile( m_nBgTextureId1, file, sizeof( file ) );\n'
       '\t\tMsg( "GMP DrawBox %s: color %d %d %d %d, box %dx%d, corner %dx%d, tex %d \'%s\' %dx%d, hollow %d\\n", GetName(),\n'
       '\t\t\tcolor.r(), color.g(), color.b(), color.a(), wide, tall, cw, ct, m_nBgTextureId1, file, tw, tt, hollow );\n\t}\n#endif\n')
# GMod-style main menu: large entries sized to the screen (GMod's scheme has no MainMenuFont and
# its MainMenu.MenuItemHeight is $WIN32-only), no server browser (no master server here).
path='gameui/BasePanel.cpp'
change(path, '\tvgui::HFont hMainMenuFont = pScheme->GetFont( "MainMenuFont", IsProportional() );',
       '\tstatic vgui::HFont s_hGMPMenuFont = 0;\n'
       '\tstatic int s_nGMPMenuFontTall = 0;\n'
       '\tint screenWide, screenTall;\n'
       '\tvgui::surface()->GetScreenSize( screenWide, screenTall );\n'
       '\tif ( !s_hGMPMenuFont || s_nGMPMenuFontTall != screenTall / 20 )\n'
       '\t{\n'
       '\t\tconst char *family = vgui::surface()->GetFontFamilyName( pScheme->GetFont( "MenuLarge", IsProportional() ) );\n'
       '\t\tif ( !s_hGMPMenuFont )\n'
       '\t\t\ts_hGMPMenuFont = vgui::surface()->CreateFont();\n'
       '\t\ts_nGMPMenuFontTall = screenTall / 20;\n'
       '\t\tvgui::surface()->SetFontGlyphSet( s_hGMPMenuFont, ( family && *family ) ? family : "Verdana", s_nGMPMenuFontTall, 800, 0, 0,\n'
       '\t\t\tvgui::ISurface::FONTFLAG_ANTIALIAS | vgui::ISurface::FONTFLAG_DROPSHADOW );\n'
       '\t}\n'
       '\tvgui::HFont hMainMenuFont = s_hGMPMenuFont;')
change(path, '\t\t\theight = scheme()->GetProportionalScaledValue( height );\n',
       '\t\t\theight = scheme()->GetProportionalScaledValue( height );\n'
       '\t\tint screenWide, screenTall;\n'
       '\t\tvgui::surface()->GetScreenSize( screenWide, screenTall );\n'
       '\t\theight = MAX( height, screenTall / 14 );\n')
change(path, '\tif( !bFoundServerBrowser && !ModInfo().IsSinglePlayerOnly() )',
       '\tif( false && !bFoundServerBrowser && !ModInfo().IsSinglePlayerOnly() )')
# The open spawn menu (gmp_physgun.inl) takes the touches; without this, scrolling it turns the camera.
path='game/client/cdll_client_int.cpp'
change(path, 'void CHLClient::IN_TouchEvent( int type, int fingerId, int x, int y )\n'
       '{\n'
       '\tif( enginevgui->IsGameUIVisible() )\n'
       '\t\treturn;\n',
       '// GModPort: set by the hl2mp spawn menu (gmp_physgun.inl); other games leave it NULL.\n'
       'bool ( *g_pfnGMPTouchBlocked )( void ) = NULL;\n'
       '\n'
       'void CHLClient::IN_TouchEvent( int type, int fingerId, int x, int y )\n'
       '{\n'
       '\tif( enginevgui->IsGameUIVisible() )\n'
       '\t\treturn;\n'
       '\t// GModPort: the open spawn menu takes the touches, or scrolling it would also turn the camera.\n'
       '\tif( type != IE_FingerUp && g_pfnGMPTouchBlocked && g_pfnGMPTouchBlocked() )\n'
       '\t\treturn;\n')
# GMod's weapon scripts live in scripts/weapons/ (garrysmod_dir.vpk); their c_* viewmodels have no
# arms (GMod adds gmod_hands), so the HL2 v_* model is used when the content has one.
change('game/shared/weapon_parse.cpp',
       '\tif ( !pKV )\n\t\treturn false;\n\n\tpFileInfo->Parse( pKV, szWeaponName );',
       '\tif ( !pKV )\n'
       '\t{\n'
       '\t\t// GModPort: GMod keeps its weapon scripts in scripts/weapons/ inside garrysmod_dir.vpk (GAME, plain .txt).\n'
       '\t\tQ_snprintf( sz, sizeof( sz ), "scripts/weapons/%s", szWeaponName );\n'
       '\t\tpKV = ReadEncryptedKVFile( filesystem, sz, NULL );\n'
       '\t\t// Its c_* viewmodels have no arms (GMod adds them as a separate gmod_hands entity):\n'
       '\t\t// use the HL2 v_* model with arms when the content has one.\n'
       '\t\tconst char *pszView = pKV ? pKV->GetString( "viewmodel", "" ) : "";\n'
       '\t\tconst char *pszC = Q_strstr( pszView, "/c_" );\n'
       '\t\tif ( pszC )\n'
       '\t\t{\n'
       '\t\t\tchar szV[MAX_WEAPON_STRING];\n'
       '\t\t\tQ_strncpy( szV, pszView, sizeof( szV ) );\n'
       '\t\t\tszV[pszC - pszView + 1] = \'v\';\n'
       '\t\t\tif ( filesystem->FileExists( szV, "GAME" ) )\n'
       '\t\t\t\tpKV->SetString( "viewmodel", szV );\n'
       '\t\t}\n'
       '\t}\n'
       '\n'
       '\tif ( !pKV )\n\t\treturn false;\n\n\tpFileInfo->Parse( pKV, szWeaponName );')
# GMod has no suit power: sprint, flashlight and air never run out.
before='\t// Suitpower cheat on?\n\tif ( sv_infinite_aux_power.GetBool() )\n\t\treturn true;\n'
change('game/server/hl2/hl2_player.cpp', before,
       before+'#ifdef HL2MP\n'
       '\t// GModPort: GMod has no suit power; sprint, flashlight and air never run out.\n'
       '\tif ( flPower >= 0.0f )\n'
       '\t\treturn true;\n'
       '#endif\n')
# GModPort: fullscreen video modes must be in this list; ios_main.mm asks for 1080 lines at the screen's
# aspect (2338x1080 on iPhone 14), which no stock entry has, so FindVideoMode fell back to another
# mode and the picture was stretched. Add the -w/-h mode itself.
change('appframework/glmdisplaydb_linuxwin.inl',
       '                        m_modes->AddToTail( new GLMDisplayMode( rect.w, rect.h, 0 ) );\n                }\n        }\n',
       '                        m_modes->AddToTail( new GLMDisplayMode( rect.w, rect.h, 0 ) );\n                }\n        }\n'
       '#if defined(SOURCE_IOS)\n'
       '        {\n'
       '                const int w = CommandLine()->ParmValue( "-w", 0 ), h = CommandLine()->ParmValue( "-h", 0 );\n'
       '                if ( w > 0 && h > 0 )\n'
       '                        m_modes->AddToTail( new GLMDisplayMode( w, h, 0 ) );\n'
       '        }\n'
       '#endif\n')
# GModPort: UIKit finger ids are UITouch pointers, but FingerEvent indexes its arrays (and the client
# touch code its finger slots) by id < TOUCH_FINGER_MAX_COUNT and drops anything else, so no touch
# reached the game once SDL's touch->mouse emulation was off. Map each live finger to a small slot.
change('inputsystem/touch_sdl.cpp',
       'int TouchSDLWatcher( void *userInfo, SDL_Event *event )\n{\n'
       '\tCInputSystem *pInputSystem = (CInputSystem *)userInfo;\n\n'
       '\tif( !event || !pInputSystem ) return 1;\n\n'
       '\tswitch ( event->type ) {\n',
       '#if defined(SOURCE_IOS)\n'
       'static SDL_FingerID s_GMPFingerIds[TOUCH_FINGER_MAX_COUNT];\n'
       'static bool s_GMPFingerUsed[TOUCH_FINGER_MAX_COUNT];\n\n'
       'static int GMP_FingerSlot( SDL_FingerID id, Uint32 type )\n{\n'
       '\tint freeSlot = -1;\n'
       '\tfor ( int i = 0; i < TOUCH_FINGER_MAX_COUNT; i++ )\n\t{\n'
       '\t\tif ( s_GMPFingerUsed[i] && s_GMPFingerIds[i] == id )\n\t\t{\n'
       '\t\t\tif ( type == SDL_FINGERUP )\n\t\t\t\ts_GMPFingerUsed[i] = false;\n'
       '\t\t\treturn i;\n\t\t}\n'
       '\t\tif ( !s_GMPFingerUsed[i] && freeSlot < 0 )\n\t\t\tfreeSlot = i;\n\t}\n'
       '\tif ( type != SDL_FINGERDOWN || freeSlot < 0 )\n\t\treturn -1;\n'
       '\ts_GMPFingerUsed[freeSlot] = true;\n'
       '\ts_GMPFingerIds[freeSlot] = id;\n'
       '\treturn freeSlot;\n}\n'
       '#endif\n\n'
       'int TouchSDLWatcher( void *userInfo, SDL_Event *event )\n{\n'
       '\tCInputSystem *pInputSystem = (CInputSystem *)userInfo;\n\n'
       '\tif( !event || !pInputSystem ) return 1;\n\n'
       '#if defined(SOURCE_IOS)\n'
       '\tif ( event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP || event->type == SDL_FINGERMOTION )\n\t{\n'
       '\t\tconst int slot = GMP_FingerSlot( event->tfinger.fingerId, event->type );\n'
       '\t\tif ( slot < 0 )\n\t\t\treturn 1;\n'
       '\t\tevent->tfinger.fingerId = slot;\n\t}\n'
       '#endif\n\n'
       '\tswitch ( event->type ) {\n')
from patch_touch_editor import apply as patch_touch_editor
changes.extend(patch_touch_editor(root))
print(json.dumps({'engine_commit':pin,'platform_files_patched':changes,'game_code_modified':True},indent=2))
