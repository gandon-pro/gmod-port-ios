//========= GModPort ==========================================================//
//
// Purpose: GMod-style physgun (weapon_physgun) and spawn menu.
//          Included at the end of weapon_physcannon.cpp to reuse CGrabController.
//
//          +attack   grab a physics object and carry it at the grab distance
//          +attack2  while carrying: freeze it in place
//          +menu     toggle the spawn menu (gmp_spawn / gmp_ent / gmp_give / gmp_ammo on the server)
//
//=============================================================================//

#include "filesystem.h"
#include "ammodef.h"
#ifndef CLIENT_DLL
#include "gmp_npc_server.inl"
#endif

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
		m_bBeam = false;
#ifdef CLIENT_DLL
		m_pBeam = NULL;
#endif
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

#ifdef CLIENT_DLL
	~CWeaponGMPPhysGun( void );
	void	OnDataChanged( DataUpdateType_t updateType );
#endif

	CNetworkVar( bool, m_bBeam );		// carrying: draw the beam to m_vecBeamEnd
	CNetworkVector( m_vecBeamEnd );		// world position of the grabbed point

private:
#ifdef CLIENT_DLL
	class CGMPPhysBeam *m_pBeam;
#endif
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
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bBeam ) ),
	RecvPropVector( RECVINFO( m_vecBeamEnd ) ),
#else
	SendPropBool( SENDINFO( m_bBeam ) ),
	SendPropVector( SENDINFO( m_vecBeamEnd ), -1, SPROP_COORD ),
#endif
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
#ifndef CLIENT_DLL
	// gmp_ent spawns these mid-game; precache them with the map's weapons, not late.
	static const char *const s_Items[] =
	{
		"item_healthkit", "item_healthvial", "item_battery", "item_suit", "item_healthcharger",
		"item_suitcharger", "item_ammo_pistol", "item_ammo_pistol_large", "item_ammo_357",
		"item_ammo_357_large", "item_ammo_smg1", "item_ammo_smg1_large", "item_ammo_smg1_grenade",
		"item_ammo_ar2", "item_ammo_ar2_large", "item_ammo_ar2_altfire", "item_box_buckshot",
		"item_ammo_crossbow", "item_rpg_round",
	};
	for ( int i = 0; i < ARRAYSIZE( s_Items ); i++ )
		UTIL_PrecacheOther( s_Items[i] );
#endif
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
	else
	{
		// Grabs unless a freeze is waiting for release; draws the beam either way.
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
	m_bBeam = false;
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
	// Firing at nothing grabbable still shows the beam, ending where the shot hits (as in GMod).
	m_bBeam = true;
	m_vecBeamEnd = tr.endpos;
	CBaseEntity *pEntity = tr.m_pEnt;
	if ( m_bWaitForRelease || !pEntity || pEntity->IsWorld() || pEntity->IsPlayer() || pEntity->GetMoveType() != MOVETYPE_VPHYSICS )
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

	Vector grabbed;
	VectorTransform( m_grab.m_attachedPositionObjectSpace, pEntity->EntityToWorldTransform(), grabbed );
	m_vecBeamEnd = grabbed;
}

// gmp_spawn <models/...mdl> [skin]: spawn a prop where you look, resting on the surface as in
// GMod. Models without a collision model become a solid prop_dynamic instead of failing.
CON_COMMAND( gmp_spawn, "GModPort: gmp_spawn <models/...mdl> [skin] - spawn a prop where you look" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 2 )
		return;
	const char *pModel = args[1];
	if ( Q_strnicmp( pModel, "models/", 7 ) || Q_strstr( pModel, ".." ) || !filesystem->FileExists( pModel, "GAME" ) )
		return;

	Vector start = pPlayer->EyePosition();
	Vector forward;
	pPlayer->EyeVectors( &forward );
	trace_t tr;
	UTIL_TraceLine( start, start + forward * 1024, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	const model_t *pMdl = modelinfo->GetModel( CBaseEntity::PrecacheModel( pModel ) );
	const bool bPhysics = pMdl && modelinfo->GetVCollide( pMdl );
	CBaseEntity *pProp = CreateEntityByName( bPhysics ? "prop_physics_override" : "prop_dynamic_override" );
	if ( !pProp )
		return;
	pProp->KeyValue( "model", pModel );
	if ( args.ArgC() > 2 )
		pProp->KeyValue( "skin", args[2] );
	if ( !bPhysics )
		pProp->KeyValue( "solid", "2" );
	pProp->SetAbsOrigin( tr.endpos );
	pProp->SetAbsAngles( QAngle( 0, pPlayer->EyeAngles().y + 180, 0 ) );
	if ( DispatchSpawn( pProp ) < 0 )
	{
		UTIL_Remove( pProp );
		return;
	}
	pProp->Activate();
	Vector origin = tr.endpos - tr.plane.normal * pProp->CollisionProp()->OBBMins().z;
	pProp->Teleport( &origin, NULL, NULL );
	Msg( "gmp_spawn: %s at %.0f %.0f %.0f\n", pModel, origin.x, origin.y, origin.z );
}

// gmp_ent <item_...>: the spawn menu's Entities tab. Real HL2 items (pickups, wall chargers), not
// the prop models the spawnlists have; chargers go flat on the wall you look at.
CON_COMMAND( gmp_ent, "GModPort: gmp_ent <item_...> - spawn an HL2 item where you look" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 2 || Q_strnicmp( args[1], "item_", 5 ) )
		return;
	Vector start = pPlayer->EyePosition();
	Vector forward;
	pPlayer->EyeVectors( &forward );
	trace_t tr;
	UTIL_TraceLine( start, start + forward * 1024, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

	CBaseEntity *pEnt = CreateEntityByName( args[1] );
	if ( !pEnt )
		return;
	const bool bWall = Q_stristr( args[1], "charger" ) != NULL;
	QAngle angles( 0, pPlayer->EyeAngles().y + 180, 0 );
	if ( bWall && fabsf( tr.plane.normal.z ) < 0.7f )
		VectorAngles( tr.plane.normal, angles );
	pEnt->SetAbsOrigin( bWall ? tr.endpos : tr.endpos + tr.plane.normal * 8 );
	pEnt->SetAbsAngles( angles );
	if ( DispatchSpawn( pEnt ) < 0 )
	{
		UTIL_Remove( pEnt );
		return;
	}
	pEnt->Activate();
	if ( !bWall )
	{
		Vector origin = tr.endpos - tr.plane.normal * pEnt->CollisionProp()->OBBMins().z;
		pEnt->Teleport( &origin, NULL, NULL );
	}
	Msg( "gmp_ent: %s\n", args[1] );
}

