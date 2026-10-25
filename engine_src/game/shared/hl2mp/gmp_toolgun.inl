//========= GModPort ==========================================================//
//
// Purpose: GMod-style tool gun (gmod_tool). Included from gmp_physgun.inl.
//
//          gmod_toolmode (client, userinfo)  remover | weld | nocollide | rope | color
//          gmod_toolcolor (client, userinfo) "r g b" for the colour tool
//          +attack   use the tool (weld / nocollide / rope take two clicks; the second may hit the world)
//          +attack2  cancel the first click; colour tool: reset to white
//          +reload   remove this tool's constraints from the aimed object
//          gmp_toolnext / gmp_toolprev  cycle the tool (C menu > Tools lists them too)
//
//          Constraints live in server-only gmp_constraint entities, so gmod_undo removes them.
//
//=============================================================================//

#ifndef CLIENT_DLL
#include "rope.h"
#include "vphysics/constraints.h"
#endif

#ifdef CLIENT_DLL
#define CWeaponGMPToolGun C_WeaponGMPToolGun
#endif

#define GMP_TOOL_ROPE_MATERIAL	"cable/rope.vmt"
#define GMP_TOOL_SHOOT_SOUND	"Airboat.FireGunRevDown"	// GMod's tool shot (hl2_game_sounds_vehicles.txt)

enum GMPToolType_t { GMP_TOOL_REMOVER, GMP_TOOL_WELD, GMP_TOOL_NOCOLLIDE, GMP_TOOL_ROPE, GMP_TOOL_COLOR, GMP_TOOL_COUNT };

static const struct { const char *mode, *name, *first, *second, *reload; } s_GMPTools[GMP_TOOL_COUNT] =
{
	{ "remover", "Remover", "Fire: remove the object", NULL, NULL },
	{ "weld", "Weld", "Fire: pick the object to weld", "Fire: weld it to this (world too)", "Reload: remove welds" },
	{ "nocollide", "No Collide", "Fire: pick the first object", "Fire: it stops colliding with this", "Reload: collide again" },
	{ "rope", "Rope", "Fire: pick the first point", "Fire: tie a rope to this point", "Reload: remove ropes" },
	{ "color", "Colour", "Fire: paint (C menu > Tool Colour)", NULL, NULL },
};

static int GMP_ToolIndex( const char *mode )
{
	for ( int i = 0; mode && i < GMP_TOOL_COUNT; i++ )
		if ( !Q_stricmp( mode, s_GMPTools[i].mode ) )
			return i;
	return GMP_TOOL_WELD;
}

#ifdef CLIENT_DLL
static ConVar gmod_toolmode( "gmod_toolmode", "weld", FCVAR_USERINFO | FCVAR_ARCHIVE, "GModPort: tool gun mode (remover, weld, nocollide, rope, color)" );
static ConVar gmod_toolcolor( "gmod_toolcolor", "255 255 255", FCVAR_USERINFO | FCVAR_ARCHIVE, "GModPort: colour tool colour (r g b, 0-255)" );

static void GMP_CycleTool( int dir )
{
	const int next = ( GMP_ToolIndex( gmod_toolmode.GetString() ) + dir + GMP_TOOL_COUNT ) % GMP_TOOL_COUNT;
	gmod_toolmode.SetValue( s_GMPTools[next].mode );
}
CON_COMMAND( gmp_toolnext, "GModPort: next tool gun mode" ) { GMP_CycleTool( 1 ); }
CON_COMMAND( gmp_toolprev, "GModPort: previous tool gun mode" ) { GMP_CycleTool( -1 ); }
#endif

