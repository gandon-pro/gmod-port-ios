"""Enable the native touch layout editor and preserve saved profiles."""
from pathlib import Path

def apply(root):
    path=root/'game/client/touch.cpp'
    text=path.read_text(encoding='utf-8')
    original=text
    def replace(before,after):
        nonlocal text
        if after in text:return
        if text.count(before)!=1:raise RuntimeError('Unexpected touch editor context: '+before[:75])
        text=text.replace(before,after)
    replace('#include "touch.h"', '#include "touch.h"\n#include "gmp_touch_layout.h"')
    marker='\tCTouchTexture *texture = new CTouchTexture;\n\ttexture->isInAtlas = false;'
    migration='''\t// Preserve existing profiles, adding the editor entry only when missing.
\tbool haveEditor = false;
\tfor ( int i = 0; i < btns.Count(); ++i )
\t\tif ( !Q_strcmp( btns[i]->name, "gmp_layout" ) ) haveEditor = true;
\tif ( !haveEditor )
\t\tAddButton( "gmp_layout", "vgui/touch/gmp_settings", "touch_enableedit",
\t\t\t0.39f, 0.02f, 0.47f, 0.16f, rgba_t(255,255,255,230) );

'''
    replace(marker,migration+marker)
    replace('void CTouchControls::EnableTouchEdit(bool enable)\n{', '''void CTouchControls::EnableTouchEdit(bool enable)
{
\tif ( enable == (state == state_edit) ) return;
\t// Release held actions before changing finger ownership (fire/jump/duck).
\tfor ( int i = 0; i < btns.Count(); ++i )
\t{
\t\tCTouchButton *button = btns[i];
\t\tif ( button->finger != -1 && button->command[0] == '+' )
\t\t{
\t\t\tchar release[256];
\t\t\tQ_strncpy( release, button->command, sizeof(release) );
\t\t\trelease[0] = '-';
\t\t\tengine->ClientCmd_Unrestricted( release );
\t\t}
\t\tbutton->finger = -1;
\t}
\tforward = side = pitch = yaw = 0.f;
\tm_flPreviousYaw = m_flPreviousPitch = 0.f;
\tselection = NULL;''')
    replace('"close_edit", "vgui/touch/back", "touch_disableedit", 0.020000, 0.800000, 0.100000, 0.977778',
            '"close_edit", "vgui/touch/gmp_done", "touch_disableedit", 0.850000, 0.020000, 0.950000, 0.190000')
    replace('float aspect = ( b->y2 - b->y1 ) / ( ( b->x2 - b->x1 )/(screen_h/screen_w) );',
            'float aspect = GMP_TouchAspect( b->x1, b->y1, b->x2, b->y2, screen_w, screen_h );')
    replace('\t\tconst int cx = active ? move_start_x * screen_w : 0.16f * screen_w;\n'
            '\t\tconst int cy = active ? move_start_y * screen_h : 0.70f * screen_h;',
            '''\t\tfloat idleX = .16f, idleY = .70f;
\t\tfor ( int i = 0; i < btns.Count(); ++i )
\t\t\tif ( btns[i]->type == touch_move )
\t\t\t{
\t\t\t\tidleX = btns[i]->x1 + (btns[i]->x2 - btns[i]->x1) / 3.f;
\t\t\t\tidleY = btns[i]->y1 + (btns[i]->y2 - btns[i]->y1) * 2.f / 3.f;
\t\t\t\tbreak;
\t\t\t}
\t\tconst int cx = active ? move_start_x * screen_w : idleX * screen_w;
\t\tconst int cy = active ? move_start_y * screen_h : idleY * screen_h;''')
    # Ignore unmatched releases, e.g. after the editor was closed while dragging.
    replace('if( ev->fingerid == move_finger )\n\t\t{\n\t\t\tmove_finger = -1;\n\t\t\tIN_CheckCoords',
            'if( selection && ev->fingerid == move_finger )\n\t\t{\n\t\t\tmove_finger = -1;\n\t\t\tIN_CheckCoords')
    if text!=original:
        path.write_text(text,encoding='utf-8',newline='\n')
        return ['game/client/touch.cpp']
    return []

if __name__=='__main__':
    import sys
    print(apply(Path(sys.argv[1])))
