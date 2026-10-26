"""SDL controllers and all exposed Micro modes hide action buttons while touch look remains active.

K mode uses resources/gmp_micro.cfg. If iOS exposes S/D as a GCController,
the SDL mapping and resources/gmp_micro_gamepad.cfg handle its digital buttons.
"""


def apply(root):
    changed = []

    def change(path, before, after):
        text = (root / path).read_text(encoding='utf-8')
        if after in text:
            return
        if text.count(before) != 1:
            raise RuntimeError('Unexpected gamepad context in %s: %s' % (path, before[:75]))
        (root / path).write_text(text.replace(before, after), encoding='utf-8', newline='\n')
        if path not in changed:
            changed.append(path)

    # The stock Xbox 360 cfg binds JOY1/POV codes that SDL controllers never send; the port ships
    # gmp_gamepad.cfg with the SDL button names. It runs once per launch and per reconnect
    # (+jlook and the axis setup are not saved in config.cfg).
    change('game/client/in_joystick.cpp',
           '#include "cam_thirdperson.h"\n',
           '#include "cam_thirdperson.h"\n'
           '#if defined(SOURCE_IOS)\n'
           'extern "C" bool GMP_MicroGamepadConnected();\n'
           '#endif\n')
    change('game/client/in_joystick.cpp',
           '\tstatic ConVarRef var( "joy_xcontroller_found" );\n'
           '\tif ( var.IsValid() && var.GetBool() && in_joystick.GetBool() )\n'
           '\t{\n'
           '\t\tif ( joy_xcontroller_cfg_loaded.GetInt() < 2 )\n'
           '\t\t{\n'
           '\t\t\tengine->ClientCmd_Unrestricted( "exec 360controller.cfg" );\n'
           '\t\t\tif ( IsLinux () )\n'
           '\t\t\t{\n'
           '\t\t\t\tengine->ClientCmd_Unrestricted( "exec 360controller-linux.cfg" );\n'
           '\t\t\t}\n'
           '\t\t\tjoy_xcontroller_cfg_loaded.SetValue( 2 );\n'
           '\t\t}\n'
           '\t}\n'
           '\telse if ( joy_xcontroller_cfg_loaded.GetInt() > 0 )\n'
           '\t{\n'
           '\t\tengine->ClientCmd_Unrestricted( "exec undo360controller.cfg" );\n'
           '\t\tjoy_xcontroller_cfg_loaded.SetValue( 0 );\n'
           '\t}\n',
           '\t// GModPort: use the digital Micro profile when iOS exposes S/D as a game controller.\n'
           '\tstatic ConVarRef var( "joy_xcontroller_found" );\n'
           '\tstatic bool s_bGMPPadCfg = false;\n'
           '\tstatic bool s_bGMPMicroCfg = false;\n'
           '\tbool bGMPMicro = false;\n'
           '#if defined(SOURCE_IOS)\n'
           '\tbGMPMicro = GMP_MicroGamepadConnected();\n'
           '#endif\n'
           '\tif ( var.IsValid() && var.GetBool() && in_joystick.GetBool() )\n'
           '\t{\n'
           '\t\tif ( !s_bGMPPadCfg || bGMPMicro != s_bGMPMicroCfg )\n'
           '\t\t{\n'
           '\t\t\ts_bGMPPadCfg = true;\n'
           '\t\t\ts_bGMPMicroCfg = bGMPMicro;\n'
           '\t\t\tengine->ClientCmd_Unrestricted( bGMPMicro ? "exec gmp_micro_gamepad.cfg" : "exec gmp_gamepad.cfg" );\n'
           '\t\t\tjoy_xcontroller_cfg_loaded.SetValue( 2 );\n'
           '\t\t}\n'
           '\t}\n'
           '\telse\n'
           '\t{\n'
           '\t\ts_bGMPPadCfg = false;\n'
           '\t\ts_bGMPMicroCfg = false;\n'
           '\t\tjoy_xcontroller_cfg_loaded.SetValue( 0 );\n'
           '\t}\n')

    path = 'game/client/touch.cpp'
    change(path, '#include "viewrender.h"\n',
           '#include "viewrender.h"\n'
           '#include "inputsystem/iinputsystem.h"\n'
           '\n'
           'extern IInputSystem *inputsystem;\n'
           '// GModPort: SDL controllers and keyboard-mode pads hide action buttons.\n'
           '// Keep touch move/look active because keyboard-mode pads lack an analogue look stick.\n'
           'extern "C" bool GMP_HardwareKeyboardConnected();\n'
           'ConVar touch_gamepad_hide( "touch_gamepad_hide", "1", FCVAR_ARCHIVE, "Hide touch buttons while a controller or keyboard is used" );\n'
           'static bool s_bGMPTouchHidden = false;\n'
           'static bool s_bGMPHadPad = false;\n'
           'static bool s_bGMPPadInput = false;\n'
           '\n'
           '// Called from CHLClient::IN_KeyEvent for controller and keyboard buttons.\n'
           'void GMP_TouchPadInput()\n'
           '{\n'
           '\ts_bGMPPadInput = true;\n'
           '}\n')
    change(path,
           '\telse\n\t\tm_bCutScene = false;\n\n'
           '\tif( touch_enable.GetBool() && touch_draw.GetBool() && !enginevgui->IsGameUIVisible() ) Paint();\n',
           '\telse\n\t\tm_bCutScene = false;\n\n'
           '\t// GModPort: keyboard-mode Micro follows the same hide/show behavior as a controller.\n'
           '\tconst bool havePad = ( inputsystem && inputsystem->GetJoystickCount() > 0 ) || GMP_HardwareKeyboardConnected();\n'
           '\tbool hide = s_bGMPTouchHidden;\n'
           '\tif( havePad != s_bGMPHadPad )\n'
           '\t{\n'
           '\t\ts_bGMPHadPad = havePad;\n'
           '\t\thide = havePad;\n'
           '\t\tDevMsg( "Touch: hardware controller or keyboard %s\\n", havePad ? "connected, hiding buttons" : "disconnected" );\n'
           '\t}\n'
           '\tif( s_bGMPPadInput )\n'
           '\t\thide = true;\n'
           '\ts_bGMPPadInput = false;\n'
           '\tif( !touch_gamepad_hide.GetBool() || state == state_edit )\n'
           '\t\thide = false;\n'
           '\tif( hide && !s_bGMPTouchHidden )\n'
           '\t{\n'
           '\t\t// Release held buttons (fire/jump/duck); the move and look zones keep working.\n'
           '\t\tfor( int i = 0; i < btns.Count(); i++ )\n'
           '\t\t{\n'
           '\t\t\tCTouchButton *btn = btns[i];\n'
           '\t\t\tif( btn->finger == -1 || btn->type == touch_move || btn->type == touch_look )\n'
           '\t\t\t\tcontinue;\n'
           '\t\t\tif( btn->command[0] == \'+\' )\n'
           '\t\t\t{\n'
           '\t\t\t\tchar cmd[256];\n'
           '\t\t\t\tQ_strncpy( cmd, btn->command, sizeof(cmd) );\n'
           '\t\t\t\tcmd[0] = \'-\';\n'
           '\t\t\t\tengine->ClientCmd_Unrestricted( cmd );\n'
           '\t\t\t}\n'
           '\t\t\tbtn->finger = -1;\n'
           '\t\t}\n'
           '\t}\n'
           '\ts_bGMPTouchHidden = hide;\n'
           '\n'
           '\tif( touch_enable.GetBool() && touch_draw.GetBool() && !enginevgui->IsGameUIVisible() && !s_bGMPTouchHidden ) Paint();\n')
    change(path,
           '\tif( ev->type == IE_FingerDown )\n'
           '\t{\n'
           '\t\tfor( it = btns.begin(); it != btns.end(); it++ )\n'
           '\t\t{\n'
           '\t\t\tCTouchButton *btn = *it;\n'
           '\t\t\tif(  x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )\n'
           '\t\t\t{\n'
           '\t\t\t\tif( btn->flags & TOUCH_FL_HIDE )\n'
           '\t\t\t\t\tcontinue;\n',
           '\tif( ev->type == IE_FingerDown )\n'
           '\t{\n'
           '\t\t// GModPort: with hidden buttons only the move/look zones act; a tap on a button shows them.\n'
           '\t\tconst bool hidden = s_bGMPTouchHidden;\n'
           '\t\tfor( it = btns.begin(); it != btns.end(); it++ )\n'
           '\t\t{\n'
           '\t\t\tCTouchButton *btn = *it;\n'
           '\t\t\tif(  x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )\n'
           '\t\t\t{\n'
           '\t\t\t\tif( btn->flags & TOUCH_FL_HIDE )\n'
           '\t\t\t\t\tcontinue;\n'
           '\t\t\t\tif( hidden && btn->type != touch_move && btn->type != touch_look )\n'
           '\t\t\t\t{\n'
           '\t\t\t\t\ts_bGMPTouchHidden = false;\n'
           '\t\t\t\t\tcontinue;\n'
           '\t\t\t\t}\n')

    path = 'game/client/cdll_client_int.cpp'
    change(path,
           'int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )\n'
           '{\n',
           'void GMP_TouchPadInput();\n'
           '\n'
           'int CHLClient::IN_KeyEvent( int eventcode, ButtonCode_t keynum, const char *pszCurrentBinding )\n'
           '{\n'
           '\t// GModPort: Micro K-mode keys hide touch buttons like controller input.\n'
           '\tif ( eventcode && ( IsKeyCode( keynum ) || IsJoystickCode( keynum ) ) )\n'
           '\t\tGMP_TouchPadInput();\n')
    return changed
