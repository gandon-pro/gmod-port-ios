//========= GModPort ==========================================================//
//
// Purpose: GMod-style physgun (weapon_physgun) and a one-item test spawn menu.
//          Included at the end of weapon_physcannon.cpp to reuse CGrabController.
//
//          +attack   grab a physics object and carry it at the grab distance
//          +attack2  while carrying: freeze it in place
//          +menu     toggle the spawn menu (gmp_spawn <model> on the server)
//
//=============================================================================//

#define GMP_SPAWN_TEST_MODEL "models/props_junk/popcan01a.mdl"

#ifdef CLIENT_DLL
#define CWeaponGMPPhysGun C_WeaponGMPPhysGun
#endif

class CWeaponGMPPhysGun : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponGMPPhysGun, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponGMPPhysGun( void )
	{
#ifndef CLIENT_DLL
		m_flDistance = 0;
		m_bWaitForRelease = false;
#endif
	}

	void	Precache( void );
	bool	HasAnyAmmo( void ) { return true; }
	void	PrimaryAttack( void ) {}
	void	SecondaryAttack( void ) {}
	void	ItemPostFrame( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	void	Drop( const Vector &vecVelocity );
	void	UpdateOnRemove( void );

private:
	void	Release( bool bFreeze );

#ifndef CLIENT_DLL
	void	Grab( CBasePlayer *pOwner );
	void	Carry( CBasePlayer *pOwner );

	CGrabController	m_grab;
	float			m_flDistance;
	bool			m_bWaitForRelease;	// after a freeze, ignore +attack until it is let go

	DECLARE_ACTTABLE();
#endif

	CWeaponGMPPhysGun( const CWeaponGMPPhysGun & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGMPPhysGun, DT_WeaponGMPPhysGun )

BEGIN_NETWORK_TABLE( CWeaponGMPPhysGun, DT_WeaponGMPPhysGun )
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponGMPPhysGun )
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_physgun, CWeaponGMPPhysGun );
PRECACHE_WEAPON_REGISTER( weapon_physgun );

#ifndef CLIENT_DLL
acttable_t CWeaponGMPPhysGun::m_acttable[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PHYSGUN,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PHYSGUN,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PHYSGUN,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PHYSGUN,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PHYSGUN,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PHYSGUN,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PHYSGUN,					false },
};
IMPLEMENT_ACTTABLE( CWeaponGMPPhysGun );
#endif

void CWeaponGMPPhysGun::Precache( void )
{
	BaseClass::Precache();
	// Spawned props must not be a late precache.
	PrecacheModel( GMP_SPAWN_TEST_MODEL );
}

void CWeaponGMPPhysGun::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

#ifndef CLIENT_DLL
	if ( !( pOwner->m_nButtons & IN_ATTACK ) )
	{
		m_bWaitForRelease = false;
		Release( false );
	}
	else if ( m_grab.GetAttached() )
	{
		if ( pOwner->m_afButtonPressed & IN_ATTACK2 )
		{
			Release( true );
			m_bWaitForRelease = true;
		}
		else
		{
			Carry( pOwner );
		}
	}
	else if ( !m_bWaitForRelease )
	{
		Grab( pOwner );
	}
#endif

	WeaponIdle();
}

bool CWeaponGMPPhysGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	Release( false );
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponGMPPhysGun::Drop( const Vector &vecVelocity )
{
	Release( false );
	BaseClass::Drop( vecVelocity );
}

void CWeaponGMPPhysGun::UpdateOnRemove( void )
{
	Release( false );
	BaseClass::UpdateOnRemove();
}

void CWeaponGMPPhysGun::Release( bool bFreeze )
{
#ifndef CLIENT_DLL
	CBaseEntity *pEntity = m_grab.GetAttached();
	if ( !pEntity )
		return;
	m_grab.DetachEntity( bFreeze );
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( bFreeze && pPhys )
		pPhys->EnableMotion( false );
	Pickup_OnPhysGunDrop( pEntity, ToBasePlayer( GetOwner() ), DROPPED_BY_CANNON );
#endif
}

#ifndef CLIENT_DLL
void CWeaponGMPPhysGun::Grab( CBasePlayer *pOwner )
{
	Vector start = pOwner->Weapon_ShootPosition();
	Vector forward;
	pOwner->EyeVectors( &forward );

	trace_t tr;
	UTIL_TraceLine( start, start + forward * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );
	CBaseEntity *pEntity = tr.m_pEnt;
	if ( !pEntity || pEntity->IsWorld() || pEntity->IsPlayer() || pEntity->GetMoveType() != MOVETYPE_VPHYSICS )
		return;
	IPhysicsObject *pPhys = pEntity->VPhysicsGetObject();
	if ( !pPhys )
		return;

	// Frozen objects (EnableMotion(false)) unfreeze when grabbed again, as in GMod.
	pPhys->EnableMotion( true );
	Pickup_OnPhysGunPickup( pEntity, pOwner, PICKED_UP_BY_CANNON );
	m_grab.SetIgnorePitch( false );
	m_grab.SetAngleAlignment( 0 );
	m_grab.AttachEntity( pOwner, pEntity, pPhys, false, tr.endpos, false );
	// Hold the object by the point that was hit, not by its center.
	VectorITransform( tr.endpos, pEntity->EntityToWorldTransform(), m_grab.m_attachedPositionObjectSpace );
	m_flDistance = ( tr.endpos - start ).Length();
}