#ifndef CLIENT_DLL
// The physics object a trace hit: the bone's for ragdolls, the static world object for the world.
static IPhysicsObject *GMP_ToolPhys( CBaseEntity *pEnt, int bone )
{
	if ( !pEnt )
		return NULL;
	if ( pEnt->IsWorld() )
		return g_PhysWorldObject;
	IPhysicsObject *list[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	const int count = pEnt->VPhysicsGetObjectList( list, ARRAYSIZE( list ) );
	if ( count <= 0 )
		return NULL;
	return ( bone > 0 && bone < count ) ? list[bone] : list[0];
}

// Rope end: a networked point (no model) parented to the object, so the client rope follows it.
class CGMPRopeAnchor : public CBaseEntity
{
public:
	DECLARE_CLASS( CGMPRopeAnchor, CBaseEntity );
	int		ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	int		UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
};
LINK_ENTITY_TO_CLASS( gmp_rope_anchor, CGMPRopeAnchor );

// One tool constraint between two objects; removing it (undo, reload, either object gone) undoes it.
class CGMPConstraint : public CLogicalEntity
{
public:
	DECLARE_CLASS( CGMPConstraint, CLogicalEntity );

	CGMPConstraint( void ) : m_iType( 0 ), m_pConstraint( NULL ), m_bNoCollide( false ) { m_iBone[0] = m_iBone[1] = 0; }

	int		ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	bool	Attach( int type, CBaseEntity *pEnt0, int bone0, const Vector &pos0, CBaseEntity *pEnt1, int bone1, const Vector &pos1 );
	bool	Uses( CBaseEntity *pEnt ) const { return pEnt && ( m_hEnt[0].Get() == pEnt || m_hEnt[1].Get() == pEnt ); }
	void	WatchThink( void );
	void	UpdateOnRemove( void );

	int		m_iType;

private:
	CBaseEntity *CreateAnchor( CBaseEntity *pParent, const Vector &pos );

	EHANDLE				m_hEnt[2];
	int					m_iBone[2];
	IPhysicsConstraint	*m_pConstraint;
	bool				m_bNoCollide;
	EHANDLE				m_hRope, m_hAnchor[2];
};
LINK_ENTITY_TO_CLASS( gmp_constraint, CGMPConstraint );

CBaseEntity *CGMPConstraint::CreateAnchor( CBaseEntity *pParent, const Vector &pos )
{
	CBaseEntity *pAnchor = CreateEntityByName( "gmp_rope_anchor" );
	if ( !pAnchor )
		return NULL;
	pAnchor->SetAbsOrigin( pos );
	DispatchSpawn( pAnchor );
	if ( pParent && !pParent->IsWorld() )
		pAnchor->SetParent( pParent );
	return pAnchor;
}

bool CGMPConstraint::Attach( int type, CBaseEntity *pEnt0, int bone0, const Vector &pos0, CBaseEntity *pEnt1, int bone1, const Vector &pos1 )
{
	IPhysicsObject *pPhys0 = GMP_ToolPhys( pEnt0, bone0 ), *pPhys1 = GMP_ToolPhys( pEnt1, bone1 );
	if ( !physenv || !pPhys0 || !pPhys1 || pPhys0 == pPhys1 )
		return false;
	m_iType = type;
	m_hEnt[0] = pEnt0;
	m_hEnt[1] = pEnt1;
	m_iBone[0] = bone0;
	m_iBone[1] = bone1;

	if ( type == GMP_TOOL_WELD )
	{
		constraint_fixedparams_t fixed;
		fixed.Defaults();
		fixed.InitWithCurrentObjectState( pPhys1, pPhys0 );
		m_pConstraint = physenv->CreateFixedConstraint( pPhys1, pPhys0, NULL, fixed );
	}
	else if ( type == GMP_TOOL_ROPE )
	{
		constraint_lengthparams_t length;
		length.Defaults();
		length.InitWorldspace( pPhys1, pPhys0, pos1, pos0 );
		length.minLength = 0;
		m_pConstraint = physenv->CreateLengthConstraint( pPhys1, pPhys0, NULL, length );
		CBaseEntity *pA0 = CreateAnchor( pEnt0, pos0 ), *pA1 = CreateAnchor( pEnt1, pos1 );
		m_hAnchor[0] = pA0;
		m_hAnchor[1] = pA1;
		if ( pA0 && pA1 )
		{
			CRopeKeyframe *pRope = CRopeKeyframe::Create( pA0, pA1, 0, 0, 2, GMP_TOOL_ROPE_MATERIAL, 10 );
			if ( pRope )
				pRope->EnableWind( false );
			m_hRope = pRope;
		}
	}
	else if ( type == GMP_TOOL_NOCOLLIDE )
	{
		PhysDisableEntityCollisions( pPhys0, pPhys1 );
		m_bNoCollide = true;
	}
	if ( ( type == GMP_TOOL_WELD || type == GMP_TOOL_ROPE ) && !m_pConstraint )
		return false;
	pPhys0->Wake();
	pPhys1->Wake();

	SetThink( &CGMPConstraint::WatchThink );
	SetNextThink( gpGlobals->curtime + 0.5f );
	return true;
}

void CGMPConstraint::WatchThink( void )
{
	if ( !m_hEnt[0].Get() || !m_hEnt[1].Get() )
	{
		UTIL_Remove( this );
		return;
	}
	SetNextThink( gpGlobals->curtime + 0.5f );
}

void CGMPConstraint::UpdateOnRemove( void )
{
	// vphysics invalidates a constraint whose object was deleted; destroying it after that is safe.
	if ( m_pConstraint && physenv )
		physenv->DestroyConstraint( m_pConstraint );
	m_pConstraint = NULL;
	if ( m_bNoCollide )
	{
		IPhysicsObject *pPhys0 = GMP_ToolPhys( m_hEnt[0], m_iBone[0] ), *pPhys1 = GMP_ToolPhys( m_hEnt[1], m_iBone[1] );
		if ( pPhys0 && pPhys1 )
		{
			PhysEnableEntityCollisions( pPhys0, pPhys1 );
			pPhys0->Wake();
			pPhys1->Wake();
		}
		m_bNoCollide = false;
	}
	UTIL_Remove( m_hRope.Get() );
	UTIL_Remove( m_hAnchor[0].Get() );
	UTIL_Remove( m_hAnchor[1].Get() );
	BaseClass::UpdateOnRemove();
}
#endif // !CLIENT_DLL

class CWeaponGMPToolGun : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponGMPToolGun, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponGMPToolGun( void )
	{
		m_iStage = 0;
#ifndef CLIENT_DLL
		m_iTool = -1;
		m_iFirstBone = 0;
#endif
	}

	void	Precache( void );
	bool	HasAnyAmmo( void ) { return true; }
	void	PrimaryAttack( void ) {}
	void	SecondaryAttack( void ) {}
	void	ItemPostFrame( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
#ifdef CLIENT_DLL
	void	Redraw( void );
#endif

	CNetworkVar( int, m_iStage );	// 1: the first object of a two-click tool is picked

private:
	void	Fire( CBasePlayer *pOwner, int button );
#ifndef CLIENT_DLL
	void	SyncTool( CBasePlayer *pOwner );
	void	UseTool( CBasePlayer *pOwner, const trace_t &tr, int button );
	void	ClearStage( void ) { m_iStage = 0; m_hFirst = NULL; }

	int		m_iTool;
	EHANDLE	m_hFirst;
	int		m_iFirstBone;
	Vector	m_vecFirstLocal;	// the first click in the first object's space

	DECLARE_ACTTABLE();
#endif

	CWeaponGMPToolGun( const CWeaponGMPToolGun & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGMPToolGun, DT_WeaponGMPToolGun )

BEGIN_NETWORK_TABLE( CWeaponGMPToolGun, DT_WeaponGMPToolGun )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iStage ) ),
#else
	SendPropInt( SENDINFO( m_iStage ), 2, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponGMPToolGun )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( gmod_tool, CWeaponGMPToolGun );
