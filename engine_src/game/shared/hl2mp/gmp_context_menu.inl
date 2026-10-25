// GModPort: GMod's C menu (+menu_context). Personal use only.
// Like GMod's: a menu bar of option toggles (Drawing / NPCs / Server), desktop icons (Player Model)
// and, on a tap in the world, the entity properties of what is under the finger. Properties run
// "gmp_prop <entindex> <action>", the player editor sets cl_playermodel / cl_playerskin /
// cl_playerbodygroups (GMod's convars), which the server validates and applies.

#ifndef CLIENT_DLL
#include "ai_basenpc.h"

static bool GMP_PropTarget( CBasePlayer *pPlayer, CBaseEntity *pEnt )
{
	if ( !pEnt || pEnt->IsWorld() || pEnt->IsPlayer() || pEnt->edict() == NULL )
		return false;
	if ( pEnt->GetOwnerEntity() && pEnt->GetOwnerEntity()->IsPlayer() )
		return false;	// a held weapon or the player's own viewmodel
	return ( pEnt->WorldSpaceCenter() - pPlayer->EyePosition() ).LengthSqr() < 8192.0f * 8192.0f;
}

// gmp_prop <entindex> <remove|freeze|unfreeze|gravity|collision|ignite|skin>
CON_COMMAND( gmp_prop, "GModPort: gmp_prop <entindex> <action> - context menu property" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 3 )
		return;
	CBaseEntity *pEnt = UTIL_EntityByIndex( atoi( args[1] ) );
	if ( !GMP_PropTarget( pPlayer, pEnt ) )
		return;
	const char *action = args[2];
	IPhysicsObject *pPhys = pEnt->GetMoveType() == MOVETYPE_VPHYSICS ? pEnt->VPhysicsGetObject() : NULL;
	CBaseAnimating *pAnim = pEnt->GetBaseAnimating();
	if ( !Q_stricmp( action, "remove" ) )
		UTIL_Remove( pEnt );
	else if ( pPhys && ( !Q_stricmp( action, "freeze" ) || !Q_stricmp( action, "unfreeze" ) ) )
	{
		const bool freeze = !Q_stricmp( action, "freeze" );
		pPhys->EnableMotion( !freeze );
		if ( !freeze )
			pPhys->Wake();
	}
	else if ( pPhys && !Q_stricmp( action, "gravity" ) )
	{
		pPhys->EnableGravity( !pPhys->IsGravityEnabled() );
		pPhys->Wake();
	}
	else if ( !Q_stricmp( action, "collision" ) )
	{
		// GMod's "Disable Collisions" uses its COLLISION_GROUP_WORLD; SDK 2013's equivalent is DEBRIS (world and static only).
		if ( pEnt->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
			pEnt->SetCollisionGroup( pEnt->MyNPCPointer() ? COLLISION_GROUP_NPC : COLLISION_GROUP_NONE );
		else
			pEnt->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
		if ( pPhys )
			pPhys->Wake();
	}
	else if ( pAnim && !Q_stricmp( action, "ignite" ) )
	{
		if ( pAnim->IsOnFire() )
			pAnim->Extinguish();
		else
			pAnim->Ignite( 360.0f, false );
	}
	else if ( pAnim && !Q_stricmp( action, "skin" ) )
	{
		CStudioHdr *pHdr = pAnim->GetModelPtr();
		const int skins = pHdr ? pHdr->numskinfamilies() : 1;
		if ( skins > 1 )
			pAnim->m_nSkin = ( pAnim->m_nSkin + 1 ) % skins;
	}
}

// Menu bar > NPCs. GMod's names; SDK 2013 only has the ai_disable toggle and per-player notarget.
static void GMP_AIDisabledChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( static_cast<ConVar *>( var )->GetBool() )
		CAI_BaseNPC::m_nDebugBits |= bits_debugDisableAI;
	else
		CAI_BaseNPC::m_nDebugBits &= ~bits_debugDisableAI;
}
static ConVar ai_disabled( "ai_disabled", "0", 0, "GModPort: NPCs stop thinking", GMP_AIDisabledChanged );

static void GMP_SetNoTarget( CBasePlayer *pPlayer, bool ignore )
{
	if ( ignore )
		pPlayer->AddFlag( FL_NOTARGET );
	else
		pPlayer->RemoveFlag( FL_NOTARGET );
}
static void GMP_IgnorePlayersChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( i ) )
			GMP_SetNoTarget( pPlayer, static_cast<ConVar *>( var )->GetBool() );
}
static ConVar ai_ignoreplayers( "ai_ignoreplayers", "0", 0, "GModPort: NPCs ignore players", GMP_IgnorePlayersChanged );

// GMod's player models live in models/player/; HL2MP's own list stays valid too (ValidatePlayerModel).
bool GMP_AcceptPlayerModel( const char *pModel )
{
	if ( !pModel || Q_strnicmp( pModel, "models/player/", 14 ) || Q_strstr( pModel, ".." ) || !Q_stristr( pModel, ".mdl" ) )
		return false;
	if ( !filesystem->FileExists( pModel, "GAME" ) )
		return false;
	CBaseEntity::PrecacheModel( pModel );
	return true;
}

static void GMP_SetLook( CBaseAnimating *pAnim, int skin, const int *groups, int numGroups )
{
	CStudioHdr *pHdr = pAnim->GetModelPtr();
	if ( !pHdr )
		return;
	pAnim->m_nSkin = clamp( skin, 0, MAX( 0, pHdr->numskinfamilies() - 1 ) );
	for ( int i = 0; i < pHdr->numbodyparts(); i++ )
	{
		const int value = i < numGroups ? groups[i] : 0;
		pAnim->SetBodygroup( i, value >= 0 && value < pHdr->pBodypart( i )->nummodels ? value : 0 );
	}
}

static int GMP_ParseGroups( const char *text, int *groups, int maxGroups )
{
	int n = 0;
	while ( text && *text && n < maxGroups )
	{
		while ( *text == ' ' )
			text++;
		if ( !*text )
			break;
		groups[n++] = atoi( text );
		while ( *text && *text != ' ' )
			text++;
	}
	return n;
}

// Called by CHL2MP_Player::SetPlayerModel, which runs on every spawn and model change.
void GMP_ApplyPlayerLook( CBasePlayer *pPlayer )
{
	const int client = engine->IndexOfEdict( pPlayer->edict() );
	int groups[32];
	const int n = GMP_ParseGroups( engine->GetClientConVarValue( client, "cl_playerbodygroups" ), groups, ARRAYSIZE( groups ) );
	GMP_SetLook( pPlayer, atoi( engine->GetClientConVarValue( client, "cl_playerskin" ) ), groups, n );
	GMP_SetNoTarget( pPlayer, ai_ignoreplayers.GetBool() );	// a respawn clears it
}

