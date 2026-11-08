"""Regression checks for build104's black tool RT and sparse empty weapon cells."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,default=Path('deps/engine'));p.add_argument('--include-dir');p.add_argument('--gl-library',default='-lGLESv2');p.add_argument('--egl-library',default='-lEGL');args=p.parse_args()
root=Path(__file__).resolve().parents[1]
def block(text,start):
    i=text.index(start);b=text.index('{',i);depth=1;j=b+1
    while depth:
        if text[j]=='{':depth+=1
        elif text[j]=='}':depth-=1
        j+=1
    return text[i:j]
header=(args.engine/'public/mathlib/vmatrix.h').read_text()
weapons=(args.engine/'game/client/hl2/hud_weaponselection.cpp').read_text()
screen=(root/'engine_src/game/shared/hl2mp/gmp_tool_screen.inl').read_text()
paint=block(weapons,'void CHudWeaponSelection::Paint()')
case=block(paint,'case HUDTYPE_BUCKETS:')+'\n break;'
assert re.search(r'^hud_showemptyweaponslots\s+0\s*$',(root/'resources/gmp_bindings.cfg').read_text(),re.M)
# Include the real inline math, production adapter and engine iteration logic, not copies.
replacements={
 '@SOURCE_ORTHO@':block(header,'inline void MatrixBuildOrtho('),
 '@SOURCE_SCALE@':block(header,'inline void MatrixBuildScale( VMatrix &dst, float'),
 '@SOURCE_MATRIX_ORTHO@':block(header,'inline void MatrixOrtho('),
 '@PRODUCTION_BEGIN_2D@':block(screen,'static void GMP_ToolScreenBegin2D('),
 '@SOURCE_BUCKET_CASE@':case,
}
source=(root/'tests/tool_ui_fixture.cpp.in').read_text()
for key,value in replacements.items():source=source.replace(key,value)
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory);cpp=tmp/'tool_ui.cpp';exe=tmp/'tool_ui';cpp.write_text(source)
    command=['c++','-std=c++17','-Wall','-Wextra','-Werror']
    if args.include_dir:command+=['-I',args.include_dir]
    subprocess.run(command+[str(cpp),args.gl_library,args.egl_library,'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