PRECACHE_WEAPON_REGISTER( gmod_tool );

#ifndef CLIENT_DLL
acttable_t CWeaponGMPToolGun::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
};
IMPLEMENT_ACTTABLE( CWeaponGMPToolGun );
#endif

void CWeaponGMPToolGun::Precache( void )
{
	BaseClass::Precache();
	PrecacheScriptSound( GMP_TOOL_SHOOT_SOUND );
#ifndef CLIENT_DLL
	PrecacheModel( GMP_TOOL_ROPE_MATERIAL );
	UTIL_PrecacheOther( "keyframe_rope" );
#endif
}

void CWeaponGMPToolGun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;
#ifndef CLIENT_DLL
	SyncTool( pOwner );
#endif
	// The base class would "fire empty" (no ammo type); the tool acts on presses only.
	if ( gpGlobals->curtime >= m_flNextPrimaryAttack )
	{
		if ( pOwner->m_afButtonPressed & IN_ATTACK )
			Fire( pOwner, IN_ATTACK );
		else if ( pOwner->m_afButtonPressed & IN_ATTACK2 )
			Fire( pOwner, IN_ATTACK2 );
		else if ( pOwner->m_afButtonPressed & IN_RELOAD )
			Fire( pOwner, IN_RELOAD );
	}
	WeaponIdle();
}

