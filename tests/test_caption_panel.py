"""Compile the production card class against a deterministic VGUI geometry host."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
text=(root/'engine_src/game/shared/hl2mp/gmp_spawn_icon.inl').read_text()
start=text.index('class CGMPSpawnIconPanel');b=text.index('{',start);depth=1;end=b+1
while depth:
 if text[end]=='{':depth+=1
 elif text[end]=='}':depth-=1
 end+=1
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'panel.cpp';exe=Path(d)/'panel'
 cpp.write_text((root/'tests/caption_panel_fixture.cpp.in').read_text().replace('@PANEL_CLASS@',text[start:end]))
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root/'engine_src/public'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