// gmp_give <weapon_...>: the spawn menu's Weapons tab. Gives the weapon (once) plus ammo and
// switches to it.
CON_COMMAND( gmp_give, "GModPort: gmp_give <weapon_...> - give a weapon from the spawn menu" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || !pPlayer->IsAlive() || args.ArgC() < 2 || Q_strnicmp( args[1], "weapon_", 7 ) )
		return;
	pPlayer->GiveNamedItem( args[1] );
	CBaseCombatWeapon *pWeapon = pPlayer->Weapon_OwnsThisType( args[1] );
	if ( !pWeapon )
		return;
	if ( pWeapon->GetPrimaryAmmoType() >= 0 )
		pPlayer->GiveAmmo( MAX( pWeapon->GetMaxClip1(), 1 ) * 3, pWeapon->GetPrimaryAmmoType() );
	if ( pWeapon->GetSecondaryAmmoType() >= 0 )
		pPlayer->GiveAmmo( 1, pWeapon->GetSecondaryAmmoType() );
	pPlayer->Weapon_Switch( pWeapon );
}

// gmp_ammo <ammo type>|all: the Weapons tab's ammo boxes. Fills that ammo type to its max carry.
CON_COMMAND( gmp_ammo, "GModPort: gmp_ammo <Pistol|SMG1|...|all> - fill an ammo type from the spawn menu" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || !pPlayer->IsAlive() || args.ArgC() < 2 )
		return;
	const bool bAll = !Q_stricmp( args[1], "all" );
	const int index = bAll ? -1 : GetAmmoDef()->Index( args[1] );
	if ( !bAll && index < 0 )
		return;
	for ( int i = 1; i < GetAmmoDef()->m_nAmmoIndex; i++ )	// 0 is the invalid type
		if ( bAll || i == index )
			pPlayer->GiveAmmo( GetAmmoDef()->MaxCarry( i ), i );
}
#endif // !CLIENT_DLL

#ifdef CLIENT_DLL
#include "vgui_controls/EditablePanel.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "matsys_controls/mdlpanel.h"
#include "datacache/imdlcache.h"
#include "ienginevgui.h"
#include "iinput.h"
#include "beamdraw.h"

#ifdef CreateFont
#undef CreateFont
#endif

#define GMP_PHYSBEAM_MATERIAL "sprites/physbeama"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectGMPPhysGun )
CLIENTEFFECT_MATERIAL( GMP_PHYSBEAM_MATERIAL )
CLIENTEFFECT_REGISTER_END()

// GMod's physgun beam: a quadratic curve from the muzzle that leaves along the aim direction and
// bends onto the grabbed point. A translucent renderable of its own, so it is drawn with the world's
// view and depth (the viewmodel pass has its own FOV and depth range).
class CGMPPhysBeam : public CDefaultClientRenderable
{
public:
	CGMPPhysBeam( C_WeaponGMPPhysGun *pWeapon ) : m_pWeapon( pWeapon ), m_bSnap( true )
	{
		m_vecOrigin.Init();
		m_vecDrawEnd.Init();
	}
	~CGMPPhysBeam()
	{
		Hide();
	}

	void Update( void )
	{
		if ( !m_pWeapon->m_bBeam || m_pWeapon->IsDormant() )
		{
			Hide();
			return;
		}
		m_vecOrigin = m_pWeapon->m_vecBeamEnd;
		if ( m_hRenderHandle == INVALID_CLIENT_RENDER_HANDLE )
		{
			m_bSnap = true;
			ClientLeafSystem()->AddRenderable( this, RENDER_GROUP_TRANSLUCENT_ENTITY );
		}
		else
		{
			ClientLeafSystem()->RenderableChanged( m_hRenderHandle );
		}
	}

	void Hide( void )
	{
		if ( m_hRenderHandle != INVALID_CLIENT_RENDER_HANDLE )
			ClientLeafSystem()->RemoveRenderable( m_hRenderHandle );
	}

	virtual const Vector &GetRenderOrigin( void ) { return m_vecOrigin; }
	virtual const QAngle &GetRenderAngles( void ) { return vec3_angle; }
	virtual const matrix3x4_t &RenderableToWorldTransform( void )
	{
		static matrix3x4_t mat;
		SetIdentityMatrix( mat );
		PositionMatrix( m_vecOrigin, mat );
		return mat;
	}
	virtual bool ShouldDraw( void ) { return true; }
	virtual bool IsTransparent( void ) { return true; }
	virtual bool ShouldReceiveProjectedTextures( int flags ) { return false; }

	// From the grabbed point back to the owner's eyes, so the leaf system keeps it while either
	// end is visible.
	virtual void GetRenderBounds( Vector &mins, Vector &maxs )
	{
		mins.Init( -64, -64, -64 );
		maxs.Init( 64, 64, 64 );
		C_BaseEntity *pOwner = m_pWeapon->GetOwner();
		if ( pOwner )
		{
			const Vector toEye = pOwner->EyePosition() - m_vecOrigin;
			for ( int i = 0; i < 3; i++ )
			{
				mins[i] = MIN( mins[i], toEye[i] - 64 );
				maxs[i] = MAX( maxs[i], toEye[i] + 64 );
			}
		}
	}

