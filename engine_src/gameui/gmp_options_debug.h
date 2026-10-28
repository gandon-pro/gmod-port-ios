// GModPort: Options > Debug. Debug overlays and switches that otherwise need the console
// (personal build). Included into OptionsDialog.cpp by tools/patch_engine.py; laid out in code,
// so it needs no .res file from the GMod content.
#ifndef GMP_OPTIONS_DEBUG_H
#define GMP_OPTIONS_DEBUG_H

#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Button.h>
#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"

// The on-screen developer text (console lines in the top left). The app no longer launches with
// -dev 2, so this archived switch is what turns developer on, and it survives restarts.
static void GMPDebugTextChanged( IConVar *var, const char *, float )
{
	ConVarRef developer( "developer" );
	if ( developer.IsValid() )
		developer.SetValue( ConVarRef( var ).GetBool() ? 2 : 0 );
}
static ConVar gmp_debug_text( "gmp_debug_text", "0", FCVAR_ARCHIVE,
	"Developer messages on screen (sets developer 2 / 0, saved)", GMPDebugTextChanged );

class CGMPOptionsSubDebug : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CGMPOptionsSubDebug, vgui::PropertyPage );

	// Applied in this order: sv_cheats before the cheat-protected material switches.
	enum { NUM_TOGGLES = 10 };
	CCvarToggleCheckButton *m_pToggles[ NUM_TOGGLES ];
	vgui::Button *m_pConsole, *m_pDebugMenu;

public:
	CGMPOptionsSubDebug( vgui::Panel *parent ) : BaseClass( parent, "GMPOptionsSubDebug" )
	{
		static const char *const s_Toggles[ NUM_TOGGLES ][ 2 ] =
		{
			{ "cl_showfps", "Show FPS" },
			{ "cl_showpos", "Show position, angles and speed" },
			{ "net_graph", "Net graph" },
			{ "gmp_debug_text", "Developer messages on screen (saved)" },
			{ "sv_cheats", "Allow cheats (sv_cheats, this game only)" },
			{ "snd_show", "Playing sounds list (needs cheats)" },
			{ "mat_fullbright", "Fullbright (needs cheats)" },
			{ "mat_wireframe", "Wireframe (needs cheats)" },
			{ "r_drawviewmodel", "Draw view model" },
			{ "gmp_construct_watcher", "gm_construct watcher" },
		};
		for ( int i = 0; i < NUM_TOGGLES; i++ )
			m_pToggles[ i ] = new CCvarToggleCheckButton( this, s_Toggles[ i ][ 0 ], s_Toggles[ i ][ 1 ], s_Toggles[ i ][ 0 ] );
		m_pConsole = new vgui::Button( this, "Console", "Open developer console", this, "GMPConsole" );
		m_pDebugMenu = new vgui::Button( this, "DebugMenu", "Debug menu (AI, NPC, entities)", this, "GMPDebugMenu" );
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();
		const int row = IsProportional() ? vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 24 ) : 24;
		const int x = row, w = GetWide() - 2 * row, step = row * 13 / 12;
		for ( int i = 0; i < NUM_TOGGLES; i++ )
			m_pToggles[ i ]->SetBounds( x, row / 2 + i * step, w, row );
		m_pConsole->SetBounds( x, row / 2 + NUM_TOGGLES * step + row / 4, w / 2 - row / 4, row );
		m_pDebugMenu->SetBounds( x + w / 2, row / 2 + NUM_TOGGLES * step + row / 4, w / 2, row );
	}

	virtual void OnResetData()
	{
		for ( int i = 0; i < NUM_TOGGLES; i++ )
			m_pToggles[ i ]->Reset();
	}

	virtual void OnApplyChanges()
	{
		for ( int i = 0; i < NUM_TOGGLES; i++ )
			m_pToggles[ i ]->ApplyChanges();
	}

	virtual void OnCommand( const char *command )
	{
		if ( !Q_stricmp( command, "GMPConsole" ) )
			engine->ClientCmd_Unrestricted( "showconsole\n" );
		else if ( !Q_stricmp( command, "GMPDebugMenu" ) )	// engine debug menu over the game; Debug Menu > Close hides it
			engine->ClientCmd_Unrestricted( "sv_cheats 1\ngameui_hide\ndebugsystemui 1\n" );
		else
			BaseClass::OnCommand( command );
	}

private:
	MESSAGE_FUNC( OnControlModified, "ControlModified" )
	{
		PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
	}
	MESSAGE_FUNC( OnCheckButtonChecked, "CheckButtonChecked" )
	{
		OnControlModified();
	}
};

#endif // GMP_OPTIONS_DEBUG_H