// gmp_playerlook <skin> [bodygroup values...]: the editor's skin/bodygroup arrows, applied at once
// (the cl_ convars it also sets only reach the server as userinfo, and HL2MP rereads them on spawn).
CON_COMMAND( gmp_playerlook, "GModPort: gmp_playerlook <skin> [bodygroup values...]" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 2 )
		return;
	int groups[32];
	int n = 0;
	for ( int i = 2; i < args.ArgC() && n < ARRAYSIZE( groups ); i++ )
		groups[n++] = atoi( args[i] );
	GMP_SetLook( pPlayer, atoi( args[1] ), groups, n );
}
#endif // !CLIENT_DLL

#ifdef CLIENT_DLL
#include "iviewrender.h"
#include "view_shared.h"
#include "studio.h"

static ConVar cl_playerskin( "cl_playerskin", "0", FCVAR_USERINFO | FCVAR_ARCHIVE, "GModPort: player model skin" );
static ConVar cl_playerbodygroups( "cl_playerbodygroups", "0", FCVAR_USERINFO | FCVAR_ARCHIVE, "GModPort: player model bodygroups, space separated" );

// GMod's player_manager list (lua/includes/modules/player_manager.lua); missing files are skipped.
struct GMPPlayerModel { const char *title, *model; };
static const GMPPlayerModel s_GMPPlayerModels[] =
{
	{ "Alyx", "models/player/alyx.mdl" }, { "Breen", "models/player/breen.mdl" }, { "Eli", "models/player/eli.mdl" },
	{ "G-Man", "models/player/gman_high.mdl" }, { "Kleiner", "models/player/kleiner.mdl" }, { "Grigori", "models/player/monk.mdl" },
	{ "Odessa", "models/player/odessa.mdl" }, { "Barney", "models/player/barney.mdl" }, { "Magnusson", "models/player/magnusson.mdl" },
	{ "Mossman", "models/player/mossman.mdl" }, { "Mossman Arctic", "models/player/mossman_arctic.mdl" },
	{ "Combine", "models/player/combine_soldier.mdl" }, { "Prison Guard", "models/player/combine_soldier_prisonguard.mdl" },
	{ "Combine Elite", "models/player/combine_super_soldier.mdl" }, { "Metrocop", "models/player/police.mdl" },
	{ "Metrocop F", "models/player/police_fem.mdl" }, { "Stripped", "models/player/soldier_stripped.mdl" },
	{ "Charple", "models/player/charple.mdl" }, { "Corpse", "models/player/corpse1.mdl" }, { "Skeleton", "models/player/skeleton.mdl" },
	{ "Zombie", "models/player/zombie_classic.mdl" }, { "Fast Zombie", "models/player/zombie_fast.mdl" }, { "Zombine", "models/player/zombie_soldier.mdl" },
	{ "Citizen F1", "models/player/Group01/female_01.mdl" }, { "Citizen F2", "models/player/Group01/female_02.mdl" },
	{ "Citizen F3", "models/player/Group01/female_03.mdl" }, { "Citizen F4", "models/player/Group01/female_04.mdl" },
	{ "Citizen F5", "models/player/Group01/female_05.mdl" }, { "Citizen F6", "models/player/Group01/female_06.mdl" },
	{ "Rebel F1", "models/player/Group03/female_01.mdl" }, { "Rebel F2", "models/player/Group03/female_02.mdl" },
	{ "Rebel F3", "models/player/Group03/female_03.mdl" }, { "Rebel F4", "models/player/Group03/female_04.mdl" },
	{ "Rebel F5", "models/player/Group03/female_05.mdl" }, { "Rebel F6", "models/player/Group03/female_06.mdl" },
	{ "Citizen M1", "models/player/Group01/male_01.mdl" }, { "Citizen M2", "models/player/Group01/male_02.mdl" },
	{ "Citizen M3", "models/player/Group01/male_03.mdl" }, { "Citizen M4", "models/player/Group01/male_04.mdl" },
	{ "Citizen M5", "models/player/Group01/male_05.mdl" }, { "Citizen M6", "models/player/Group01/male_06.mdl" },
	{ "Citizen M7", "models/player/Group01/male_07.mdl" }, { "Citizen M8", "models/player/Group01/male_08.mdl" },
	{ "Citizen M9", "models/player/Group01/male_09.mdl" },
	{ "Rebel M1", "models/player/Group03/male_01.mdl" }, { "Rebel M2", "models/player/Group03/male_02.mdl" },
	{ "Rebel M3", "models/player/Group03/male_03.mdl" }, { "Rebel M4", "models/player/Group03/male_04.mdl" },
	{ "Rebel M5", "models/player/Group03/male_05.mdl" }, { "Rebel M6", "models/player/Group03/male_06.mdl" },
	{ "Rebel M7", "models/player/Group03/male_07.mdl" }, { "Rebel M8", "models/player/Group03/male_08.mdl" },
	{ "Rebel M9", "models/player/Group03/male_09.mdl" },
	{ "Medic M1", "models/player/Group03m/male_01.mdl" }, { "Medic M2", "models/player/Group03m/male_02.mdl" },
	{ "Medic M3", "models/player/Group03m/male_03.mdl" }, { "Medic M4", "models/player/Group03m/male_04.mdl" },
	{ "Medic M5", "models/player/Group03m/male_05.mdl" }, { "Medic M6", "models/player/Group03m/male_06.mdl" },
	{ "Medic M7", "models/player/Group03m/male_07.mdl" }, { "Medic M8", "models/player/Group03m/male_08.mdl" },
	{ "Medic M9", "models/player/Group03m/male_09.mdl" },
	{ "Medic F1", "models/player/Group03m/female_01.mdl" }, { "Medic F2", "models/player/Group03m/female_02.mdl" },
	{ "Medic F3", "models/player/Group03m/female_03.mdl" }, { "Medic F4", "models/player/Group03m/female_04.mdl" },
	{ "Medic F5", "models/player/Group03m/female_05.mdl" }, { "Medic F6", "models/player/Group03m/female_06.mdl" },
	{ "Refugee 1", "models/player/Group02/male_02.mdl" }, { "Refugee 2", "models/player/Group02/male_04.mdl" },
	{ "Refugee 3", "models/player/Group02/male_06.mdl" }, { "Refugee 4", "models/player/Group02/male_08.mdl" },
	{ "Hostage 1", "models/player/hostage/hostage_01.mdl" }, { "Hostage 2", "models/player/hostage/hostage_02.mdl" },
	{ "Hostage 3", "models/player/hostage/hostage_03.mdl" }, { "Hostage 4", "models/player/hostage/hostage_04.mdl" },
	{ "Arctic", "models/player/arctic.mdl" }, { "Gasmask", "models/player/gasmask.mdl" }, { "Guerilla", "models/player/guerilla.mdl" },
	{ "Leet", "models/player/leet.mdl" }, { "Phoenix", "models/player/phoenix.mdl" }, { "Riot", "models/player/riot.mdl" },
	{ "SWAT", "models/player/swat.mdl" }, { "Urban", "models/player/urban.mdl" }, { "Chell", "models/player/p2_chell.mdl" },
	{ "DoD German", "models/player/dod_german.mdl" }, { "DoD American", "models/player/dod_american.mdl" },
};

