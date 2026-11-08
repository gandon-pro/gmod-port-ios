"""Run actual patched Source texture-reupload methods against real GLES3."""
import argparse
from pathlib import Path
import subprocess
import tempfile
p=argparse.ArgumentParser()
p.add_argument('engine',type=Path)
p.add_argument('--include-dir')
p.add_argument('--gl-library',default='-lGLESv2')
p.add_argument('--egl-library',default='-lEGL')
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
source=(a.engine/'togles/linuxwin/cglmtex.cpp').read_text()
assert '#if defined(OSX) || defined(SOURCE_IOS) // GModPort sRGB texture fallback' in source
start=source.index('void CGLMTex::HandleSRGBMismatch(')
end=source.index('\n#endif\n',source.index('void CGLMTex::ResetSRGB(',start))
methods=source[start:end]
with tempfile.TemporaryDirectory() as directory:
 tmp=Path(directory);cpp=tmp/'test.cpp';exe=tmp/'test'
 cpp.write_text((root/'tests/srgb_gpu_fixture.cpp.in').read_text().replace('@METHODS@',methods))
 command=['c++','-std=c++17','-Wall','-Wextra','-Werror']
 if a.include_dir:command+=['-I',a.include_dir]
 subprocess.run(command+[str(cpp),a.gl_library,a.egl_library,'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