	virtual int DrawModel( int flags )
	{
		C_BasePlayer *pOwner = ToBasePlayer( m_pWeapon->GetOwner() );
		if ( !pOwner || !m_pWeapon->m_bBeam || pOwner->GetActiveWeapon() != m_pWeapon )
			return 0;

		Vector start;
		QAngle angles;
		if ( pOwner->IsLocalPlayer() && !C_BasePlayer::ShouldDrawLocalPlayer() )
		{
			C_BaseViewModel *pViewModel = pOwner->GetViewModel();
			if ( !pViewModel )
				return 0;
			const int attachment = pViewModel->LookupAttachment( "muzzle" );
			if ( !pViewModel->GetAttachment( attachment > 0 ? attachment : 1, start, angles ) )
				return 0;
		}
		else
		{
			const int attachment = m_pWeapon->LookupAttachment( "core" );
			if ( !m_pWeapon->GetAttachment( attachment > 0 ? attachment : 1, start, angles ) )
				return 0;
		}

		// The server sends the grabbed point at tick rate; ease toward it so the end does not step.
		if ( m_bSnap )
			m_vecDrawEnd = m_pWeapon->m_vecBeamEnd;
		else
			m_vecDrawEnd += ( m_pWeapon->m_vecBeamEnd - m_vecDrawEnd ) * MIN( 1.0f, gpGlobals->frametime * 20.0f );
		m_bSnap = false;

		Vector forward;
		AngleVectors( pOwner->EyeAngles(), &forward );
		const float distance = ( m_vecDrawEnd - start ).Length();
		const Vector control = start + forward * ( distance * 0.5f );

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( materials->FindMaterial( GMP_PHYSBEAM_MATERIAL, TEXTURE_GROUP_CLIENT_EFFECTS ) );
		const Vector color( 0.3f, 0.85f, 1.0f );	// GMod's default weapon colour
		DrawBeamQuadratic( start, control, m_vecDrawEnd, 6.0f, color, gpGlobals->curtime * 2.0f );
		return 1;
	}

private:
	C_WeaponGMPPhysGun	*m_pWeapon;
	Vector				m_vecOrigin;
	Vector				m_vecDrawEnd;
	bool				m_bSnap;
};

C_WeaponGMPPhysGun::~C_WeaponGMPPhysGun( void )
{
	delete m_pBeam;
}

void C_WeaponGMPPhysGun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( !m_pBeam && m_bBeam )
		m_pBeam = new CGMPPhysBeam( this );
	if ( m_pBeam )
		m_pBeam->Update();
}

// gmp_weapnext / gmp_weapprev: the touch weapon buttons. Switches straight to the next or previous
// weapon in slot order, without the HUD selection that invnext/invprev open and +attack confirms.
static void GMP_CycleWeapon( int dir )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;
	CUtlVector<C_BaseCombatWeapon *> weapons;
	for ( int i = 0; i < pPlayer->WeaponCount(); i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );
		if ( !pWeapon || !pWeapon->CanBeSelected() )
			continue;
		const int key = pWeapon->GetSlot() * 100 + pWeapon->GetPosition();
		int at = 0;
		while ( at < weapons.Count() && weapons[at]->GetSlot() * 100 + weapons[at]->GetPosition() <= key )
			at++;
		weapons.InsertBefore( at, pWeapon );
	}
	if ( weapons.Count() == 0 )
		return;
	const int current = weapons.Find( pPlayer->GetActiveWeapon() );
	const int next = current < 0 ? 0 : ( current + dir + weapons.Count() ) % weapons.Count();
	if ( weapons[next] != pPlayer->GetActiveWeapon() )
	{
		::input->MakeWeaponSelection( weapons[next] );
		pPlayer->EmitSound( "Player.WeaponSelectionMoveSlot" );
	}
}
CON_COMMAND( gmp_weapnext, "GModPort: switch to the next weapon" ) { GMP_CycleWeapon( 1 ); }
CON_COMMAND( gmp_weapprev, "GModPort: switch to the previous weapon" ) { GMP_CycleWeapon( -1 ); }

// GMod-style spawn menu: GMod's own spawnlists (settings/spawnlist*/ *.txt) on the left, a grid
// of live model icons on the right, a Weapons tab. Drag scrolls, a tap spawns.

struct GMPSpawnItem
{
	CUtlString model;	// empty: a header row
	CUtlString text;	// header text or the label under the icon
	CUtlString command;	// client command run on tap
	int skin;
};

struct GMPSpawnList
{
	CUtlString name;
	int id, parent, depth;
	bool needsApp;
	CUtlVector<GMPSpawnItem> items;
};

#include "gmp_npc_menu.inl"

static CUtlVector<GMPSpawnList *> g_GMPSpawnLists;	// tree order: every list followed by its children
static CUtlVector<GMPSpawnItem> g_GMPWeapons;
static CUtlVector<GMPSpawnItem> g_GMPEntities;

// Entities tab: gmp_ent <class>; a NULL class starts a header.
static const struct { const char *cls, *name, *model; } s_GMPEntityTable[] =
{
	{ NULL, "Half-Life 2: Items", NULL },
	{ "item_healthkit", "Health Kit", "models/items/healthkit.mdl" },
	{ "item_healthvial", "Health Vial", "models/healthvial.mdl" },
	{ "item_battery", "Suit Battery", "models/items/battery.mdl" },
	{ "item_suit", "HEV Suit", "models/items/hevsuit.mdl" },
	{ "item_healthcharger", "Health Charger", "models/props_combine/health_charger001.mdl" },
	{ "item_suitcharger", "Suit Charger", "models/props_combine/suit_charger001.mdl" },
	{ NULL, "Half-Life 2: Ammo", NULL },
	{ "item_ammo_pistol", "Pistol Ammo", "models/items/boxsrounds.mdl" },
	{ "item_ammo_pistol_large", "Pistol Ammo (Large)", "models/items/boxsrounds.mdl" },
	{ "item_ammo_357", ".357 Ammo", "models/items/357ammo.mdl" },
	{ "item_ammo_357_large", ".357 Ammo (Large)", "models/items/357ammobox.mdl" },
	{ "item_ammo_smg1", "SMG Ammo", "models/items/boxmrounds.mdl" },
	{ "item_ammo_smg1_large", "SMG Ammo (Large)", "models/items/boxmrounds.mdl" },
	{ "item_ammo_smg1_grenade", "SMG Grenade", "models/items/ar2_grenade.mdl" },
	{ "item_ammo_ar2", "Pulse Ammo", "models/items/combine_rifle_cartridge01.mdl" },
	{ "item_ammo_ar2_large", "Pulse Ammo (Large)", "models/items/combine_rifle_cartridge01.mdl" },
	{ "item_ammo_ar2_altfire", "Combine Ball", "models/items/combine_rifle_ammo01.mdl" },
	{ "item_box_buckshot", "Shotgun Ammo", "models/items/boxbuckshot.mdl" },
	{ "item_ammo_crossbow", "Crossbow Bolts", "models/items/crossbowrounds.mdl" },
	{ "item_rpg_round", "RPG Rocket", "models/weapons/w_missile_closed.mdl" },
};