// Menu bar toggles, GMod's (lua/autorun/menubar.lua) where this engine has the convar; rows whose
// convar does not exist are left out, cheat convars are greyed out while sv_cheats is 0.
// With a value the row is a radio choice: checked while the convar holds it, tapping sets it.
struct GMPMenuToggle { const char *label, *cvar, *value; };
static const GMPMenuToggle s_GMPDrawingMenu[] = {
	{ "Draw HUD", "cl_drawhud" }, { "Shadows", "r_shadows" }, { "Detail Props", "r_drawdetailprops" },
	{ "Show FPS", "cl_showfps" }, { "Wireframe", "mat_wireframe" },
};
static const GMPMenuToggle s_GMPNPCMenu[] = {
	{ "Disable Thinking", "ai_disabled" }, { "Ignore Players", "ai_ignoreplayers" },
	{ "Join Player's Squad", "npc_citizen_auto_player_squad" },
};
static const GMPMenuToggle s_GMPServerMenu[] = {
	{ "Allow Cheats", "sv_cheats" }, { "Realistic Fall Damage", "mp_falldamage" }, { "Mega Gravity Gun", "physcannon_mega_enabled" },
};
// GMod's weapon colour presets (the physgun vmts list them).
static const GMPMenuToggle s_GMPPhysgunMenu[] = {
	{ "Default", "cl_weaponcolor", "0.30 1.80 2.10" }, { "White", "cl_weaponcolor", "1.50 2.20 1.90" },
	{ "Red", "cl_weaponcolor", "3.50 0.30 0.20" }, { "Yellow", "cl_weaponcolor", "2.80 1.70 0.40" },
	{ "Green", "cl_weaponcolor", "0.50 2.10 0.80" }, { "Forest Green", "cl_weaponcolor", "1.00 2.40 0.40" },
	{ "Lavender", "cl_weaponcolor", "0.50 0.60 3.10" }, { "Purple", "cl_weaponcolor", "1.40 0.60 2.50" },
	{ "Pink", "cl_weaponcolor", "1.90 0.60 1.10" },
};
static bool GMP_MenuItemChecked( const GMPMenuToggle &item, ConVarRef &var )
{
	if ( !item.value )
		return var.GetBool();
	Vector have, want;
	return GMP_ParseWeaponColor( var.GetString(), have ) && GMP_ParseWeaponColor( item.value, want ) && have.DistToSqr( want ) < 0.01f;
}
struct GMPMenu { const char *title; const GMPMenuToggle *items; int count; };
static const GMPMenu s_GMPMenus[] = {
	{ "Drawing", s_GMPDrawingMenu, ARRAYSIZE( s_GMPDrawingMenu ) },
	{ "NPCs", s_GMPNPCMenu, ARRAYSIZE( s_GMPNPCMenu ) },
	{ "Server", s_GMPServerMenu, ARRAYSIZE( s_GMPServerMenu ) },
	{ "Physgun", s_GMPPhysgunMenu, ARRAYSIZE( s_GMPPhysgunMenu ) },
};

static void GMP_DrawTextIn( vgui::HFont font, const char *text, int x0, int y0, int x1, int y1, Color color, bool center = true )
{
	wchar_t wide[128];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wide, sizeof( wide ) );
	int tw, th;
	vgui::surface()->GetTextSize( font, wide, tw, th );
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextColor( color );
	vgui::surface()->DrawSetTextPos( center ? ( x0 + x1 - tw ) / 2 : x0, ( y0 + y1 - th ) / 2 );
	vgui::surface()->DrawPrintText( wide, wcslen( wide ) );
}

static int GMP_TextWide( vgui::HFont font, const char *text )
{
	wchar_t wide[128];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, wide, sizeof( wide ) );
	int tw, th;
	vgui::surface()->GetTextSize( font, wide, tw, th );
	return tw;
}

// A list of rows (a menu bar dropdown or the entity properties): a child of the C menu drawn over
// its other children; a tap on a row is passed back to the owner.
class IGMPPopupOwner
{
public:
	virtual void OnPopupRow( int popup, int row ) = 0;
};

