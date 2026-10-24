"""Convert exported waf lists to static CMake targets and module factories."""
import argparse
import json
from pathlib import Path
import re

parser=argparse.ArgumentParser()
parser.add_argument('--manifest',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--platform',type=Path,required=True)
args=parser.parse_args()
manifest=json.loads(args.manifest.read_text())
if manifest['failures']:raise SystemExit('Cannot build an incomplete source graph')
args.output.mkdir(parents=True,exist_ok=True)
projects=manifest['projects']
def q(text):return '"'+str(text).replace('\\','/').replace('"','\\"')+'"'
def ident(name):return 'src_'+re.sub('[^A-Za-z0-9_]','_',name)
text=[]
table=[]
externs=[]
module_targets=[]
names={p['target']:ident(p['target']) for p in projects}
external={'SDL2':'SDL2::SDL2-static','FT2':'ios_freetype','FC':'ios_fontconfig',
          'JPEG':'ios_jpeg','PNG':'ios_png','CURL':'ios_curl','OPENAL':'ios_openal',
          'BZ2':'bz2','ZLIB':'z','M':'m','ICONV':'iconv'}
ignored={'DL','LOG','RT','UNWIND','ANDROID_SUPPORT'}
for p in projects:
    name=p['target'];target=ident(name)
    source=[s for s in p['source'] if not s.endswith(('/tier1/interface.cpp','/tier0/memoverride.cpp'))]
    text.append(f'add_library({target} STATIC\n  '+'\n  '.join(q(s) for s in source)+'\n)')
    text.append(f'target_include_directories({target} PRIVATE '+' '.join(q(s) for s in p['includes'])+' ${SOURCE_ENGINE}/common ${GMOD_DEPS_PREFIX}/include ${GMOD_DEPS_PREFIX}/include/freetype2)')
    defines=[d for d in p['defines'] if d not in ('PROTECTED_THINGS_ENABLE','USE_BREAKPAD_HANDLER')]
    text.append(f'target_compile_definitions({target} PRIVATE '+' '.join(q(d) for d in defines)+')')
    text.append(f'target_compile_options({target} PRIVATE -fno-strict-aliasing -fsigned-char -Wno-register -Wno-c++11-narrowing -Wno-deprecated-declarations -Wno-inconsistent-missing-override)')
    deps=[]
    for dep in p.get('use',[]):
        if dep in names:deps.append(names[dep])
        elif dep in external:deps.append(external[dep])
        elif dep not in ignored:raise SystemExit('Unmapped dependency '+dep+' for '+name)
    if deps:text.append(f'target_link_libraries({target} PUBLIC '+' '.join(dict.fromkeys(deps))+')')
    if p['kind']=='module':
        prefix='gp_'+re.sub('[^A-Za-z0-9_]','_',name)
        header=args.output/(name+'_prefix.h')
        symbols=['InterfaceReg','CreateInterface','CreateInterfaceInternal','Sys_GetFactoryThis',
                 'g_pFileSystem','g_pFullFileSystem','g_pMaterialSystem','g_pMatSystemSurface',
                 'g_pLauncherMgr','g_pPhysics','g_pGameRules','g_pSourceVR']
        header.write_text('#pragma once\n'+'\n'.join('#define '+s+' '+prefix+'_'+s for s in symbols)+'\n')
        factory=args.output/(name+'_factory.cpp')
        factory.write_text((args.platform/'module_factory.cpp.in').read_text())
        text.append(f'target_sources({target} PRIVATE {q(factory)})')
        text.append(f'target_compile_options({target} PRIVATE -include {q(header)})')
        externs.append(f'extern "C" void* {prefix}_CreateInterface(const char*,int*);')
        table.append('{"'+name+'", &'+prefix+'_CreateInterface}')
        module_targets.append(target)
registry=args.output/'module_table.cpp'
registry.write_text('#include "static_modules.h"\n'+'\n'.join(externs)+'\nbool InstallSourceModules() {\nstatic const gmodport::Module modules[] = {\n'+',\n'.join(table)+'\n};\nreturn gmodport::InstallModules(modules,sizeof(modules)/sizeof(modules[0]));\n}\n')
text.append('set(SOURCE_MODULE_TARGETS '+' '.join(module_targets)+')')
text.append('set(SOURCE_MODULE_TABLE '+q(registry)+')')
(args.output/'targets.cmake').write_text('\n'.join(text)+'\n')
