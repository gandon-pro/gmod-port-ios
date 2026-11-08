"""Compile production menu dispatch branches with a deterministic engine/filesystem host."""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile
p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,default=Path('deps/engine'));a=p.parse_args()
root=Path(__file__).resolve().parents[1]
def block(text,start):
 i=text.index(start);b=text.index('{',i);depth=1;j=b+1
 while depth:
  if text[j]=='{':depth+=1
  elif text[j]=='}':depth-=1
  j+=1
 return text[i:j]
menu=(root/'engine_src/gameui/gmp_lua_menu.inl').read_text();demo=(a.engine/'public/demofile/demoformat.h').read_text()
source=(root/'tests/local_content_fixture.cpp.in').read_text()
for key,text in {
 '@DEMO_HEADER@':block(demo,'struct demoheader_t')+';',
 '@DEMO_BYTESWAP@':block(demo,'inline void ByteSwap_demoheader_t'),
 '@MENU_NAME@':block(menu,'static bool GMP_MenuName('),
 '@LOCAL_ACTION@':block(menu,'if(!strcmp(op,"LocalAction"))'),
 '@LOCAL_DELETE@':block(menu,'if(!strcmp(op,"LocalDelete"))'),
}.items():source=source.replace(key,text)
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'local.cpp';exe=Path(d)/'local';cpp.write_text(source)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-sign-compare','-I'+str(root/'engine_src/public'),'-I'+str(root/'engine_src/game/shared/gmod'),'-I'+str(root/'deps/luajit/src'),str(cpp),str(root/'deps/luajit/src/libluajit.a'),*(['-ldl'] if sys.platform!='darwin' else []),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