class CGMPListPopup : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGMPListPopup, vgui::Panel );
public:
	struct Row { CUtlString label; bool check, checked, disabled, red; };

	CGMPListPopup( vgui::Panel *parent, IGMPPopupOwner *owner, int id ) : BaseClass( parent, "GMPListPopup" ),
		m_pOwner( owner ), m_nId( id ), m_hFont( 0 ), m_hTitleFont( 0 ), m_nRowTall( 1 )
	{
		SetPaintBackgroundEnabled( false );
		SetVisible( false );
	}

	void SetFonts( vgui::HFont font, vgui::HFont title ) { m_hFont = font; m_hTitleFont = title; }
	void SetTitle( const char *title ) { m_Title = title; }
	CUtlVector<Row> &Rows( void ) { return m_Rows; }

	// Opens at (x, y), kept on screen.
	void Open( int x, int y, int wide, int rowTall )
	{
		m_nRowTall = MAX( 1, rowTall );
		const int tall = m_nRowTall * ( m_Rows.Count() + ( m_Title.IsEmpty() ? 0 : 1 ) );
		SetBounds( clamp( x, 0, MAX( 0, ScreenWidth() - wide ) ), clamp( y, 0, MAX( 0, ScreenHeight() - tall ) ), wide, tall );
		SetVisible( true );
		MoveToFront();
	}

	virtual void Paint( void )
	{
		int w, h;
		GetSize( w, h );
		const bool light = m_Title.IsEmpty();	// dropdowns look like GMod's DMenu, the properties keep the dark popup
		vgui::surface()->DrawSetColor( light ? Color( 245, 245, 245, 250 ) : Color( 40, 40, 40, 235 ) );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );
		int y = 0;
		if ( !m_Title.IsEmpty() )
		{
			vgui::surface()->DrawSetColor( 70, 120, 190, 240 );
			vgui::surface()->DrawFilledRect( 0, 0, w, m_nRowTall );
			GMP_DrawTextIn( m_hTitleFont, m_Title.Get(), 0, 0, w, m_nRowTall, Color( 255, 255, 255, 255 ) );
			y = m_nRowTall;
		}
		for ( int i = 0; i < m_Rows.Count(); i++, y += m_nRowTall )
		{
			const Row &row = m_Rows[i];
			vgui::surface()->DrawSetColor( light ? Color( 210, 210, 210, 255 ) : Color( 80, 80, 80, 255 ) );
			vgui::surface()->DrawLine( 0, y, w, y );
			Color text = row.red ? Color( 255, 120, 110, 255 ) : light ? Color( 30, 30, 30, 255 ) : Color( 235, 235, 235, 255 );
			if ( row.disabled )
				text = Color( 150, 150, 150, 255 );
			if ( !row.check )
			{
				GMP_DrawTextIn( m_hFont, row.label.Get(), 0, y, w, y + m_nRowTall, text );
				continue;
			}
			const int box = m_nRowTall / 2, bx = m_nRowTall / 4, by = y + ( m_nRowTall - box ) / 2;
			vgui::surface()->DrawSetColor( row.disabled ? Color( 170, 170, 170, 255 ) : Color( 90, 90, 90, 255 ) );
			vgui::surface()->DrawOutlinedRect( bx, by, bx + box, by + box );
			if ( row.checked )
			{
				vgui::surface()->DrawSetColor( row.disabled ? Color( 170, 170, 170, 255 ) : Color( 60, 130, 220, 255 ) );
				vgui::surface()->DrawFilledRect( bx + box / 5, by + box / 5, bx + box - box / 5, by + box - box / 5 );
			}
			GMP_DrawTextIn( m_hFont, row.label.Get(), bx * 2 + box, y, w, y + m_nRowTall, text, false );
		}
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		int x, y;
		vgui::input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );
		const int row = y / m_nRowTall - ( m_Title.IsEmpty() ? 0 : 1 );
		if ( row >= 0 && row < m_Rows.Count() && !m_Rows[row].disabled )
			m_pOwner->OnPopupRow( m_nId, row );
	}

private:
	IGMPPopupOwner *m_pOwner;
	int m_nId;
	vgui::HFont m_hFont, m_hTitleFont;
	int m_nRowTall;
	CUtlString m_Title;
	CUtlVector<Row> m_Rows;
};

// The model preview: CMDLPanel plus the bodygroup mask it has no setter for, turning slowly.
class CGMPModelPreview : public CMDLPanel
{
	DECLARE_CLASS_SIMPLE( CGMPModelPreview, CMDLPanel );
public:
	CGMPModelPreview( vgui::Panel *parent ) : BaseClass( parent, "GMPModelPreview" ) {}
	void SetBody( int body ) { m_RootMDL.m_MDL.m_nBody = body; }
	virtual void OnThink( void )
	{
		BaseClass::OnThink();
		SetModelAnglesAndPosition( QAngle( 0, fmodf( gpGlobals->realtime * 30.0f, 360.0f ), 0 ), vec3_origin );
	}
};

// Desktop > Player Model, GMod's PlayerEditor: the model grid, a turning preview and the skin and
// bodygroup arrows. Picking a model sets cl_playermodel (HL2MP applies it at once); skin and bodygroups
// go to cl_playerskin / cl_playerbodygroups and gmp_playerlook.
class CGMPPlayerEditor : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGMPPlayerEditor, vgui::Panel );
public:
	CGMPPlayerEditor( vgui::Panel *parent ) : BaseClass( parent, "GMPPlayerEditor" ), m_hFont( 0 ), m_hTitleFont( 0 ), m_nModel( -1 )
	{
		SetPaintBackgroundEnabled( false );
		for ( int i = 0; i < ARRAYSIZE( s_GMPPlayerModels ); i++ )
		{
			if ( !g_pFullFileSystem->FileExists( s_GMPPlayerModels[i].model, "GAME" ) )
				continue;
			GMPSpawnItem item;
			item.model = s_GMPPlayerModels[i].model;
			item.text = s_GMPPlayerModels[i].title;
			item.command.Format( "gmp_pm_select %d", i );
			item.skin = 0;
			m_Items.AddToTail( item );
		}
		m_pGrid = new CGMPSpawnGrid( this );
		m_pGrid->SetItems( &m_Items );
		m_pPreview = new CGMPModelPreview( this );
		m_pPreview->SetMouseInputEnabled( false );
		m_pPreview->SetKeyBoardInputEnabled( false );
		m_pPreview->SetBackgroundColor( 70, 70, 70 );
		SetVisible( false );
	}

	void SetFonts( vgui::HFont font, vgui::HFont title )
	{
		m_hFont = font;
		m_hTitleFont = title;
		m_pGrid->SetFonts( font, title );
	}

	void Open( void )
	{
		SyncFromConVars();
		SetVisible( true );
		InvalidateLayout( true );
	}

	// gmp_pm_select: a grid tap.
	void SelectModel( int index )
	{
		if ( index < 0 || index >= ARRAYSIZE( s_GMPPlayerModels ) )
			return;
		char cmd[256];
		Q_snprintf( cmd, sizeof( cmd ), "cl_playermodel %s; cl_playerskin 0; cl_playerbodygroups 0\n", s_GMPPlayerModels[index].model );
		engine->ClientCmd_Unrestricted( cmd );
		m_nModel = index;
		m_nSkin = 0;
		m_Groups.RemoveAll();
		LoadPreview();
	}

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();
		int w, h;
		GetSize( w, h );
		const int head = HeaderTall();
		m_pGrid->SetBounds( 0, head, w * 62 / 100, h - head );
		m_pPreview->SetBounds( w * 62 / 100, head * 2, w - w * 62 / 100, ( h - head ) * 55 / 100 - head );	// the model name above it
	}

	virtual void Paint( void )
	{
		int w, h;
		GetSize( w, h );
		const int head = HeaderTall();
		vgui::surface()->DrawSetColor( 50, 50, 50, 245 );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );
		vgui::surface()->DrawSetColor( 30, 30, 30, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, w, head );
		GMP_DrawTextIn( m_hTitleFont, "Player Model", head / 3, 0, w, head, Color( 255, 255, 255, 255 ), false );
		vgui::surface()->DrawSetColor( 200, 70, 60, 255 );
		vgui::surface()->DrawFilledRect( w - head * 3 / 2, 0, w, head );
		GMP_DrawTextIn( m_hFont, "X", w - head * 3 / 2, 0, w, head, Color( 255, 255, 255, 255 ) );

		if ( m_nModel >= 0 )
		{
			int px, py, pw, ph;
			m_pPreview->GetBounds( px, py, pw, ph );
			GMP_DrawTextIn( m_hTitleFont, s_GMPPlayerModels[m_nModel].title, px, py - head, px + pw, py, Color( 255, 255, 255, 255 ) );
		}
		for ( int r = 0; r < NumControlRows(); r++ )
		{
			int x0, y0, x1, y1;
			ControlRow( r, x0, y0, x1, y1 );
			char label[64], value[16];
			RowText( r, label, sizeof( label ), value, sizeof( value ) );
			const int arrow = y1 - y0;
			GMP_DrawTextIn( m_hFont, label, x0 + arrow / 4, y0, x1, y1, Color( 230, 230, 230, 255 ), false );
			vgui::surface()->DrawSetColor( 90, 90, 90, 255 );
			vgui::surface()->DrawFilledRect( x1 - arrow * 3, y0 + 2, x1 - arrow * 2, y1 - 2 );
			vgui::surface()->DrawFilledRect( x1 - arrow, y0 + 2, x1, y1 - 2 );
			GMP_DrawTextIn( m_hFont, "<", x1 - arrow * 3, y0, x1 - arrow * 2, y1, Color( 255, 255, 255, 255 ) );
			GMP_DrawTextIn( m_hFont, value, x1 - arrow * 2, y0, x1 - arrow, y1, Color( 255, 255, 255, 255 ) );
			GMP_DrawTextIn( m_hFont, ">", x1 - arrow, y0, x1, y1, Color( 255, 255, 255, 255 ) );
		}
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		int x, y, w, h;
		vgui::input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );
		GetSize( w, h );
		if ( y < HeaderTall() && x >= w - HeaderTall() * 3 / 2 )
		{
			SetVisible( false );
			return;
		}
		for ( int r = 0; r < NumControlRows(); r++ )
		{
			int x0, y0, x1, y1;
			ControlRow( r, x0, y0, x1, y1 );
			const int arrow = y1 - y0;
			if ( y < y0 || y >= y1 )
				continue;
			if ( x >= x1 - arrow * 3 && x < x1 - arrow * 2 )
				Step( r, -1 );
			else if ( x >= x1 - arrow && x < x1 )
				Step( r, 1 );
			return;
		}
	}

