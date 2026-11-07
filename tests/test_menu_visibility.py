"""Compile the actual patched CGameMenu visibility override against a tiny VGUI fixture.

Run after patch_engine.py, passing deps/engine/gameui/BasePanel.cpp.
The unpatched override must reproduce the visible-popup bug in the same fixture.
"""
import ast
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).read_text()
patch = ast.parse((root / 'tools/patch_engine.py').read_text())
pair = None
for node in ast.walk(patch):
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == 'change':
        args = node.args
        if len(args) == 3 and all(isinstance(x, ast.Constant) for x in args):
            path, before, after = [x.value for x in args]
            if path == 'gameui/BasePanel.cpp' and before.startswith('\tvirtual void SetVisible(bool state)'):
                pair = before, after
assert pair, 'visibility patch missing'
before, after = pair
assert after in source, 'run patch_engine.py before the regression test'

def method(text):
    start = text.index('\tvirtual void SetVisible(bool state)')
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

fixture = r'''
#include <cassert>
struct MenuLua { bool ready=false; bool DocumentReady()const{return ready;} } s_GMPMenuLua;
struct Panels { void SetVisible(int,bool){} void MoveToBack(int){} } panels;
Panels* ipanel(){return &panels;}
struct Base {
 bool visible=true, mouse=true, keyboard=true;
 void SetVisible(bool state){visible=state;}
 void SetMouseInputEnabled(bool state){mouse=state;}
 void SetKeyBoardInputEnabled(bool state){keyboard=state;}
 int GetVPanel(){return 1;}
};
struct Menu:Base {
 using BaseClass=Base;
 int m_hMainMenuOverridePanel=0;
 METHOD
};
int main(){
 Menu menu;
 s_GMPMenuLua.ready=false;menu.SetVisible(true);
 assert(menu.visible && menu.mouse && menu.keyboard);
 s_GMPMenuLua.ready=true;menu.SetVisible(false);
 CHECK
}
'''
checks = '''assert(!menu.visible && !menu.mouse && !menu.keyboard);
// Native UpdateGameMenus may request true while the HTML owns the UI.
menu.SetVisible(true);assert(!menu.visible && !menu.mouse && !menu.keyboard);
// A failed/released HTML controller restores a usable native fallback.
s_GMPMenuLua.ready=false;menu.SetVisible(true);
assert(menu.visible && menu.mouse && menu.keyboard);'''
with tempfile.TemporaryDirectory() as folder:
    tmp = Path(folder)
    cases = [('original', method(source.replace(after, before)), 'assert(menu.visible);'),
             ('patched', method(source), checks)]
    for name, body, check in cases:
        cpp = tmp / (name + '.cpp')
        exe = tmp / name
        cpp.write_text(fixture.replace('METHOD', body).replace('CHECK', check))
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        print(name + ': passed')
print('Actual CGameMenu override: original bug reproduced; HTML hide, repeated show, input and fallback passed')