static const struct { const char *cls, *name, *model; } s_GMPWeaponTable[] =
{
	{ "weapon_physgun", "Physics Gun", "models/weapons/w_physics.mdl" },
	{ "weapon_physcannon", "Gravity Gun", "models/weapons/w_physics.mdl" },
	{ "weapon_crowbar", "Crowbar", "models/weapons/w_crowbar.mdl" },
	{ "weapon_stunstick", "Stunstick", "models/weapons/w_stunbaton.mdl" },
	{ "weapon_pistol", "9mm Pistol", "models/weapons/w_pistol.mdl" },
	{ "weapon_357", ".357 Magnum", "models/weapons/w_357.mdl" },
	{ "weapon_smg1", "SMG", "models/weapons/w_smg1.mdl" },
	{ "weapon_ar2", "Pulse-Rifle", "models/weapons/w_irifle.mdl" },
	{ "weapon_shotgun", "Shotgun", "models/weapons/w_shotgun.mdl" },
	{ "weapon_crossbow", "Crossbow", "models/weapons/w_crossbow.mdl" },
	{ "weapon_frag", "Grenade", "models/weapons/w_grenade.mdl" },
	{ "weapon_rpg", "RPG Launcher", "models/weapons/w_rocket_launcher.mdl" },
	{ "weapon_slam", "S.L.A.M", "models/weapons/w_slam.mdl" },
	{ "weapon_bugbait", "Bugbait", "models/weapons/w_bugbait.mdl" },
	{ "weapon_annabelle", "Annabelle", "models/weapons/w_annabelle.mdl" },
};

// Ammo boxes: gmp_ammo <type> (hl2mp_gamerules.cpp ammo names).
static const struct { const char *type, *name, *model; } s_GMPAmmoTable[] =
{
	{ "all", "All Ammo", "models/items/ammocrate_smg1.mdl" },
	{ "Pistol", "Pistol Ammo", "models/items/boxsrounds.mdl" },
	{ "357", ".357 Ammo", "models/items/357ammo.mdl" },
	{ "SMG1", "SMG Ammo", "models/items/boxmrounds.mdl" },
	{ "SMG1_Grenade", "SMG Grenades", "models/items/ar2_grenade.mdl" },
	{ "AR2", "Pulse Ammo", "models/items/combine_rifle_cartridge01.mdl" },
	{ "AR2AltFire", "Combine Balls", "models/items/combine_rifle_ammo01.mdl" },
	{ "Buckshot", "Shotgun Ammo", "models/items/boxbuckshot.mdl" },
	{ "XBowBolt", "Crossbow Bolts", "models/items/crossbowrounds.mdl" },
	{ "RPG_Round", "RPG Rockets", "models/weapons/w_missile_closed.mdl" },
	{ "Grenade", "Grenades", "models/weapons/w_grenade.mdl" },
	{ "slam", "S.L.A.M", "models/weapons/w_slam.mdl" },
};

static int GMP_CompareStrings( const CUtlString *a, const CUtlString *b )
{
	return Q_stricmp( a->Get(), b->Get() );
}

static void GMP_AddSpawnListTree( CUtlVector<GMPSpawnList *> &all, int parent, int depth )
{
	for ( int i = 0; i < all.Count(); i++ )
	{
		GMPSpawnList *list = all[i];
		if ( list->parent != parent || list->needsApp )
			continue;
		list->depth = depth;
		g_GMPSpawnLists.AddToTail( list );
		if ( list->id != parent )
			GMP_AddSpawnListTree( all, list->id, depth + 1 );
	}
}