private:
	struct Group { int index, count; CUtlString name; };

	int HeaderTall( void ) { return GetTall() / 10; }
	int NumControlRows( void ) const { return m_nModel >= 0 ? 1 + m_Groups.Count() : 0; }

	// Under the preview; as many rows as fit.
	void ControlRow( int r, int &x0, int &y0, int &x1, int &y1 )
	{
		int px, py, pw, ph;
		m_pPreview->GetBounds( px, py, pw, ph );
		const int rows = MAX( 1, NumControlRows() );
		const int rowTall = MIN( GetTall() / 11, ( GetTall() - py - ph ) / rows );
		x0 = px; x1 = px + pw - rowTall / 4;
		y0 = py + ph + r * rowTall; y1 = y0 + rowTall;
	}

	void RowText( int r, char *label, int labelSize, char *value, int valueSize ) const
	{
		if ( r == 0 )
		{
			Q_strncpy( label, "Skin", labelSize );
			Q_snprintf( value, valueSize, "%d", m_nSkin );
			return;
		}
		const Group &g = m_Groups[r - 1];
		Q_strncpy( label, g.name.Get(), labelSize );
		Q_snprintf( value, valueSize, "%d", m_nGroupValues[g.index] );
	}

	void Step( int r, int delta )
	{
		if ( r == 0 )
			m_nSkin = ( m_nSkin + delta + m_nSkins ) % m_nSkins;
		else
		{
			const Group &g = m_Groups[r - 1];
			m_nGroupValues[g.index] = ( m_nGroupValues[g.index] + delta + g.count ) % g.count;
		}
		// GMod's cl_playerbodygroups: one value per bodygroup index, space separated.
		char groups[160] = "", cmd[256];
		for ( int i = 0; i < m_nNumParts; i++ )
			Q_snprintf( groups + Q_strlen( groups ), sizeof( groups ) - Q_strlen( groups ), i ? " %d" : "%d", m_nGroupValues[i] );
		Q_snprintf( cmd, sizeof( cmd ), "cl_playerskin %d; cl_playerbodygroups \"%s\"; gmp_playerlook %d %s\n", m_nSkin, groups, m_nSkin, groups );
		engine->ClientCmd_Unrestricted( cmd );
		ApplyPreviewLook();
	}

	void SyncFromConVars( void )
	{
		m_nModel = -1;
		ConVarRef model( "cl_playermodel" );
		for ( int i = 0; model.IsValid() && i < ARRAYSIZE( s_GMPPlayerModels ); i++ )
			if ( !Q_stricmp( model.GetString(), s_GMPPlayerModels[i].model ) )
				m_nModel = i;
		m_nSkin = cl_playerskin.GetInt();
		LoadPreview();
		int values[32];
		const int n = GMP_ParseGroupsClient( cl_playerbodygroups.GetString(), values, ARRAYSIZE( values ) );
		for ( int i = 0; i < m_nNumParts; i++ )
		{
			m_nGroupValues[i] = i < n ? values[i] : 0;
			if ( m_nGroupValues[i] < 0 || m_nGroupValues[i] >= m_nPartModels[i] )
				m_nGroupValues[i] = 0;
		}
		m_nSkin = clamp( m_nSkin, 0, m_nSkins - 1 );
		ApplyPreviewLook();
	}

	static int GMP_ParseGroupsClient( const char *text, int *groups, int maxGroups )
	{
		int n = 0;
		while ( text && *text && n < maxGroups )
		{
			while ( *text == ' ' )
				text++;
			if ( !*text )
				break;
			groups[n++] = atoi( text );
			while ( *text && *text != ' ' )
				text++;
		}
		return n;
	}

	// Loads the selected model into the preview and reads its skins, bodygroups and idle sequence.
	void LoadPreview( void )
	{
		m_Groups.RemoveAll();
		m_nSkins = 1;
		m_nNumParts = 0;
		if ( m_nModel < 0 )
		{
			m_pPreview->SetMDL( MDLHANDLE_INVALID );
			return;
		}
		MDLHandle_t hMDL = mdlcache->FindMDL( s_GMPPlayerModels[m_nModel].model );
		studiohdr_t *pStudio = mdlcache->GetStudioHdr( hMDL );
		const bool ok = pStudio && !mdlcache->IsErrorModel( hMDL );
		m_pPreview->SetMDL( ok ? hMDL : MDLHANDLE_INVALID );
		mdlcache->Release( hMDL );
		if ( !ok )
			return;
		m_nSkins = MAX( 1, (int)pStudio->numskinfamilies );
		m_nNumParts = MIN( pStudio->numbodyparts, (int)ARRAYSIZE( m_nGroupValues ) );
		for ( int i = 0; i < m_nNumParts; i++ )
		{
			mstudiobodyparts_t *pPart = pStudio->pBodypart( i );
			m_nPartBase[i] = pPart->base;
			m_nPartModels[i] = MAX( 1, pPart->nummodels );
			m_nGroupValues[i] = 0;
			if ( pPart->nummodels > 1 )
			{
				Group g;
				g.index = i;
				g.count = pPart->nummodels;
				g.name = pPart->pszName();
				m_Groups.AddToTail( g );
			}
		}
		// GMod's editor plays the idle of the player animations ($includemodel'd, hence CStudioHdr).
		CStudioHdr hdr( pStudio, mdlcache );
		int seq = 0;
		for ( int i = 0; i < hdr.GetNumSeq(); i++ )
		{
			if ( !Q_stricmp( hdr.pSeqdesc( i ).pszLabel(), "idle_all_01" ) )
			{
				seq = i;
				break;
			}
		}
		m_pPreview->SetSequence( seq, true );
		m_pPreview->SetCameraPositionAndAngles( vec3_origin, QAngle( 0, 180, 0 ) );
		m_pPreview->LookAtMDL();
		ApplyPreviewLook();
	}

	void ApplyPreviewLook( void )
	{
		int body = 0;
		for ( int i = 0; i < m_nNumParts; i++ )
			body += m_nGroupValues[i] * m_nPartBase[i];
		m_pPreview->SetSkin( m_nSkin );
		m_pPreview->SetBody( body );
	}

	CUtlVector<GMPSpawnItem> m_Items;
	CGMPSpawnGrid *m_pGrid;
	CGMPModelPreview *m_pPreview;
	vgui::HFont m_hFont, m_hTitleFont;
	int m_nModel, m_nSkin, m_nSkins, m_nNumParts;
	int m_nGroupValues[32], m_nPartBase[32], m_nPartModels[32];
	CUtlVector<Group> m_Groups;
};

