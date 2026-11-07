"""Exercise the exact map-failure patch against Mesa GLES3 buffers."""
import argparse,ast
from pathlib import Path
import subprocess,tempfile
p=argparse.ArgumentParser()
p.add_argument('--include-dir')
p.add_argument('--gl-library',default='-lGLESv2')
p.add_argument('--egl-library',default='-lEGL')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
patch=ast.parse((root/'tools/patch_engine.py').read_text())
block=None
for node in ast.walk(patch):
 if isinstance(node,ast.Call) and isinstance(node.func,ast.Name) and node.func.id=='change' and len(node.args)==3:
  if all(isinstance(arg,ast.Constant) for arg in node.args):
   path,before,after=[arg.value for arg in node.args]
   if path=='togles/linuxwin/cglmbuffer.cpp' and 'GMP buffer: map failed' in after:
    block=after[after.index('#if defined(SOURCE_IOS)'):]
assert block
with tempfile.TemporaryDirectory() as directory:
 tmp=Path(directory);cpp=tmp/'test.cpp';exe=tmp/'test'
 cpp.write_text((root/'tests/buffer_fallback_fixture.cpp.in').read_text().replace('@FALLBACK@',block))
 cmd=['c++','-std=c++17','-Wall','-Wextra','-Werror']
 if a.include_dir:cmd+=['-I',a.include_dir]
 subprocess.run(cmd+[str(cpp),a.gl_library,a.egl_library,'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