bool CWeaponGMPToolGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	ClearStage();
#endif
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponGMPToolGun::Fire( CBasePlayer *pOwner, int button )
{
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
	if ( button == IN_ATTACK )
	{
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
		pOwner->SetAnimation( PLAYER_ATTACK1 );
#ifndef CLIENT_DLL
		// Server only, no prediction rules, emitted from the owner like CBaseCombatWeapon::WeaponSound
		// (a held weapon is not spatialized on the client, so its own sounds are dropped).
		CPASAttenuationFilter filter( pOwner, GMP_TOOL_SHOOT_SOUND );
		EmitSound( filter, pOwner->entindex(), GMP_TOOL_SHOOT_SOUND );
#endif
		m_flTimeWeaponIdle = gpGlobals->curtime + SequenceDuration();
	}

#ifndef CLIENT_DLL
	Vector start = pOwner->Weapon_ShootPosition();
	Vector forward, right, up;
	pOwner->EyeVectors( &forward, &right, &up );
	trace_t tr;
	UTIL_TraceLine( start, start + forward * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );
	if ( button == IN_ATTACK && tr.fraction < 1.0f )
	{
		UTIL_Tracer( start + forward * 16 + right * 6 - up * 4, tr.endpos, 0, TRACER_DONT_USE_ATTACHMENT, 6000, false, "AR2Tracer" );
		g_pEffects->Sparks( tr.endpos, 1, 1, &tr.plane.normal );
	}
	UseTool( pOwner, tr, button );
#endif
}

#ifndef CLIENT_DLL
void CWeaponGMPToolGun::SyncTool( CBasePlayer *pOwner )
{
	const int tool = GMP_ToolIndex( engine->GetClientConVarValue( pOwner->entindex(), "gmod_toolmode" ) );
	if ( tool != m_iTool )
	{
		m_iTool = tool;
		ClearStage();
	}
}

void CWeaponGMPToolGun::UseTool( CBasePlayer *pOwner, const trace_t &tr, int button )
{
	CBaseEntity *pEnt = tr.fraction < 1.0f ? tr.m_pEnt : NULL;
	const bool bTwoClicks = s_GMPTools[m_iTool].second != NULL;

	if ( button == IN_ATTACK2 )
	{
		if ( m_iTool == GMP_TOOL_COLOR && pEnt && !pEnt->IsWorld() && !pEnt->IsPlayer() )
			pEnt->SetRenderColor( 255, 255, 255 );
		ClearStage();
		return;
	}
	if ( button == IN_RELOAD )
	{
		if ( !bTwoClicks || !pEnt )
			return;
		int removed = 0;
		for ( CBaseEntity *p = gEntList.FindEntityByClassname( NULL, "gmp_constraint" ); p; p = gEntList.FindEntityByClassname( p, "gmp_constraint" ) )
		{
			CGMPConstraint *pC = static_cast<CGMPConstraint *>( p );
			if ( pC->m_iType == m_iTool && pC->Uses( pEnt ) && !( pC->GetEFlags() & EFL_KILLME ) )
			{
				UTIL_Remove( pC );
				removed++;
			}
		}
		ClearStage();
		if ( removed )
			ClientPrint( pOwner, HUD_PRINTCENTER, UTIL_VarArgs( "Removed %d %s", removed, s_GMPTools[m_iTool].name ) );
		return;
	}

	if ( !pEnt )
		return;
	if ( m_iTool == GMP_TOOL_REMOVER )
	{
		if ( pEnt->IsWorld() || pEnt->IsPlayer() || ( pEnt->GetOwnerEntity() && pEnt->GetOwnerEntity()->IsPlayer() ) )
			return;
		Msg( "gmod_tool remover: %s\n", pEnt->GetClassname() );
		UTIL_Remove( pEnt );
		return;
	}
	if ( m_iTool == GMP_TOOL_COLOR )
	{
		int r = 255, g = 255, b = 255;
		if ( pEnt->IsWorld() || sscanf( engine->GetClientConVarValue( pOwner->entindex(), "gmod_toolcolor" ), "%d %d %d", &r, &g, &b ) != 3 )
			return;
		pEnt->SetRenderColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ) );
		return;
	}

	// Two clicks: the first must be a physics object; the second may be the world (not for nocollide).
	IPhysicsObject *pPhys = GMP_ToolPhys( pEnt, tr.physicsbone );
	if ( !pPhys || pEnt->IsPlayer() || ( pEnt->IsWorld() && ( m_iStage == 0 || m_iTool == GMP_TOOL_NOCOLLIDE ) ) )
	{
		ClientPrint( pOwner, HUD_PRINTCENTER, m_iStage == 0 ? "Pick a physics object" : "Pick another physics object" );
		return;
	}
	if ( m_iStage == 0 || !m_hFirst.Get() )
	{
		m_hFirst = pEnt;
		m_iFirstBone = tr.physicsbone;
		pPhys->WorldToLocal( &m_vecFirstLocal, tr.endpos );
		m_iStage = 1;
		return;
	}

	CBaseEntity *pFirst = m_hFirst;
	IPhysicsObject *pFirstPhys = GMP_ToolPhys( pFirst, m_iFirstBone );
	if ( !pFirstPhys || pFirstPhys == pPhys )
	{
		ClientPrint( pOwner, HUD_PRINTCENTER, "Pick another object" );
		return;
	}
	Vector firstPos;
	pFirstPhys->LocalToWorld( &firstPos, m_vecFirstLocal );
	CGMPConstraint *pC = static_cast<CGMPConstraint *>( CreateEntityByName( "gmp_constraint" ) );
	if ( !pC )
		return;
	if ( !pC->Attach( m_iTool, pFirst, m_iFirstBone, firstPos, pEnt, tr.physicsbone, tr.endpos ) )
	{
		UTIL_Remove( pC );
		ClientPrint( pOwner, HUD_PRINTCENTER, "Could not constrain these objects" );
		ClearStage();
		return;
	}
	GMP_AddUndo( pOwner, pC, s_GMPTools[m_iTool].name );
	Msg( "gmod_tool %s: %s -> %s\n", s_GMPTools[m_iTool].mode, pFirst->GetClassname(), pEnt->GetClassname() );
	ClearStage();
}
#endif // !CLIENT_DLL

