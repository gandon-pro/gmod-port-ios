// GModPort: Options > Debug. Debug overlays and switches that otherwise need the console
// (personal build). Included into OptionsDialog.cpp by tools/patch_engine.py; laid out in code,
// so it needs no .res file from the GMod content.
#ifndef GMP_OPTIONS_DEBUG_H
#define GMP_OPTIONS_DEBUG_H

#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Button.h>
#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"

class CGMPOptionsSubDebug : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( CGMPOptionsSubDebug, vgui::PropertyPage );

	// Applied in this order: sv_cheats before the cheat-protected material switches.
	enum { NUM_TOGGLES = 9 };
	CCvarToggleCheckButton *m_pToggles[ NUM_TOGGLES ];
	vgui::Button *m_pConsole;

public:
	CGMPOptionsSubDebug( vgui::Panel *parent ) : BaseClass( parent, "GMPOptionsSubDebug" )
	{
		static const char *const s_Toggles[ NUM_TOGGLES ][ 2 ] =
		{
			{ "cl_showfps", "Show FPS" },
			{ "cl_showpos", "Show position, angles and speed" },
			{ "net_graph", "Net graph" },
			{ "developer", "Developer messages on screen" },
			{ "sv_cheats", "Allow cheats (sv_cheats, this game only)" },
			{ "mat_fullbright", "Fullbright (needs cheats)" },
			{ "mat_wireframe", "Wireframe (needs cheats)" },
			{ "r_drawviewmodel", "Draw view model" },
			{ "gmp_construct_watcher", "gm_construct watcher" },
		};
		for ( int i = 0; i < NUM_TOGGLES; i++ )
			m_pToggles[ i ] = new CCvarToggleCheckButton( this, s_Toggles[ i ][ 0 ], s_Toggles[ i ][ 1 ], s_Toggles[ i ][ 0 ] );
		m_pConsole = new vgui::Button( this, "Console", "Open developer console", this, "GMPConsole" );
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();
		const int row = IsProportional() ? vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 24 ) : 24;
		const int x = row, w = GetWide() - 2 * row, step = row * 13 / 12;
		for ( int i = 0; i < NUM_TOGGLES; i++ )
			m_pToggles[ i ]->SetBounds( x, row / 2 + i * step, w, row );
		m_pConsole->SetBounds( x, row / 2 + NUM_TOGGLES * step + row / 4, w / 2, row );
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