static void GMP_LoadSpawnMenu( void )
{
	if ( g_GMPSpawnLists.Count() )
		return;

	// GMod copies spawnlist_default to settings/spawnlist on first run and edits that copy.
	const char *dir = filesystem->IsDirectory( "settings/spawnlist", "GAME" ) ? "settings/spawnlist" : "settings/spawnlist_default";
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "%s/*.txt", dir );
	CUtlVector<CUtlString> files;
	FileFindHandle_t hFind;
	for ( const char *name = filesystem->FindFirstEx( path, "GAME", &hFind ); name; name = filesystem->FindNext( hFind ) )
		files.AddToTail( name );
	filesystem->FindClose( hFind );
	files.Sort( GMP_CompareStrings );

	CUtlVector<GMPSpawnList *> all;
	for ( int i = 0; i < files.Count(); i++ )
	{
		if ( i && !Q_stricmp( files[i].Get(), files[i - 1].Get() ) )
			continue;	// same file in two search paths
		Q_snprintf( path, sizeof( path ), "%s/%s", dir, files[i].Get() );
		KeyValues *kv = new KeyValues( "spawnlist" );
		if ( kv->LoadFromFile( filesystem, path, "GAME" ) )
		{
			GMPSpawnList *list = new GMPSpawnList;
			list->name = kv->GetString( "name", files[i].Get() );
			list->id = kv->GetInt( "id" );
			list->parent = kv->GetInt( "parentid" );
			list->depth = 0;
			// Lists for other games' content (Portal, TF2, CS...): not mounted on the phone.
			list->needsApp = kv->GetString( "needsapp" )[0] != 0;
			KeyValues *contents = kv->FindKey( "contents" );
			for ( KeyValues *sub = contents ? contents->GetFirstTrueSubKey() : NULL; sub; sub = sub->GetNextTrueSubKey() )
			{
				GMPSpawnItem item;
				item.skin = sub->GetInt( "skin" );
				const char *type = sub->GetString( "type" );
				if ( !Q_stricmp( type, "model" ) && sub->GetString( "model" )[0] )
				{
					item.model = sub->GetString( "model" );
					char cmd[MAX_PATH + 32];
					Q_snprintf( cmd, sizeof( cmd ), "gmp_spawn \"%s\" %d", item.model.Get(), item.skin );
					item.command = cmd;
				}
				else if ( !Q_stricmp( type, "header" ) )
					item.text = sub->GetString( "text" );
				else
					continue;
				list->items.AddToTail( item );
			}
			all.AddToTail( list );
		}
		kv->deleteThis();
	}
	GMP_AddSpawnListTree( all, 0, 0 );
	DevMsg( "GModPort: %d spawnlists from %s (%d shown)\n", all.Count(), dir, g_GMPSpawnLists.Count() );

	GMPSpawnItem header;
	header.text = "Half-Life 2";
	header.skin = 0;
	g_GMPWeapons.AddToTail( header );
	for ( int i = 0; i < ARRAYSIZE( s_GMPWeaponTable ); i++ )
	{
		GMPSpawnItem item;
		item.model = s_GMPWeaponTable[i].model;
		item.text = s_GMPWeaponTable[i].name;
		char cmd[64];
		Q_snprintf( cmd, sizeof( cmd ), "gmp_give %s", s_GMPWeaponTable[i].cls );
		item.command = cmd;
		item.skin = 0;
		g_GMPWeapons.AddToTail( item );
	}
	for ( int i = 0; i < ARRAYSIZE( s_GMPEntityTable ); i++ )
	{
		GMPSpawnItem item;
		item.text = s_GMPEntityTable[i].name;
		item.skin = 0;
		if ( s_GMPEntityTable[i].cls )
		{
			item.model = s_GMPEntityTable[i].model;
			char cmd[64];
			Q_snprintf( cmd, sizeof( cmd ), "gmp_ent %s", s_GMPEntityTable[i].cls );
			item.command = cmd;
		}
		g_GMPEntities.AddToTail( item );
	}
	header.text = "Ammo";
	g_GMPWeapons.AddToTail( header );
	for ( int i = 0; i < ARRAYSIZE( s_GMPAmmoTable ); i++ )
	{
		GMPSpawnItem item;
		item.model = s_GMPAmmoTable[i].model;
		item.text = s_GMPAmmoTable[i].name;
		char cmd[64];
		Q_snprintf( cmd, sizeof( cmd ), "gmp_ammo %s", s_GMPAmmoTable[i].type );
		item.command = cmd;
		item.skin = 0;
		g_GMPWeapons.AddToTail( item );
	}
}

// Touch-friendly scrolling: press, drag past a threshold to scroll, release without a drag to tap.
class CGMPDragPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CGMPDragPanel, vgui::Panel );
public:
	CGMPDragPanel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name ),
		m_nScroll( 0 ), m_nContentTall( 0 ), m_bPressed( false ), m_bDragged( false ) {}

	void SetScroll( int scroll )
	{
		scroll = clamp( scroll, 0, MAX( 0, m_nContentTall - GetTall() ) );
		if ( scroll != m_nScroll )
		{
			m_nScroll = scroll;
			OnScrolled();
		}
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		if ( code != MOUSE_LEFT )
			return;
		vgui::input()->GetCursorPos( m_nPressX, m_nPressY );
		ScreenToLocal( m_nPressX, m_nPressY );
		m_nPressScroll = m_nScroll;
		m_bPressed = true;
		m_bDragged = false;
		vgui::input()->SetMouseCapture( GetVPanel() );
	}

	virtual void OnCursorMoved( int x, int y )
	{
		if ( !m_bPressed )
			return;
		if ( abs( y - m_nPressY ) > GetTall() / 40 )
			m_bDragged = true;
		if ( m_bDragged )
			SetScroll( m_nPressScroll - ( y - m_nPressY ) );
	}

	virtual void OnMouseReleased( vgui::MouseCode code )
	{
		if ( code != MOUSE_LEFT || !m_bPressed )
			return;
		m_bPressed = false;
		vgui::input()->SetMouseCapture( NULL );
		if ( !m_bDragged )
			OnTap( m_nPressX, m_nPressY + m_nScroll );
	}

	virtual void OnMouseWheeled( int delta ) { SetScroll( m_nScroll - delta * GetTall() / 6 ); }

protected:
	virtual void OnScrolled( void ) { Repaint(); }
	virtual void OnTap( int x, int y ) = 0;	// content coordinates

	int m_nScroll, m_nContentTall;

private:
	bool m_bPressed, m_bDragged;
	int m_nPressX, m_nPressY, m_nPressScroll;
};

// The left column: spawnlists as an always-expanded tree.
class CGMPSpawnListPanel : public CGMPDragPanel
{
	DECLARE_CLASS_SIMPLE( CGMPSpawnListPanel, CGMPDragPanel );
public:
	CGMPSpawnListPanel( vgui::Panel *parent ) : BaseClass( parent, "SpawnLists" ), m_hFont( 0 ), m_hTitleFont( 0 ), m_nSelected( 0 ) {}

	void SetFonts( vgui::HFont font, vgui::HFont title ) { m_hFont = font; m_hTitleFont = title; InvalidateLayout(); }
	int Selected( void ) const { return m_nSelected; }

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();
		m_nRow = MAX( 1, vgui::surface()->GetFontTall( m_hFont ) * 2 );
		m_nContentTall = m_nRow * ( g_GMPSpawnLists.Count() + 1 );
		SetScroll( m_nScroll );
	}

	virtual void Paint( void )
	{
		int w, h;
		GetSize( w, h );
		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );
		DrawText( m_hTitleFont, m_nRow / 3, -m_nScroll, "Your Spawnlists", Color( 40, 40, 40, 255 ) );
		for ( int i = 0; i < g_GMPSpawnLists.Count(); i++ )
		{
			const int y = ( i + 1 ) * m_nRow - m_nScroll;
			if ( y + m_nRow < 0 || y > h )
				continue;
			const bool selected = i == m_nSelected;
			if ( selected )
			{
				vgui::surface()->DrawSetColor( 60, 140, 230, 255 );
				vgui::surface()->DrawFilledRect( 0, y, w, y + m_nRow );
			}
			const GMPSpawnList *list = g_GMPSpawnLists[i];
			DrawText( m_hFont, m_nRow / 3 + list->depth * m_nRow * 2 / 3, y, list->name.Get(),
				selected ? Color( 255, 255, 255, 255 ) : Color( 40, 40, 40, 255 ) );
		}
		vgui::surface()->DrawSetColor( 160, 160, 160, 255 );
		vgui::surface()->DrawOutlinedRect( 0, 0, w, h );
	}