#ifdef CLIENT_DLL
// Tool name and controls in the top-left corner while the tool gun is out (CHudWeapon calls Redraw).
void CWeaponGMPToolGun::Redraw( void )
{
	BaseClass::Redraw();
	C_BasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner || !pOwner->IsLocalPlayer() )
		return;

	static vgui::HFont s_hTitle = vgui::INVALID_FONT, s_hText = vgui::INVALID_FONT;
	static int s_iFontTall = 0;
	const int h = ScreenHeight();
	if ( s_iFontTall != h )
	{
		vgui::IScheme *pScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetDefaultScheme() );
		const char *family = pScheme ? vgui::surface()->GetFontFamilyName( pScheme->GetFont( "Default", false ) ) : NULL;
		if ( !family || !*family )
			family = "Verdana";
		s_hTitle = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( s_hTitle, family, h / 22, 800, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS );
		s_hText = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( s_hText, family, h / 40, 600, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS );
		s_iFontTall = h;
	}

	const int tool = GMP_ToolIndex( gmod_toolmode.GetString() );
	const char *lines[3];
	int count = 0;
	lines[count++] = ( m_iStage == 1 && s_GMPTools[tool].second ) ? s_GMPTools[tool].second : s_GMPTools[tool].first;
	if ( s_GMPTools[tool].second )
		lines[count++] = "Alt fire: cancel";
	else if ( tool == GMP_TOOL_COLOR )
		lines[count++] = "Alt fire: reset to white";
	if ( s_GMPTools[tool].reload )
		lines[count++] = s_GMPTools[tool].reload;

	wchar_t title[64], text[3][96];
	g_pVGuiLocalize->ConvertANSIToUnicode( s_GMPTools[tool].name, title, sizeof( title ) );
	int w, th, lh;
	vgui::surface()->GetTextSize( s_hTitle, title, w, th );
	for ( int i = 0; i < count; i++ )
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( lines[i], text[i], sizeof( text[i] ) );
		int lw;
		vgui::surface()->GetTextSize( s_hText, text[i], lw, lh );
		w = MAX( w, lw );
	}
	lh = vgui::surface()->GetFontTall( s_hText );
	const int pad = h / 90, x = h / 40, y = h / 40;
	vgui::surface()->DrawSetColor( 0, 0, 0, 170 );
	vgui::surface()->DrawFilledRect( x, y, x + w + pad * 2, y + th + lh * count + pad * 2 );
	vgui::surface()->DrawSetColor( 0, 150, 255, 255 );
	vgui::surface()->DrawFilledRect( x, y, x + w + pad * 2, y + pad / 2 );

	vgui::surface()->DrawSetTextFont( s_hTitle );
	vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
	vgui::surface()->DrawSetTextPos( x + pad, y + pad );
	vgui::surface()->DrawPrintText( title, wcslen( title ) );
	vgui::surface()->DrawSetTextFont( s_hText );
	for ( int i = 0; i < count; i++ )
	{
		vgui::surface()->DrawSetTextColor( 220, 220, 220, 255 );
		vgui::surface()->DrawSetTextPos( x + pad, y + pad + th + lh * i );
		vgui::surface()->DrawPrintText( text[i], wcslen( text[i] ) );
	}
}
#endif // CLIENT_DLL
