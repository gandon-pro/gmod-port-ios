"""Make a statically linked Source module retain its original private state.

Apple's supported ld -r + nmedit pipeline, run only at build time:
https://github.com/apple-oss-distributions/cctools/blob/main/man/nmedit.1
No duplicate-definition suppression or runtime dynamic loader is used.
"""
import argparse
import json
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--archive',type=Path,required=True)
parser.add_argument('--helper',type=Path,action='append',default=[])
parser.add_argument('--export',dest='exports',action='append',default=[])
parser.add_argument('--export-module-symbols',action='store_true')
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--sdk',default='iphoneos',choices=['iphoneos','macosx'])
parser.add_argument('--arch',default='arm64')
parser.add_argument('--minimum',default='15.0')
args=parser.parse_args()

def run(command):
    return subprocess.check_output(command,text=True).strip()

def defined(path):
    return {s for s in run(['xcrun','nm','-g','-U','-j',str(path)]).splitlines()
            if s.startswith('_') and not s.endswith(':')}

sdk=run(['xcrun','--sdk',args.sdk,'--show-sdk-path'])
version=run(['xcrun','--sdk',args.sdk,'--show-sdk-version'])
symbols={'_'+s for s in args.exports}
module_symbols=defined(args.archive)
if not symbols <= module_symbols:
    raise SystemExit('Missing module exports: '+str(sorted(symbols-module_symbols)))
if args.export_module_symbols:
    # Shared tier0/vstdlib/ToGLES APIs remain visible. Their factory registry
    # implementation is still private, just as in every other module.
    symbols |= {s for s in module_symbols
                if not any(name in s for name in
                           ('InterfaceReg','CreateInterfaceInternal','Sys_GetFactoryThis'))}
args.output.parent.mkdir(parents=True,exist_ok=True)
exports=args.output.with_suffix('.exports.txt')
exports.write_text('\n'.join(sorted(symbols))+'\n')
combined=args.output.with_suffix('.combined.o')
subprocess.run(['xcrun','ld','-r','-arch',args.arch,'-syslibroot',sdk,
                '-platform_version','ios' if args.sdk=='iphoneos' else 'macos',args.minimum,version,
                '-o',str(combined),'-force_load',str(args.archive),
                *map(str,args.helper)],check=True)
subprocess.run(['xcrun','nmedit','-s',str(exports),str(combined),'-o',str(args.output)],check=True)
actual=defined(args.output)
if actual != symbols:
    raise SystemExit('Unexpected module exports: '+str(sorted(actual ^ symbols)))
print(json.dumps({'module':str(args.archive),'object':str(args.output),
                  'exported':len(actual),'helpers':len(args.helper),'isolated':True}))