// Hits every entity but the local player, whatever its collision group: a prop whose collisions
// were disabled (COLLISION_GROUP_DEBRIS) must stay pickable to turn them back on.
class CGMPPickFilter : public ITraceFilter
{
public:
	virtual bool ShouldHitEntity( IHandleEntity *pHandle, int contentsMask )
	{
		IClientUnknown *pUnk = static_cast<IClientUnknown *>( pHandle );
		C_BaseEntity *pEnt = pUnk ? pUnk->GetBaseEntity() : NULL;
		return pEnt && pEnt != C_BasePlayer::GetLocalPlayer() && !pEnt->IsPlayer();
	}
	virtual TraceType_t GetTraceType() const { return TRACE_EVERYTHING; }
};

class CGMPContextMenu : public vgui::Panel, public IGMPPopupOwner
{
	DECLARE_CLASS_SIMPLE( CGMPContextMenu, vgui::Panel );
public:
	enum { POPUP_PROPERTIES = -1 };	// dropdowns use their menu index

	CGMPContextMenu( vgui::VPANEL parent ) : BaseClass( NULL, "GMPContextMenu" ), m_hFont( 0 ), m_hTitleFont( 0 ), m_nEnt( -1 ),
		m_nOpenMenu( -1 ), m_nIconTexture( -1 )
	{
		SetParent( parent );
		MakePopup( false );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( true );
		SetPaintBackgroundEnabled( false );
		SetVisible( false );
		m_pEditor = new CGMPPlayerEditor( this );
		m_pDropdown = new CGMPListPopup( this, this, 0 );
		m_pProperties = new CGMPListPopup( this, this, POPUP_PROPERTIES );
	}

	void Toggle( void )
	{
		ClosePopups();
		if ( IsVisible() )
		{
			SetVisible( false );
			return;
		}
		SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
		SetVisible( true );
		MoveToFront();
		InvalidateLayout( true );
	}