protected:
	virtual void OnTap( int x, int y )
	{
		const int i = y / m_nRow - 1;
		if ( i < 0 || i >= g_GMPSpawnLists.Count() )
			return;
		m_nSelected = i;
		Repaint();
		PostActionSignal( new KeyValues( "SpawnListSelected", "index", i ) );
	}

private:
	void DrawText( vgui::HFont font, int x, int y, const char *text, Color color )
	{
		wchar_t wide[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wide, sizeof( wide ) );
		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->DrawSetTextColor( color );
		vgui::surface()->DrawSetTextPos( x, y + ( m_nRow - vgui::surface()->GetFontTall( font ) ) / 2 );
		vgui::surface()->DrawPrintText( wide, wcslen( wide ) );
	}

	vgui::HFont m_hFont, m_hTitleFont;
	int m_nSelected, m_nRow;
};

// The icon grid. Headers take a row of their own; the icons are a pool of CMDLPanels that
// follow the visible cells, so only what is on screen is loaded and rendered.
class CGMPSpawnGrid : public CGMPDragPanel
{
	DECLARE_CLASS_SIMPLE( CGMPSpawnGrid, CGMPDragPanel );
public:
	CGMPSpawnGrid( vgui::Panel *parent ) : BaseClass( parent, "SpawnGrid" ), m_pItems( NULL ), m_hFont( 0 ), m_hHeaderFont( 0 ) {}

	void SetFonts( vgui::HFont font, vgui::HFont header ) { m_hFont = font; m_hHeaderFont = header; InvalidateLayout(); }

	void SetItems( const CUtlVector<GMPSpawnItem> *pItems )
	{
		m_pItems = pItems;
		m_nScroll = 0;
		for ( int i = 0; i < m_Cells.Count(); i++ )
			m_Cells[i].item = -1;
		InvalidateLayout();
		Repaint();
	}

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();
		int w, h;
		GetSize( w, h );
		m_nCell = MAX( 32, h / 4 );
		m_nGap = m_nCell / 14;
		const int cols = MAX( 1, ( w - m_nGap ) / ( m_nCell + m_nGap ) );
		m_nCell = ( w - m_nGap ) / cols - m_nGap;	// fill the width
		m_Places.RemoveAll();
		int col = 0, y = m_nGap;
		const int headerTall = vgui::surface()->GetFontTall( m_hHeaderFont ) * 3 / 2;
		for ( int i = 0; m_pItems && i < m_pItems->Count(); i++ )
		{
			Place place = { i, 0, 0, 0, 0, false };
			if ( ( *m_pItems )[i].model.IsEmpty() )
			{
				if ( col )
					y += m_nCell + m_nGap;
				col = 0;
				place.x = m_nGap; place.y = y; place.w = w - 2 * m_nGap; place.h = headerTall;
				y += headerTall + m_nGap;
			}
			else
			{
				place.x = m_nGap + col * ( m_nCell + m_nGap ); place.y = y; place.w = place.h = m_nCell;
				if ( ++col == cols )
				{
					col = 0;
					y += m_nCell + m_nGap;
				}
			}
			m_Places.AddToTail( place );
		}
		m_nContentTall = y + ( col ? m_nCell + m_nGap : 0 );
		SetScroll( m_nScroll );
		UpdateCells();
	}

	virtual void Paint( void )
	{
		int w, h;
		GetSize( w, h );
		vgui::surface()->DrawSetColor( 227, 227, 227, 255 );
		vgui::surface()->DrawFilledRect( 0, 0, w, h );
		for ( int i = 0; i < m_Places.Count(); i++ )
		{
			const Place &place = m_Places[i];
			const int y = place.y - m_nScroll;
			if ( y + place.h < 0 || y > h )
				continue;
			const GMPSpawnItem &item = ( *m_pItems )[place.item];
			if ( item.model.IsEmpty() )
			{
				DrawText( m_hHeaderFont, place.x, y, place.w, place.h, item.text.Get(), Color( 60, 60, 60, 255 ), false );
				vgui::surface()->DrawSetColor( 160, 160, 160, 255 );
				vgui::surface()->DrawFilledRect( place.x, y + place.h - 2, place.x + place.w, y + place.h );
				continue;
			}
			vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
			vgui::surface()->DrawFilledRect( place.x, y, place.x + place.w, y + place.h );
			vgui::surface()->DrawSetColor( 170, 170, 170, 255 );
			vgui::surface()->DrawOutlinedRect( place.x, y, place.x + place.w, y + place.h );
			if ( place.missing )
			{
				// Not in the mounted content: show the file name instead of an icon.
				DrawText( m_hFont, place.x, y, place.w, place.h, V_UnqualifiedFileName( item.model.Get() ), Color( 150, 60, 60, 255 ), true );
			}
			else if ( !item.text.IsEmpty() )
			{
				// The icon panel stops above this strip (UpdateCells), so the label is not covered.
				const int labelTall = vgui::surface()->GetFontTall( m_hFont ) * 3 / 2;
				DrawText( m_hFont, place.x, y + place.h - labelTall, place.w, labelTall, item.text.Get(), Color( 40, 40, 40, 255 ), true );
			}
		}
	}

protected:
	virtual void OnScrolled( void )
	{
		UpdateCells();
		Repaint();
	}

	virtual void OnTap( int x, int y )
	{
		for ( int i = 0; i < m_Places.Count(); i++ )
		{
			const Place &place = m_Places[i];
			const GMPSpawnItem &item = ( *m_pItems )[place.item];
			if ( item.command.IsEmpty() || x < place.x || x >= place.x + place.w || y < place.y || y >= place.y + place.h )
				continue;
			vgui::surface()->PlaySound( "ui/buttonclickrelease.wav" );
			engine->ClientCmd( item.command.Get() );
			return;
		}
	}

