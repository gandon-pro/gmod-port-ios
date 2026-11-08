"""Test Source's injected sRGB suffix/state update on real GLES3."""
import argparse,ast,re
from pathlib import Path
import subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--include-dir');p.add_argument('--gl-library',default='-lGLESv2');p.add_argument('--egl-library',default='-lEGL');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
suffix=draw=depth=None
for node in ast.walk(ast.parse((root/'tools/patch_engine.py').read_text())):
 if isinstance(node,ast.Call) and isinstance(node.func,ast.Name) and node.func.id=='change' and len(node.args)==3 and all(isinstance(x,ast.Constant) for x in node.args):
  path,before,after=[x.value for x in node.args]
  if path=='togles/linuxwin/dx9asmtogl2.cpp' and 'vec3 linearRGB' in after:
   ios=after.split('#else')[0]
   suffix=''.join(ast.literal_eval(s) for s in re.findall(r'StrcatToALUCode\(\s*("(?:[^"\\]|\\.)*")\s*\)',ios))
  if path=='togles/linuxwin/dx9asmtogl2.cpp' and 'm_iFragDataCount == 0' in after:
   depth=after[:after.index('\t// Control bit')]
  if path=='togles/linuxwin/glmgr_flush.inl' and 'const float enabled = m_FakeBlendEnableSRGB' in after:
   draw=after[:after.index('\tm_pBoundPair->UpdateScreenUniform')]
assert suffix and draw and depth
with tempfile.TemporaryDirectory() as directory:
 tmp=Path(directory);cpp=tmp/'test.cpp';exe=tmp/'test'
 cpp.write_text((root/'tests/srgb_output_fixture.cpp.in').read_text().replace('@SUFFIX@',suffix).replace('@DRAW_STATE@',draw).replace('@DEPTH_GUARD@',depth))
 cmd=['c++','-std=c++17','-Wall','-Wextra','-Werror','-DSOURCE_IOS=1']
 if a.include_dir:cmd+=['-I',a.include_dir]
 subprocess.run(cmd+[str(cpp),a.gl_library,a.egl_library,'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
