"""Extract existing waf module source lists for the separate iOS CMake target.

This evaluates project metadata, not dependency/compiler probes. Actual compiler
and linker checks are performed by CMake. Missing submodules fail explicitly.
"""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import runpy
import sys
import types


class Env(dict):
    def __getattr__(self, name):
        if name.startswith('__'): raise AttributeError(name)
        return self.get(name, [])
    def __setattr__(self, name, value): self[name] = value
    def append_unique(self, name, values):
        if isinstance(values, str): values = [values]
        target = self.setdefault(name, [])
        for value in values:
            if value not in target: target.append(value)
    append_value = append_unique


class Context:
    def __init__(self, engine, folder, env):
        self.env = env
        self.options = types.SimpleNamespace(DEDICATED=False, TESTS=False, GAMES='hl2mp',
                                             SDL=1, GL=0, TOGLES=True, DEBUG_ENGINE=False)
        self.directory = engine / folder
        self.projects = []
        self.path = types.SimpleNamespace(abspath=lambda: str(self.directory))
    def define(self, name, value=1):
        if isinstance(value, (str,list,tuple)):
            value=json.dumps(str(value) if not isinstance(value,str) else value)
        self.env.append_unique('DEFINES', [f'{name}={value}'])
    def get_taskgen_count(self): return len(self.projects)
    def stlib(self, **kwargs): return self.capture('static', kwargs)
    def shlib(self, **kwargs): return self.capture('module', kwargs)
    def __call__(self, **kwargs): return self.capture('other', kwargs)
    def capture(self, kind, kwargs):
        def paths(value): return [str((self.directory / p).resolve()) for p in value]
        kwargs['source'] = paths(kwargs.get('source', []))
        kwargs['includes'] = paths(kwargs.get('includes', []))
        kwargs['defines'] = list(dict.fromkeys(self.env.DEFINES + kwargs.get('defines', [])))
        kwargs['kind'] = kind
        self.projects.append(kwargs)
        return types.SimpleNamespace(**kwargs)
    def check(self, **kwargs): return True  # Dependency probes belong to CMake.
    check_cc = check
    check_cxx = check
    check_cfg = check
    def fatal(self, message, *args): raise RuntimeError(str(message) + str(args))
    def install_files(self, *args, **kwargs): pass
    def install_as(self, *args, **kwargs): pass


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--engine', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sdl-include', required=True, type=Path)
    args=parser.parse_args()
    engine=args.engine.resolve()
    # No dependency detection is emulated: only source-list helpers are used.
    waf=types.ModuleType('waflib')
    waf.Utils=types.SimpleNamespace(to_list=lambda v: v.split() if isinstance(v,str) else list(v))
    waf.Configure=types.SimpleNamespace(conf=lambda f:f)
    waf.Logs=types.SimpleNamespace(info=print, warn=print)
    waf.Context=types.SimpleNamespace(Context=type('Context',(),{}))
    sys.modules['waflib']=waf
    sys.path.insert(0,str(engine/'scripts/waifulib'))
    root=runpy.run_path(str(engine/'wscript'))
    folders=[p for p in root['projects']['game'] if p not in ('launcher_main','utils/vtex','serverbrowser','video')]
    folders.append('togles')
    env=Env(DEST_OS='ios', DEST_CPU='aarch64', COMPILER_CC='clang', COMPILER_CXX='clang',
            GAMES='hl2mp', SDL=1, GL=False, TOGLES=True, OPUS=False, DEDICATED=False,
            TESTS=False, BUILD_SDK=False, PREFIX='/unused', LIBDIR='/unused', MSVC_SUBSYSTEM='',
            INCLUDES_SDL2=[str(args.sdl_include.resolve())],
            DEFINES=['POSIX=1','_POSIX=1','PLATFORM_POSIX=1','OSX=1','_OSX=1',
                     'SOURCE_IOS=1','PLATFORM_64BITS=1','GNUC=1','TOGLES=1',
                     'DX_TO_GL_ABSTRACTION=1','USE_SDL=1','DISABLE_STEAM=1','NO_STEAM=1',
                     'NO_HOOK_MALLOC=1','NO_MEMOVERRIDE_NEW_DELETE=1','TIER1_STATIC_LIB=1',
                     'NDEBUG=1','_DLL_EXT=.a','GIT_COMMIT_HASH="ed8209cc35c61fbd8ddff8480962a01c981eef2f"'])
    result=[]
    failures=[]
    for folder in dict.fromkeys(folders):
        context=Context(engine,folder,copy.deepcopy(env))
        context.env.SUBPROJECT_PATH=[str(context.directory)]
        previous=os.getcwd()
        try:
            os.chdir(context.directory)
            module=runpy.run_path(str(context.directory/'wscript'))
            if 'configure' in module: module['configure'](context)
            module['build'](context)
            for target in context.projects:
                target['folder']=folder
                # Only add Aero's integration code; never edit it.
                if folder in ('game/client','game/server'):
                    extra=list((engine/folder/'gmod').rglob('*.cpp'))+list((engine/'game/shared/gmod').rglob('*.cpp'))
                    target['source'] += [str(p) for p in extra]
                result.append(target)
        except Exception as error:
            failures.append({'folder':folder,'error':str(error)})
        finally:
            os.chdir(previous)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({'projects':result,'failures':failures,
                                     'metadata_only':True},indent=2)+'\n',encoding='utf-8')
    print(f'Exported {len(result)} targets; metadata errors: {len(failures)}')
    for failure in failures: print(failure)
    if failures: raise SystemExit(1)


if __name__=='__main__': main()