private:
	struct Place { int item, x, y, w, h; bool missing; };
	struct Cell { CMDLPanel *panel; int item; };

	void UpdateCells( void )
	{
		if ( !m_pItems )
			return;
		const int h = GetTall();
		// Free the cells whose item scrolled out.
		for ( int c = 0; c < m_Cells.Count(); c++ )
		{
			const int p = PlaceOf( m_Cells[c].item );
			if ( p < 0 || !IsShown( m_Places[p], h ) )
				m_Cells[c].item = -1;
		}
		for ( int p = 0; p < m_Places.Count(); p++ )
		{
			Place &place = m_Places[p];
			if ( ( *m_pItems )[place.item].model.IsEmpty() || !IsShown( place, h ) )
				continue;
			int c = CellOf( place.item );
			if ( c < 0 )
			{
				c = CellOf( -1 );
				if ( c < 0 )
				{
					Cell cell = { new CMDLPanel( this, "SpawnIcon" ), -1 };
					cell.panel->SetMouseInputEnabled( false );
					cell.panel->SetKeyBoardInputEnabled( false );
					cell.panel->SetBackgroundColor( 255, 255, 255 );
					c = m_Cells.AddToTail( cell );
				}
				m_Cells[c].item = place.item;
				place.missing = !LoadIcon( m_Cells[c].panel, ( *m_pItems )[place.item] );
			}
			const int inset = MAX( 2, m_nGap / 2 );
			const int labelTall = ( *m_pItems )[place.item].text.IsEmpty() ? 0 : vgui::surface()->GetFontTall( m_hFont ) * 3 / 2;
			m_Cells[c].panel->SetBounds( place.x + inset, place.y - m_nScroll + inset, place.w - 2 * inset, place.h - 2 * inset - labelTall );
			m_Cells[c].panel->SetVisible( !place.missing );
		}
		for ( int c = 0; c < m_Cells.Count(); c++ )
			if ( m_Cells[c].item < 0 )
				m_Cells[c].panel->SetVisible( false );
	}

	static bool LoadIcon( CMDLPanel *panel, const GMPSpawnItem &item )
	{
		MDLHandle_t hMDL = mdlcache->FindMDL( item.model.Get() );
		mdlcache->GetStudioHdr( hMDL );	// loads it; a missing file becomes the error model
		const bool ok = !mdlcache->IsErrorModel( hMDL );
		panel->SetMDL( ok ? hMDL : MDLHANDLE_INVALID );
		mdlcache->Release( hMDL );
		if ( !ok )
			return false;
		panel->SetSkin( item.skin );
		// GMod's spawn icon view: from the front left, slightly above.
		panel->SetCameraPositionAndAngles( vec3_origin, QAngle( 25, 220, 0 ) );
		panel->LookAtMDL();
		return true;
	}

	bool IsShown( const Place &place, int h ) const { return place.y - m_nScroll + place.h > 0 && place.y - m_nScroll < h; }

	int PlaceOf( int item ) const
	{
		for ( int p = 0; item >= 0 && p < m_Places.Count(); p++ )
			if ( m_Places[p].item == item )
				return p;
		return -1;
	}

	int CellOf( int item ) const
	{
		for ( int c = 0; c < m_Cells.Count(); c++ )
			if ( m_Cells[c].item == item )
				return c;
		return -1;
	}

	void DrawText( vgui::HFont font, int x, int y, int w, int h, const char *text, Color color, bool center )
	{
		wchar_t wide[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( text, wide, sizeof( wide ) );
		int tw, th;
		vgui::surface()->GetTextSize( font, wide, tw, th );
		vgui::surface()->DrawSetTextFont( font );
		vgui::surface()->DrawSetTextColor( color );
		vgui::surface()->DrawSetTextPos( center ? x + MAX( 0, ( w - tw ) / 2 ) : x + th / 4, y + ( h - th ) / 2 );
		vgui::surface()->DrawPrintText( wide, wcslen( wide ) );
	}

	const CUtlVector<GMPSpawnItem> *m_pItems;
	CUtlVector<Place> m_Places;
	CUtlVector<Cell> m_Cells;
	vgui::HFont m_hFont, m_hHeaderFont;
	int m_nCell, m_nGap;
};