	CGMPPlayerEditor *Editor( void ) { return m_pEditor; }

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		const char *family = vgui::surface()->GetFontFamilyName( pScheme->GetFont( "Default", false ) );
		if ( !family || !*family )
			family = "Verdana";
		m_hFont = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( m_hFont, family, ScreenHeight() / 30, 600, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS );
		m_hTitleFont = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( m_hTitleFont, family, ScreenHeight() / 34, 800, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS );
		m_hSmallFont = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( m_hSmallFont, family, ScreenHeight() / 40, 700, 0, 0,
			vgui::ISurface::FONTFLAG_ANTIALIAS | vgui::ISurface::FONTFLAG_DROPSHADOW );
		m_pEditor->SetFonts( m_hFont, m_hTitleFont );
		m_pDropdown->SetFonts( m_hFont, m_hTitleFont );
		m_pProperties->SetFonts( m_hFont, m_hTitleFont );
	}

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();
		const int w = ScreenWidth(), h = ScreenHeight();
		const int gap = h / 40;
		m_pEditor->SetBounds( Inset() + IconTile() + 2 * gap, BarTall() + gap, w - 2 * Inset() - IconTile() - 2 * gap, h - BarTall() - 2 * gap );
	}

	virtual void OnThink( void )
	{
		if ( m_nEnt >= 0 && !ClientEntityList().GetBaseEntity( m_nEnt ) )
		{
			m_nEnt = -1;	// removed (by us or anything else) while the popup was open
			m_pProperties->SetVisible( false );
		}
	}

	virtual void Paint( void )
	{
		const int w = ScreenWidth(), h = ScreenHeight(), bar = BarTall();

		// Menu bar.
		vgui::surface()->DrawSetColor( 236, 236, 236, 245 );
		vgui::surface()->DrawFilledRect( 0, 0, w, bar );
		for ( int i = 0; i < ARRAYSIZE( s_GMPMenus ); i++ )
		{
			int x0, x1;
			MenuButton( i, x0, x1 );
			if ( i == m_nOpenMenu )
			{
				vgui::surface()->DrawSetColor( 190, 210, 240, 255 );
				vgui::surface()->DrawFilledRect( x0, 0, x1, bar );
			}
			GMP_DrawTextIn( m_hFont, s_GMPMenus[i].title, x0, 0, x1, bar, Color( 30, 30, 30, 255 ) );
		}
		int x0, y0, x1, y1;
		CloseRect( x0, y0, x1, y1 );
		vgui::surface()->DrawSetColor( 200, 70, 60, 240 );
		vgui::surface()->DrawFilledRect( x0, y0, x1, y1 );
		GMP_DrawTextIn( m_hFont, "X", x0, y0, x1, y1, Color( 255, 255, 255, 255 ) );

		// Desktop icon.
		IconRect( x0, y0, x1, y1 );
		const int icon = ( x1 - x0 ) * 6 / 10, ix = ( x0 + x1 - icon ) / 2;
		if ( IconTexture() >= 0 )
		{
			vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTexture( m_nIconTexture );
			vgui::surface()->DrawTexturedRect( ix, y0, ix + icon, y0 + icon );
		}
		else
		{
			vgui::surface()->DrawSetColor( 70, 120, 190, 230 );
			vgui::surface()->DrawFilledRect( ix, y0, ix + icon, y0 + icon );
		}
		GMP_DrawTextIn( m_hSmallFont, "Player Model", x0 - icon, y0 + icon, x1 + icon, y1, Color( 255, 255, 255, 255 ) );

		if ( m_nEnt < 0 && !m_pEditor->IsVisible() )
			GMP_DrawTextIn( m_hTitleFont, "Tap an object for its properties", 0, h - bar, w, h, Color( 255, 255, 255, 200 ) );

		// Outline what the properties apply to, like GMod's halo on the hovered entity.
		C_BaseEntity *pEnt = m_nEnt >= 0 ? ClientEntityList().GetBaseEntity( m_nEnt ) : NULL;
		if ( pEnt )
		{
			Vector mins, maxs;
			pEnt->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
			int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
			bool onScreen = true;
			for ( int i = 0; i < 8; i++ )
			{
				Vector c( i & 1 ? maxs.x : mins.x, i & 2 ? maxs.y : mins.y, i & 4 ? maxs.z : mins.z );
				int sx, sy;
				onScreen &= GetVectorInScreenSpace( c, sx, sy );
				bx0 = MIN( bx0, sx ); by0 = MIN( by0, sy ); bx1 = MAX( bx1, sx ); by1 = MAX( by1, sy );
			}
			if ( onScreen )
			{
				vgui::surface()->DrawSetColor( 120, 190, 255, 220 );
				vgui::surface()->DrawOutlinedRect( bx0, by0, bx1, by1 );
			}
		}
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		int x, y;
		vgui::input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );
		const bool hadPopup = m_pDropdown->IsVisible();
		const int wasOpen = m_nOpenMenu;
		ClosePopups();

		int x0, y0, x1, y1;
		CloseRect( x0, y0, x1, y1 );
		if ( x >= x0 && x < x1 && y >= y0 && y < y1 )
		{
			Toggle();
			return;
		}
		if ( y < BarTall() )
		{
			for ( int i = 0; i < ARRAYSIZE( s_GMPMenus ); i++ )
			{
				MenuButton( i, x0, x1 );
				if ( x >= x0 && x < x1 && i != wasOpen )
					OpenMenu( i );
			}
			return;
		}
		if ( hadPopup )
			return;	// the tap only closed the dropdown
		IconRect( x0, y0, x1, y1 );
		if ( x >= x0 && x < x1 && y >= y0 && y < y1 )
		{
			if ( m_pEditor->IsVisible() )
				m_pEditor->SetVisible( false );
			else
				m_pEditor->Open();
			return;
		}
		Pick( x, y );
	}

	virtual void OnPopupRow( int popup, int row )
	{
		if ( popup == POPUP_PROPERTIES )
		{
			char cmd[64];
			Q_snprintf( cmd, sizeof( cmd ), "gmp_prop %d %s\n", m_nEnt, m_Actions[row] );
			engine->ClientCmd_Unrestricted( cmd );
			ClosePopups();
			return;
		}
		const GMPMenuToggle &item = *m_MenuRows[row];
		ConVarRef var( item.cvar );
		if ( item.value )
		{
			var.SetValue( item.value );
			for ( int i = 0; i < m_pDropdown->Rows().Count(); i++ )
				m_pDropdown->Rows()[i].checked = ( i == row );
			return;
		}
		if ( var.IsValid() )
		{
			char cmd[96];
			Q_snprintf( cmd, sizeof( cmd ), "%s %d\n", item.cvar, var.GetBool() ? 0 : 1 );
			engine->ClientCmd_Unrestricted( cmd );
		}
		m_pDropdown->Rows()[row].checked = !m_pDropdown->Rows()[row].checked;	// the command runs next frame
	}