void CWeaponGMPPhysGun::Carry( CBasePlayer *pOwner )
{
	CBaseEntity *pEntity = m_grab.GetAttached();
	if ( !pEntity || !pEntity->VPhysicsGetObject() )
	{
		Release( false );
		return;
	}

	Vector forward;
	pOwner->EyeVectors( &forward );
	Vector end = pOwner->Weapon_ShootPosition() + forward * m_flDistance;

	QAngle angles = m_grab.TransformAnglesFromPlayerSpace( m_grab.m_attachedAnglesPlayerSpace, pOwner );
	matrix3x4_t attachedToWorld;
	Vector offset;
	AngleMatrix( angles, attachedToWorld );
	VectorRotate( m_grab.m_attachedPositionObjectSpace, attachedToWorld, offset );
	m_grab.SetTargetPosition( end - offset, angles );
}

CON_COMMAND( gmp_spawn, "GModPort: gmp_spawn <models/...mdl> - spawn a physics prop where you look" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 2 )
		return;
	const char *pModel = args[1];
	if ( Q_strnicmp( pModel, "models/", 7 ) || Q_strstr( pModel, ".." ) )
		return;

	Vector start = pPlayer->EyePosition();
	Vector forward;
	pPlayer->EyeVectors( &forward );
	trace_t tr;
	UTIL_TraceLine( start, start + forward * 512, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	CBaseEntity *pProp = CreateEntityByName( "prop_physics" );
	if ( !pProp )
		return;
	pProp->KeyValue( "model", pModel );
	pProp->SetAbsOrigin( tr.endpos + tr.plane.normal * 8 );
	pProp->SetAbsAngles( QAngle( 0, pPlayer->EyeAngles().y + 180, 0 ) );
	if ( DispatchSpawn( pProp ) < 0 )
	{
		UTIL_Remove( pProp );
		return;
	}
	pProp->Activate();
	Msg( "gmp_spawn: %s at %.0f %.0f %.0f\n", pModel, tr.endpos.x, tr.endpos.y, tr.endpos.z );
}
#endif // !CLIENT_DLL

#ifdef CLIENT_DLL
#include "vgui_controls/Frame.h"
#include "vgui_controls/Button.h"
#include "ienginevgui.h"

class CGMPSpawnMenu : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CGMPSpawnMenu, vgui::Frame );
public:
	CGMPSpawnMenu( vgui::VPANEL parent ) : BaseClass( NULL, "GMPSpawnMenu" )
	{
		SetParent( parent );
		SetTitle( "Spawn", true );
		SetSizeable( false );
		SetDeleteSelfOnClose( false );
		m_pCan = new vgui::Button( this, "PopCan", "Pop can", this, "spawn_popcan" );
		SetVisible( false );
	}

	void Toggle( void )
	{
		if ( IsVisible() )
		{
			Close();
			return;
		}
		int w = ScreenWidth() / 3, h = ScreenHeight() / 3;
		SetBounds( ( ScreenWidth() - w ) / 2, ( ScreenHeight() - h ) / 2, w, h );
		Activate();
	}

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();
		int w, h;
		GetSize( w, h );
		m_pCan->SetBounds( w / 8, h / 4, w * 3 / 4, h / 2 );
	}

	virtual void OnCommand( const char *command )
	{
		if ( !Q_strcmp( command, "spawn_popcan" ) )
		{
			engine->ClientCmd( "gmp_spawn " GMP_SPAWN_TEST_MODEL );
			Close();
			return;
		}
		BaseClass::OnCommand( command );
	}

private:
	vgui::Button *m_pCan;
};

static vgui::DHANDLE<CGMPSpawnMenu> g_hGMPSpawnMenu;

// +menu toggles rather than shows-while-held: a touch button cannot stay pressed while
// another finger taps the menu.
static void GMP_SpawnMenuDown( const CCommand &args )
{
	if ( !g_hGMPSpawnMenu.Get() )
		g_hGMPSpawnMenu = new CGMPSpawnMenu( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
	g_hGMPSpawnMenu->Toggle();
}
static void GMP_SpawnMenuUp( const CCommand &args ) {}
static ConCommand gmp_menu_down( "+menu", GMP_SpawnMenuDown, "GModPort: toggle the spawn menu" );
static ConCommand gmp_menu_up( "-menu", GMP_SpawnMenuUp );
#endif // CLIENT_DLL