class CGMPSpawnMenu : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CGMPSpawnMenu, vgui::EditablePanel );
public:
	CGMPSpawnMenu( vgui::VPANEL parent ) : BaseClass( NULL, "GMPSpawnMenu" ),
		m_hTabFont( 0 ), m_hListFont( 0 ), m_hHeaderFont( 0 ), m_hLabelFont( 0 ), m_nTab( 0 )
	{
		SetParent( parent );
		// A popup so vgui shows the cursor and gets taps; no keyboard, so the +menu bind still closes it.
		MakePopup( false );
		SetKeyBoardInputEnabled( false );
		SetMouseInputEnabled( true );
		SetPaintBackgroundEnabled( false );
		m_pLists = new CGMPSpawnListPanel( this );
		m_pLists->AddActionSignalTarget( this );
		m_pGrid = new CGMPSpawnGrid( this );
		SetVisible( false );
	}

	void Toggle( void )
	{
		if ( IsVisible() )
		{
			SetVisible( false );
			return;
		}
		GMP_LoadSpawnMenu();
		GMP_LoadNPCMenu();
		SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
		SelectTab( m_nTab );
		SetVisible( true );
		MoveToFront();
	}

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		const char *family = vgui::surface()->GetFontFamilyName( pScheme->GetFont( "Default", false ) );
		if ( !family || !*family )
			family = "Verdana";
		const int tall = ScreenHeight();
		m_hTabFont = MakeFont( family, tall / 28, 700 );
		m_hListFont = MakeFont( family, tall / 34, 500 );
		m_hHeaderFont = MakeFont( family, tall / 26, 800 );
		m_hLabelFont = MakeFont( family, tall / 50, 500 );
		m_pLists->SetFonts( m_hListFont, m_hTabFont );
		m_pGrid->SetFonts( m_hLabelFont, m_hHeaderFont );
	}

	virtual void PerformLayout( void )
	{
		BaseClass::PerformLayout();
		int w, h;
		GetSize( w, h );
		// Landscape iPhones keep the notch and the home bar out of the side and bottom margins.
		m_nX0 = w * 6 / 100; m_nX1 = w - w * 6 / 100;
		m_nY0 = h / 30; m_nY1 = h - h / 20;
		m_nTabTall = h / 11;
		const int pad = h / 90;
		const int top = m_nY0 + m_nTabTall + pad, bottom = m_nY1 - pad;
		const int listWide = m_nTab == 0 ? ( m_nX1 - m_nX0 ) / 4 : 0;
		m_pLists->SetVisible( listWide > 0 );
		m_pLists->SetBounds( m_nX0 + pad, top, listWide, bottom - top );
		const int gridX = m_nX0 + pad + ( listWide ? listWide + pad : 0 );
		m_pGrid->SetBounds( gridX, top, m_nX1 - pad - gridX, bottom - top );
	}

	virtual void Paint( void )
	{
		vgui::surface()->DrawSetColor( 50, 50, 50, 240 );
		vgui::surface()->DrawFilledRect( m_nX0, m_nY0 + m_nTabTall, m_nX1, m_nY1 );
		for ( int i = 0; i <= ARRAYSIZE( s_Tabs ); i++ )
		{
			int x0, x1;
			TabRect( i, x0, x1 );
			const bool close = i == ARRAYSIZE( s_Tabs );
			const bool active = i == m_nTab;
			if ( close )
				vgui::surface()->DrawSetColor( 200, 70, 60, 240 );
			else if ( active )
				vgui::surface()->DrawSetColor( 50, 50, 50, 240 );
			else
				vgui::surface()->DrawSetColor( 95, 95, 95, 230 );
			vgui::surface()->DrawFilledRect( x0, m_nY0 + ( active || close ? 0 : m_nTabTall / 8 ), x1, m_nY0 + m_nTabTall );
			wchar_t wide[32];
			g_pVGuiLocalize->ConvertANSIToUnicode( close ? "X" : s_Tabs[i], wide, sizeof( wide ) );
			int tw, th;
			vgui::surface()->GetTextSize( m_hTabFont, wide, tw, th );
			vgui::surface()->DrawSetTextFont( m_hTabFont );
			vgui::surface()->DrawSetTextColor( active || close ? Color( 255, 255, 255, 255 ) : Color( 200, 200, 200, 255 ) );
			vgui::surface()->DrawSetTextPos( ( x0 + x1 - tw ) / 2, m_nY0 + ( m_nTabTall - th ) / 2 );
			vgui::surface()->DrawPrintText( wide, wcslen( wide ) );
		}
	}

	virtual void OnMousePressed( vgui::MouseCode code )
	{
		int x, y;
		vgui::input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );
		if ( x < m_nX0 || x >= m_nX1 || y < m_nY0 || y >= m_nY1 )
		{
			SetVisible( false );	// tap outside closes, like letting go of Q in GMod
			return;
		}
		if ( y >= m_nY0 + m_nTabTall )
			return;
		for ( int i = 0; i <= ARRAYSIZE( s_Tabs ); i++ )
		{
			int x0, x1;
			TabRect( i, x0, x1 );
			if ( x < x0 || x >= x1 )
				continue;
			if ( i == ARRAYSIZE( s_Tabs ) )
				SetVisible( false );
			else
				SelectTab( i );
			return;
		}
	}

	MESSAGE_FUNC_INT( OnSpawnListSelected, "SpawnListSelected", index )
	{
		if ( g_GMPSpawnLists.IsValidIndex( index ) )
			m_pGrid->SetItems( &g_GMPSpawnLists[index]->items );
	}

private:
	static const char *const s_Tabs[4];

	void SelectTab( int tab )
	{
		m_nTab = tab;
		if ( tab == 0 )
			OnSpawnListSelected( m_pLists->Selected() );
		else if ( tab == 3 )
			m_pGrid->SetItems( &g_GMPNPCs );
		else
			m_pGrid->SetItems( tab == 1 ? &g_GMPEntities : &g_GMPWeapons );
		InvalidateLayout();
		Repaint();
	}

	void TabRect( int i, int &x0, int &x1 ) const
	{
		const int wide = ( m_nX1 - m_nX0 ) / 6, gap = m_nTabTall / 12;
		if ( i == ARRAYSIZE( s_Tabs ) )	// close button at the right end
		{
			x1 = m_nX1;
			x0 = m_nX1 - m_nTabTall * 3 / 2;
			return;
		}
		x0 = m_nX0 + i * ( wide + gap );
		x1 = x0 + wide;
	}

	static vgui::HFont MakeFont( const char *family, int tall, int weight )
	{
		vgui::HFont font = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( font, family, tall, weight, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS );
		return font;
	}

	CGMPSpawnListPanel *m_pLists;
	CGMPSpawnGrid *m_pGrid;
	vgui::HFont m_hTabFont, m_hListFont, m_hHeaderFont, m_hLabelFont;
	int m_nTab, m_nX0, m_nX1, m_nY0, m_nY1, m_nTabTall;
};

const char *const CGMPSpawnMenu::s_Tabs[4] = { "Spawnlists", "Entities", "Weapons", "NPCs" };

static vgui::DHANDLE<CGMPSpawnMenu> g_hGMPSpawnMenu;

// Touch look/move stays off while the menu is open (cdll_client_int.cpp IN_TouchEvent), or
// scrolling it would also turn the camera.
extern bool ( *g_pfnGMPTouchBlocked )( void );
static bool GMP_SpawnMenuIsOpen( void )
{
	return g_hGMPSpawnMenu.Get() && g_hGMPSpawnMenu->IsVisible();
}

// +menu toggles rather than shows-while-held: a touch button cannot stay pressed while
// another finger taps the menu.
static void GMP_SpawnMenuDown( const CCommand &args )
{
	if ( !g_hGMPSpawnMenu.Get() )
	{
		g_hGMPSpawnMenu = new CGMPSpawnMenu( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
		g_pfnGMPTouchBlocked = GMP_SpawnMenuIsOpen;
	}
	g_hGMPSpawnMenu->Toggle();
}
static void GMP_SpawnMenuUp( const CCommand &args ) {}
static ConCommand gmp_menu_down( "+menu", GMP_SpawnMenuDown, "GModPort: toggle the spawn menu" );
static ConCommand gmp_menu_up( "-menu", GMP_SpawnMenuUp );
#endif // CLIENT_DLL