private:
	int BarTall( void ) const { return ScreenHeight() / 10; }
	int Inset( void ) const { return ScreenWidth() * 6 / 100; }	// clear of the notch in landscape
	int IconTile( void ) const { return ScreenHeight() / 6; }
	int RowTall( void ) const { return ScreenHeight() / 11; }
	int PopupWide( void ) const { return ScreenHeight() * 2 / 5; }

	void CloseRect( int &x0, int &y0, int &x1, int &y1 ) const
	{
		x1 = ScreenWidth() - Inset(); x0 = x1 - BarTall() * 3 / 2;
		y0 = 0; y1 = BarTall();
	}

	void MenuButton( int i, int &x0, int &x1 ) const
	{
		x0 = Inset();
		for ( int j = 0; ; j++ )
		{
			x1 = x0 + GMP_TextWide( m_hFont, s_GMPMenus[j].title ) + BarTall();
			if ( j == i )
				return;
			x0 = x1;
		}
	}

	void IconRect( int &x0, int &y0, int &x1, int &y1 ) const
	{
		x0 = Inset() + ScreenHeight() / 40; x1 = x0 + IconTile();
		y0 = BarTall() + ScreenHeight() / 40; y1 = y0 + IconTile();
	}

	// gmp/playermodel from make_port_assets.py (GMod's icon64/playermodel.png); without it a plain tile.
	int IconTexture( void )
	{
		if ( m_nIconTexture == -1 )
		{
			IMaterial *pMaterial = materials->FindMaterial( "gmp/playermodel", TEXTURE_GROUP_VGUI, false );
			if ( pMaterial && !pMaterial->IsErrorMaterial() )
			{
				m_nIconTexture = vgui::surface()->CreateNewTextureID();
				vgui::surface()->DrawSetTextureFile( m_nIconTexture, "gmp/playermodel", true, false );
			}
			else
				m_nIconTexture = -2;
		}
		return m_nIconTexture >= 0 ? m_nIconTexture : -1;
	}

	void ClosePopups( void )
	{
		m_pDropdown->SetVisible( false );
		m_pProperties->SetVisible( false );
		m_nOpenMenu = -1;
		m_nEnt = -1;
	}

	void OpenMenu( int i )
	{
		ConVarRef cheats( "sv_cheats" );
		m_pDropdown->Rows().RemoveAll();
		m_MenuRows.RemoveAll();
		for ( int j = 0; j < s_GMPMenus[i].count; j++ )
		{
			const GMPMenuToggle &item = s_GMPMenus[i].items[j];
			ConVarRef var( item.cvar, true );
			if ( !var.IsValid() )
				continue;
			CGMPListPopup::Row row;
			row.label = item.label;
			row.check = true;
			row.checked = GMP_MenuItemChecked( item, var );
			row.disabled = var.IsFlagSet( FCVAR_CHEAT ) && !( cheats.IsValid() && cheats.GetBool() );
			if ( row.disabled )
				row.label += " (cheats)";
			row.red = false;
			m_pDropdown->Rows().AddToTail( row );
			m_MenuRows.AddToTail( &item );
		}
		int x0, x1;
		MenuButton( i, x0, x1 );
		const int rows = MAX( 1, m_pDropdown->Rows().Count() );
		m_pDropdown->Open( x0, BarTall(), ScreenHeight() * 7 / 10, MIN( RowTall(), ( ScreenHeight() - BarTall() ) / rows ) );
		m_nOpenMenu = i;
	}

	// Ray through the tapped pixel of the player's view (fov there is already the horizontal
	// one, scaled for the aspect ratio by CViewRender::SetUpViews).
	void Pick( int x, int y )
	{
		m_nEnt = -1;
		const CViewSetup *pView = view->GetPlayerViewSetup();
		if ( !pView || pView->width <= 0 )
			return;
		Vector forward, right, up;
		AngleVectors( pView->angles, &forward, &right, &up );
		const float halfW = pView->width * 0.5f;
		const float dist = halfW / tanf( DEG2RAD( pView->fov * 0.5f ) );
		Vector dir = forward * dist + right * ( x - pView->x - halfW ) + up * ( pView->y + pView->height * 0.5f - y );
		VectorNormalize( dir );

		Ray_t ray;
		ray.Init( pView->origin, pView->origin + dir * 8192.0f );
		CGMPPickFilter filter;
		trace_t tr;
		enginetrace->TraceRay( ray, MASK_SHOT | CONTENTS_GRATE, &filter, &tr );
		C_BaseEntity *pEnt = tr.m_pEnt;
		if ( !pEnt || pEnt->IsWorld() || pEnt->entindex() <= 0 || pEnt->IsPlayer() )
			return;
		if ( pEnt->GetOwnerEntity() && pEnt->GetOwnerEntity()->IsPlayer() )
			return;

		const model_t *pModel = pEnt->GetModel();
		const char *name = pModel ? modelinfo->GetModelName( pModel ) : pEnt->GetClassname();
		char base[64];
		Q_FileBase( name, base, sizeof( base ) );
		m_pProperties->SetTitle( base );

		const bool physics = pEnt->GetMoveType() == MOVETYPE_VPHYSICS;
		const studiohdr_t *pStudio = pModel ? modelinfo->GetStudiomodel( pModel ) : NULL;
		m_pProperties->Rows().RemoveAll();
		m_Actions.RemoveAll();
		AddAction( "Remove", "remove", true );
		if ( physics )
		{
			AddAction( "Freeze", "freeze" );
			AddAction( "Unfreeze", "unfreeze" );
			AddAction( "Toggle Gravity", "gravity" );
		}
		AddAction( pEnt->GetCollisionGroup() == COLLISION_GROUP_DEBRIS ? "Enable Collisions" : "Disable Collisions", "collision" );
		if ( pEnt->GetBaseAnimating() )
			AddAction( pEnt->GetBaseAnimating()->IsOnFire() ? "Extinguish" : "Ignite", "ignite" );
		if ( pStudio && pStudio->numskinfamilies > 1 )
			AddAction( "Next Skin", "skin" );

		m_nEnt = pEnt->entindex();
		m_pProperties->Open( x + RowTall() / 3, y - RowTall() / 2, PopupWide(), RowTall() );
	}

	void AddAction( const char *label, const char *action, bool remove = false )
	{
		CGMPListPopup::Row row;
		row.label = label;
		row.check = row.checked = row.disabled = false;
		row.red = remove;
		m_pProperties->Rows().AddToTail( row );
		m_Actions.AddToTail( action );
	}

	vgui::HFont m_hFont, m_hTitleFont, m_hSmallFont;
	int m_nEnt, m_nOpenMenu, m_nIconTexture;
	CGMPPlayerEditor *m_pEditor;
	CGMPListPopup *m_pDropdown, *m_pProperties;
	CUtlVector<const char *> m_Actions;
	CUtlVector<const GMPMenuToggle *> m_MenuRows;
};

static vgui::DHANDLE<CGMPContextMenu> g_hGMPContextMenu;

static bool GMP_ContextMenuIsOpen( void )
{
	return g_hGMPContextMenu.Get() && g_hGMPContextMenu->IsVisible();
}

// Toggles like +menu: on the phone the C button cannot stay held while another finger taps.
static void GMP_ContextMenuDown( const CCommand &args )
{
	if ( !g_hGMPContextMenu.Get() )
	{
		g_hGMPContextMenu = new CGMPContextMenu( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
		g_pfnGMPTouchBlocked = GMP_SpawnMenuIsOpen;	// covers both menus
	}
	if ( g_hGMPSpawnMenu.Get() && g_hGMPSpawnMenu->IsVisible() )
		g_hGMPSpawnMenu->SetVisible( false );
	g_hGMPContextMenu->Toggle();
}
static void GMP_ContextMenuUp( const CCommand &args ) {}
static ConCommand gmp_context_down( "+menu_context", GMP_ContextMenuDown, "GModPort: toggle the context (C) menu" );
static ConCommand gmp_context_up( "-menu_context", GMP_ContextMenuUp );

// A tap in the player editor's grid (CGMPSpawnGrid runs the item's command).
CON_COMMAND_F( gmp_pm_select, "GModPort: gmp_pm_select <index> - pick a player model in the C menu", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() >= 2 && g_hGMPContextMenu.Get() )
		g_hGMPContextMenu->Editor()->SelectModel( atoi( args[1] ) );
}
#endif // CLIENT_DLL
