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
#include "gmp_undo.inl"
#include "gmp_npc_server.inl"
#endif

// GMod sandbox: tools, constraints, entities, modifiers, duplicator / saves / vehicles.
#include "gmp_tooldefs.inl"
#ifndef CLIENT_DLL
#include "gmp_gmfile.inl"
#include "gmp_constraints.inl"
#endif
#include "gmp_sents.inl"
#include "gmp_entmods.inl"
#include "gmp_dupe.inl"
#include "gmp_construct_watcher.inl"

#ifdef CLIENT_DLL
#define CWeaponGMPPhysGun C_WeaponGMPPhysGun
#endif

// GMod's cl_weaponcolor: an RGB tint (components up to ~4) for the physgun's glow and beam.
#define GMP_WEAPONCOLOR_DEFAULT	0.30f, 1.80f, 2.10f

#ifdef CLIENT_DLL
static ConVar cl_weaponcolor( "cl_weaponcolor", "0.30 1.80 2.10", FCVAR_USERINFO | FCVAR_ARCHIVE, "GModPort: physgun colour (r g b)" );
#endif

static bool GMP_ParseWeaponColor( const char *pValue, Vector &color )
{
	float r, g, b;
	if ( !pValue || sscanf( pValue, "%f %f %f", &r, &g, &b ) != 3 )
		return false;
	color.Init( clamp( r, 0.0f, 4.0f ), clamp( g, 0.0f, 4.0f ), clamp( b, 0.0f, 4.0f ) );
	return true;
}

class CWeaponGMPPhysGun : public CBaseHL2MPCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponGMPPhysGun, CBaseHL2MPCombatWeapon );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponGMPPhysGun( void )
	{
		m_bBeam = false;
		m_vecColor.Init( GMP_WEAPONCOLOR_DEFAULT );
#ifdef CLIENT_DLL
		m_pBeam = NULL;
#endif
#ifndef CLIENT_DLL
		m_flDistance = 0;
		m_bWaitForRelease = false;
		m_iGrabBone = 0;
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
	// The local player's own gun follows cl_weaponcolor at once, without waiting for the server.
	Vector	GetWeaponColor( void )
	{
		Vector color;
		C_BasePlayer *pOwner = ToBasePlayer( GetOwner() );
		if ( pOwner && pOwner->IsLocalPlayer() && GMP_ParseWeaponColor( cl_weaponcolor.GetString(), color ) )
			return color;
		return m_vecColor;
	}
#endif

	CNetworkVar( bool, m_bBeam );		// carrying: draw the beam to m_vecBeamEnd
	CNetworkVector( m_vecBeamEnd );		// world position of the grabbed point
	CNetworkVector( m_vecColor );		// the owner's cl_weaponcolor (GMod's weapon colour)

private:
#ifdef CLIENT_DLL
	class CGMPPhysBeam *m_pBeam;
#endif
	void	Release( bool bFreeze );

#ifndef CLIENT_DLL
	void	Grab( CBasePlayer *pOwner );
	void	Carry( CBasePlayer *pOwner );
	IPhysicsObject *HeldPhys( void );

	CGrabController	m_grab;
	float			m_flDistance;
	bool			m_bWaitForRelease;	// after a freeze, ignore +attack until it is let go
	int				m_iGrabBone;		// held physics object (ragdoll bone; 0 for props)

	DECLARE_ACTTABLE();
#endif

	CWeaponGMPPhysGun( const CWeaponGMPPhysGun & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGMPPhysGun, DT_WeaponGMPPhysGun )

BEGIN_NETWORK_TABLE( CWeaponGMPPhysGun, DT_WeaponGMPPhysGun )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bBeam ) ),
	RecvPropVector( RECVINFO( m_vecBeamEnd ) ),
	RecvPropVector( RECVINFO( m_vecColor ) ),
#else
	SendPropBool( SENDINFO( m_bBeam ) ),
	SendPropVector( SENDINFO( m_vecBeamEnd ), -1, SPROP_COORD ),
	SendPropVector( SENDINFO( m_vecColor ), 8, 0, 0.0f, 4.0f ),
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
	Vector color;
	if ( GMP_ParseWeaponColor( engine->GetClientConVarValue( pOwner->entindex(), "cl_weaponcolor" ), color ) && color != m_vecColor.Get() )
		m_vecColor = color;

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
	IPhysicsObject *pPhys = HeldPhys();
	m_grab.DetachEntity( bFreeze );
	// Freezing a ragdoll freezes only the held bone, as in GMod.
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
	// Ragdolls are held by the bone that was hit (tr.physicsbone indexes the object list).
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	const int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE( pList ) );
	if ( count <= 0 )
		return;
	m_iGrabBone = ( tr.physicsbone > 0 && tr.physicsbone < count ) ? tr.physicsbone : 0;
	IPhysicsObject *pPhys = pList[m_iGrabBone];
	if ( !pPhys )
		return;

	// Frozen objects (EnableMotion(false)) unfreeze when grabbed again, as in GMod.
	pPhys->EnableMotion( true );
	Pickup_OnPhysGunPickup( pEntity, pOwner, PICKED_UP_BY_CANNON );
	m_grab.SetIgnorePitch( false );
	m_grab.SetAngleAlignment( 0 );
	m_grab.AttachEntity( pOwner, pEntity, pPhys, false, tr.endpos, false );
	// Hold the object by the point that was hit, not by its center (in the held object's frame).
	matrix3x4_t physToWorld;
	pPhys->GetPositionMatrix( &physToWorld );
	VectorITransform( tr.endpos, physToWorld, m_grab.m_attachedPositionObjectSpace );
	m_flDistance = ( tr.endpos - start ).Length();
}

void CWeaponGMPPhysGun::Carry( CBasePlayer *pOwner )
{
	IPhysicsObject *pPhys = HeldPhys();
	if ( !pPhys )
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

	matrix3x4_t physToWorld;
	pPhys->GetPositionMatrix( &physToWorld );
	Vector grabbed;
	VectorTransform( m_grab.m_attachedPositionObjectSpace, physToWorld, grabbed );
	m_vecBeamEnd = grabbed;
}

IPhysicsObject *CWeaponGMPPhysGun::HeldPhys( void )
{
	CBaseEntity *pEntity = m_grab.GetAttached();
	if ( !pEntity )
		return NULL;
	IPhysicsObject *pList[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	const int count = pEntity->VPhysicsGetObjectList( pList, ARRAYSIZE( pList ) );
	return m_iGrabBone < count ? pList[m_iGrabBone] : NULL;
}

// gmp_spawn <models/...mdl> [skin]: spawn a prop where you look, resting on the surface as in
// GMod. Models with a multi-solid collision model ($ragdoll) become a prop_ragdoll, as in GMod;
// models without one become a solid prop_dynamic instead of failing.
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
	const vcollide_t *pCollide = pMdl ? modelinfo->GetVCollide( pMdl ) : NULL;
	const bool bPhysics = pCollide != NULL;
	const bool bRagdoll = pCollide && pCollide->solidCount > 1;
	CBaseEntity *pProp = CreateEntityByName( bRagdoll ? "prop_ragdoll" : bPhysics ? "prop_physics_override" : "prop_dynamic_override" );
	if ( !pProp )
		return;
	pProp->KeyValue( "model", pModel );
	if ( args.ArgC() > 2 )
		pProp->KeyValue( "skin", args[2] );
	if ( !bPhysics )
		pProp->KeyValue( "solid", "2" );
	Vector origin = tr.endpos;
	if ( bRagdoll )
	{
		// The ragdoll is built from its bind pose at spawn, so lift it by the model's bounds up front.
		Vector mins, maxs;
		modelinfo->GetModelBounds( pMdl, mins, maxs );
		origin -= tr.plane.normal * MIN( mins.z, 0.0f );
		origin += tr.plane.normal * 2.0f;
	}
	pProp->SetAbsOrigin( origin );
	pProp->SetAbsAngles( QAngle( 0, pPlayer->EyeAngles().y + 180, 0 ) );
	if ( DispatchSpawn( pProp ) < 0 )
	{
		UTIL_Remove( pProp );
		return;
	}
	pProp->Activate();
	if ( !bRagdoll )
	{
		origin = tr.endpos - tr.plane.normal * pProp->CollisionProp()->OBBMins().z;
		pProp->Teleport( &origin, NULL, NULL );
	}
	GMP_AddUndo( pPlayer, pProp, bRagdoll ? "Ragdoll" : "Prop" );
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
	GMP_AddUndo( pPlayer, pEnt, args[1] );
	Msg( "gmp_ent: %s\n", args[1] );
}

// gmp_give <weapon_...>: the spawn menu's Weapons tab. Gives the weapon (once) plus ammo and
// switches to it.
CON_COMMAND( gmp_give, "GModPort: gmp_give <weapon_...> - give a weapon from the spawn menu" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || !pPlayer->IsAlive() || args.ArgC() < 2 || ( Q_strnicmp( args[1], "weapon_", 7 ) && Q_stricmp( args[1], "gmod_tool" ) ) )
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
#include "tier1/fmtstr.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "matsys_controls/mdlpanel.h"
#include "datacache/imdlcache.h"
#include "ienginevgui.h"
#include "iinput.h"
#include "beamdraw.h"
#include "functionproxy.h"
#include "baseviewmodel_shared.h"

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
		// The weapon colour, scaled so its brightest component is 1 (the gun's tint goes up to ~4).
		Vector color = m_pWeapon->GetWeaponColor();
		const float brightest = MAX( color.x, MAX( color.y, color.z ) );
		color = brightest > 0.01f ? color / brightest : Vector( 1, 1, 1 );
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

// GMod's PlayerWeaponColor material proxy (the physgun's vmts: resultVar $selfillumtint). Engines
// without it leave the glow at its vmt value; this feeds it the gun's colour, viewmodel included.
class CGMPPlayerWeaponColorProxy : public CResultProxy
{
public:
	virtual void OnBind( void *pArg )
	{
		Vector color( GMP_WEAPONCOLOR_DEFAULT );
		C_BaseEntity *pEntity = BindArgToEntity( pArg );
		C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel *>( pEntity );
		if ( pViewModel )
			pEntity = pViewModel->GetOwningWeapon();
		C_WeaponGMPPhysGun *pGun = dynamic_cast<C_WeaponGMPPhysGun *>( pEntity );
		if ( pGun )
			color = pGun->GetWeaponColor();
		else if ( !GMP_ParseWeaponColor( cl_weaponcolor.GetString(), color ) )	// spawn icons, previews
			color.Init( GMP_WEAPONCOLOR_DEFAULT );
		m_pResult->SetVecValue( color.x, color.y, color.z );
	}
};
EXPOSE_INTERFACE( CGMPPlayerWeaponColorProxy, IMaterialProxy, "PlayerWeaponColor" IMATERIAL_PROXY_INTERFACE_VERSION );

// Use the engine selector, including slot icons, timeout, attack confirmation and hud_fastswitch.
CON_COMMAND( gmp_weapnext, "GModPort: open/cycle the native weapon selector forward" )
{
    C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
    if ( player && player->IsAlive() ) engine->ClientCmd( "invnext" );
}
CON_COMMAND( gmp_weapprev, "GModPort: open/cycle the native weapon selector backward" )
{
    C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
    if ( player && player->IsAlive() ) engine->ClientCmd( "invprev" );
}

// GMod-style spawn menu: GMod's own spawnlists (settings/spawnlist*/ *.txt) on the left, a grid
// of live model icons on the right, a Weapons tab. Drag scrolls, a tap spawns.

struct GMPSpawnItem
{
	CUtlString model;	// empty: a header row
	CUtlString icon;	// original materials/ PNG, when supplied by content metadata
	CUtlString text;	// header text or the label under the icon
	CUtlString command;	// client command run on tap; on a header it makes the header a button
	CUtlString file;	// Dupes / Saves: the file the corner X deletes (with its .txt)
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
static CUtlVector<GMPSpawnItem> g_GMPVehicleItems;
static CUtlVector<GMPSpawnItem> g_GMPDupeItems;
static CUtlVector<GMPSpawnItem> g_GMPSaveItems;

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
	{ "gmod_tool", "Tool Gun", "models/weapons/w_toolgun.mdl" },
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
                item.icon = sub->GetString( "material" );
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
        item.icon = CFmtStr("entities/%s.png",s_GMPWeaponTable[i].cls).Access();
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
            item.icon = CFmtStr("entities/%s.png",s_GMPEntityTable[i].cls).Access();
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

	// Vehicles tab (gmp_dupe.inl): one header per category.
	for ( int i = 0; i < ARRAYSIZE( s_GMPVehicles ); i++ )
	{
		if ( !i || Q_strcmp( s_GMPVehicles[i].category, s_GMPVehicles[i - 1].category ) )
		{
			header.text = s_GMPVehicles[i].category;
			g_GMPVehicleItems.AddToTail( header );
		}
		GMPSpawnItem item;
		item.model = s_GMPVehicles[i].model;
		item.text = s_GMPVehicles[i].name;
		char cmd[96];
		Q_snprintf( cmd, sizeof( cmd ), "gmp_vehicle %s", s_GMPVehicles[i].name );
		item.command = cmd;
		item.skin = 0;
		g_GMPVehicleItems.AddToTail( item );
	}
}

// Dupes / Saves tabs: the files in dupes/ or saves/ (MOD path, newest first), each shown with the
// biggest model from its .txt (GMP_DupeWriteInfo). The first row is a button that saves a new one.
static void GMP_LoadFileItems( CUtlVector<GMPSpawnItem> &items, const char *dir, const char *ext, bool dupe )
{
	items.RemoveAll();
	GMPSpawnItem button;
	button.text = dupe ? "+ Save the Duplicator's copy" : "+ Save this map";
	button.command = dupe ? "gm_dupe_save" : "gm_save";
	button.skin = 0;
	items.AddToTail( button );

	struct Found { CUtlString name; long long time; };
	CUtlVector<Found> found;
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "%s/*.%s", dir, ext );
	FileFindHandle_t hFind;
	for ( const char *name = filesystem->FindFirstEx( path, "MOD", &hFind ); name; name = filesystem->FindNext( hFind ) )
	{
		if ( filesystem->FindIsDirectory( hFind ) )
			continue;
		Q_snprintf( path, sizeof( path ), "%s/%s", dir, name );
		Found f = { name, (long long)filesystem->GetFileTime( path, "MOD" ) };
		found.AddToTail( f );
	}
	filesystem->FindClose( hFind );
	for ( int i = 1; i < found.Count(); i++ )	// newest first
		for ( int j = i; j > 0 && found[j].time > found[j - 1].time; j-- )
		{
			Found t = found[j]; found[j] = found[j - 1]; found[j - 1] = t;
		}

	for ( int i = 0; i < found.Count(); i++ )
	{
		char base[MAX_PATH];
		Q_StripExtension( found[i].name.Get(), base, sizeof( base ) );
		GMPSpawnItem item;
		item.skin = 0;
		Q_snprintf( path, sizeof( path ), "%s/%s", dir, found[i].name.Get() );
		item.file = path;
		char cmd[MAX_PATH + 32];
		if ( dupe )
			Q_snprintf( cmd, sizeof( cmd ), "gm_dupe_open %s", path );
		else
			Q_snprintf( cmd, sizeof( cmd ), "gm_load %s", base );
		item.command = cmd;
		Q_snprintf( path, sizeof( path ), "%s/%s.txt", dir, base );
		KeyValues *pInfo = new KeyValues( "DupeInfo" );
		const bool info = pInfo->LoadFromFile( filesystem, path, "MOD" );
		item.model = info ? pInfo->GetString( "model" ) : "";
		if ( item.model.IsEmpty() )
			item.model = "models/props_junk/wood_crate001a.mdl";	// nothing spawned: still a tappable cell
		char label[128];
		if ( info && !dupe )
			Q_snprintf( label, sizeof( label ), "%s (%d)", base, pInfo->GetInt( "entities" ) );
		else
			Q_strncpy( label, base, sizeof( label ) );
		item.text = label;
		pInfo->deleteThis();
		items.AddToTail( item );
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

    virtual void OnMouseCaptureLost(){m_bPressed=false;m_bDragged=false;BaseClass::OnMouseCaptureLost();}

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
#include "gmp_spawn_icon.inl"

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
		bool props = false;
        for(int i=0;m_pItems&&i<m_pItems->Count();++i)if(!(*m_pItems)[i].model.IsEmpty()){props=!Q_strnicmp((*m_pItems)[i].command.Get(),"gmp_spawn ",10);break;}
        // SpawnIcon is 64x64; ContentIcon is 128x128 in the pinned Sandbox source.
        m_nCell = props ? 64 : 128;
        m_nGap = 4;
		const int cols = MAX( 1, ( w - m_nGap ) / ( m_nCell + m_nGap ) );
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
        if(!m_pItems||!m_pItems->Count())DrawText(m_hFont,4,4,w-8,MAX(30,h/10),"No items. Check your search or installed content.",Color(80,80,80,255),true);
		for ( int i = 0; i < m_Places.Count(); i++ )
		{
			const Place &place = m_Places[i];
			const int y = place.y - m_nScroll;
			if ( y + place.h < 0 || y > h )
				continue;
			const GMPSpawnItem &item = ( *m_pItems )[place.item];
			if ( item.model.IsEmpty() && !item.command.IsEmpty() )
			{
				// A header that runs a command: a button (Save on the Dupes / Saves tabs).
				vgui::surface()->DrawSetColor( 60, 140, 230, 255 );
				vgui::surface()->DrawFilledRect( place.x, y, place.x + place.w, y + place.h );
				DrawText( m_hHeaderFont, place.x, y, place.w, place.h, item.text.Get(), Color( 255, 255, 255, 255 ), true );
				continue;
			}
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
			if ( !item.file.IsEmpty() )
			{
				// Delete corner: the icon panel starts below it (UpdateCells), or it would cover it.
				const int xs = DeleteSize( place );
				vgui::surface()->DrawSetColor( 200, 70, 60, 255 );
				vgui::surface()->DrawFilledRect( place.x + place.w - xs, y, place.x + place.w, y + xs );
				DrawText( m_hFont, place.x + place.w - xs, y, xs, xs, "X", Color( 255, 255, 255, 255 ), true );
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
			if ( !item.file.IsEmpty() && x >= place.x + place.w - DeleteSize( place ) && y < place.y + DeleteSize( place ) )
			{
				PostActionSignal( new KeyValues( "DeleteFile", "file", item.file.Get() ) );
				return;
			}
			engine->ClientCmd( item.command.Get() );
			PostActionSignal( new KeyValues( "ItemTapped" ) );
			return;
		}
	}

private:
	struct Place { int item, x, y, w, h; bool missing; };
	struct Cell { CGMPSpawnIconPanel *panel; int item; };

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
					Cell cell = { new CGMPSpawnIconPanel( this ), -1 };
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
			const int topTall = ( *m_pItems )[place.item].file.IsEmpty() ? 0 : DeleteSize( place );
			m_Cells[c].panel->SetBounds( place.x + inset, place.y - m_nScroll + inset + topTall, place.w - 2 * inset, place.h - 2 * inset - labelTall - topTall );
			m_Cells[c].panel->SetVisible( !place.missing );
		}
		for ( int c = 0; c < m_Cells.Count(); c++ )
			if ( m_Cells[c].item < 0 )
				m_Cells[c].panel->SetVisible( false );
	}

	static bool LoadIcon( CGMPSpawnIconPanel *panel, const GMPSpawnItem &item )
    {
        panel->ClearImage();panel->SetMDL(MDLHANDLE_INVALID);
        if(!item.icon.IsEmpty()&&!strstr(item.icon.Get(),"..")){
            char path[MAX_PATH];Q_snprintf(path,sizeof(path),"materials/%s",item.icon.Get());
            if(panel->LoadImage(path))return true;
        }
        // Only consume an installed cache hit. Missing/invalid images use the native model preview.
        char model[MAX_PATH],path[MAX_PATH];Q_strncpy(model,item.model.Get(),sizeof(model));
        V_FixSlashes(model);Q_strlower(model);V_StripExtension(model,model,sizeof(model));
        if(!strstr(model,"..")&&!Q_strnicmp(model,"models/",7)){
            if(item.skin>0)Q_snprintf(path,sizeof(path),"materials/spawnicons/%s_skin%d.png",model,item.skin);
            else Q_snprintf(path,sizeof(path),"materials/spawnicons/%s.png",model);
            if(panel->LoadImage(path))return true;
        }
		MDLHandle_t hMDL = mdlcache->FindMDL( item.model.Get() );
		mdlcache->GetStudioHdr( hMDL );	// loads it; a missing file becomes the error model
		const bool ok = !mdlcache->IsErrorModel( hMDL );
		panel->SetMDL( ok ? hMDL : MDLHANDLE_INVALID );
		mdlcache->Release( hMDL );
		if ( !ok )
			return false;
		panel->SetSkin( item.skin );
		if ( !Q_strnicmp( item.model.Get(), "models/player/", 14 ) )
		{
			// GMod's player model icons: head and shoulders from the front (a player stands 72 units tall).
			panel->SetCameraPositionAndAngles( vec3_origin, QAngle( 0, 180, 0 ) );
			panel->LookAt( Vector( 0, 0, 58 ), 18.0f );
			return true;
		}
		// GMod's spawn icon view: from the front left, slightly above.
		panel->SetCameraPositionAndAngles( vec3_origin, QAngle( 25, 220, 0 ) );
		panel->LookAtMDL();
		return true;
	}

	static int DeleteSize( const Place &place ) { return MAX( 12, place.w / 5 ); }

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

#include "gmp_spawn_tools.inl"

class CGMPSpawnMenu : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CGMPSpawnMenu, vgui::EditablePanel );
public:
	CGMPSpawnMenu( vgui::VPANEL parent ) : BaseClass( NULL, "GMPSpawnMenu" ),
		m_hTabFont( 0 ), m_hListFont( 0 ), m_hHeaderFont( 0 ), m_hLabelFont( 0 ), m_nTab( 0 ), m_nToolTab(0)
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
		m_pGrid->AddActionSignalTarget( this );
		m_pTools = new CGMPSpawnTools(this);
		m_pSearch = new CGMPQTextEntry(this,"Search");
		m_pSearch->SetText("");static_cast<CGMPQTextEntry *>(m_pSearch)->SetHint("Search name or model...");
		m_pSearch->AddActionSignalTarget(this);
		m_pSource = NULL;
		m_flRefreshAt = 0;
		SetVisible( false );
	}

	void Toggle( void )
	{
		if ( IsVisible() )
		{
			Close();
			return;
		}
		GMP_LoadSpawnMenu();
		GMP_LoadNPCMenu();
		SetBounds( 0, 0, ScreenWidth(), ScreenHeight() );
		InvalidateLayout(true);
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
		m_hTabFont = MakeFont( family, MAX(12,tall / 55), 500,m_hTabFont );
		m_hListFont = MakeFont( family, MAX(12,tall / 50), 500,m_hListFont );
		m_hHeaderFont = MakeFont( family, MAX(14,tall / 42), 700,m_hHeaderFont );
		m_hLabelFont = MakeFont( family, MAX(12,tall / 50), 500,m_hLabelFont );
		m_pLists->SetFonts( m_hListFont, m_hTabFont );
		m_pGrid->SetFonts( m_hLabelFont, m_hHeaderFont );
        m_pTools->SetFont(m_hListFont);m_pSearch->SetFont(m_hListFont);
	}

	virtual void PerformLayout( void )
    {
        BaseClass::PerformLayout();gmpui::Layout layout(GetWide(),GetTall(),true,1);
        m_nX0=layout.x;m_nX1=layout.x+layout.width;m_nY0=layout.y;m_nY1=layout.y+layout.height;m_nTabTall=layout.tab;
        const int pad=4,contentRight=m_nX0+layout.left,top=m_nY0+m_nTabTall+pad;
        m_nContentRight=contentRight;
        m_pTools->SetBounds(contentRight+layout.split,top,layout.tools,m_nY1-top);
        m_pSearch->SetBounds(m_nX0+pad,top,layout.left-pad*2,layout.search);
        const int gridTop=top+layout.search+pad,listWide=m_nTab==0?layout.tree:0;
        m_pLists->SetVisible(listWide>0);m_pLists->SetBounds(m_nX0+pad,gridTop,listWide,m_nY1-gridTop-pad);
        const int gridX=m_nX0+pad+(listWide?listWide+pad:0);
        m_pGrid->SetBounds(gridX,gridTop,MAX(1,contentRight-pad-gridX),MAX(1,m_nY1-gridTop-pad));
    }

    virtual void Paint( void )
    {
        if(!s_GMPQSkin.Draw("Tab_Control",m_nX0,m_nY0+m_nTabTall-8,m_nContentRight-m_nX0,m_nY1-m_nY0-m_nTabTall+8)){
            vgui::surface()->DrawSetColor(170,170,170,255);vgui::surface()->DrawFilledRect(m_nX0,m_nY0+m_nTabTall,m_nContentRight,m_nY1);
        }
        s_GMPQSkin.Draw("Tab_Control",m_nContentRight+6,m_nY0+m_nTabTall-8,m_nX1-m_nContentRight-6,m_nY1-m_nY0-m_nTabTall+8);
        for(int i=0;i<ARRAYSIZE(s_Tabs);++i){int x0,x1;TabRect(i,x0,x1);s_GMPQSkin.Draw(i==m_nTab?"TabT_Active":"TabT_Inactive",x0,m_nY0+(i==m_nTab?0:2),x1-x0,m_nTabTall);PaintText(s_Tabs[i],x0,m_nY0,x1-x0,m_nTabTall,Color(25,25,25,255));}
        const int toolWidth=(m_nX1-m_nContentRight-6)/2;
        for(int i=0;i<2;++i){int x=m_nContentRight+6+i*toolWidth;s_GMPQSkin.Draw(i==m_nToolTab?"TabT_Active":"TabT_Inactive",x,m_nY0,toolWidth,m_nTabTall);PaintText(i==0?"Tools":"Utilities",x,m_nY0,toolWidth,m_nTabTall,Color(25,25,25,255));}
        PaintText("X",GetWide()-MAX(24,m_nTabTall),0,MAX(24,m_nTabTall),MAX(24,m_nTabTall),Color(220,70,70,255));
    }
    void PaintText(const char *text,int x,int y,int w,int h,Color color){
        wchar_t wide[128];g_pVGuiLocalize->ConvertANSIToUnicode(text,wide,sizeof(wide));int tw,th;vgui::surface()->GetTextSize(m_hTabFont,wide,tw,th);
        vgui::surface()->DrawSetTextFont(m_hTabFont);vgui::surface()->DrawSetTextColor(color);vgui::surface()->DrawSetTextPos(x+MAX(0,(w-tw)/2),y+(h-th)/2);vgui::surface()->DrawPrintText(wide,wcslen(wide));
    }

    void Close(){vgui::input()->SetMouseCapture(NULL);SDL_StopTextInput();SetKeyBoardInputEnabled(false);SetVisible(false);}
    virtual void OnKeyCodeTyped(vgui::KeyCode code){if(code==KEY_ESCAPE){Close();return;}BaseClass::OnKeyCodeTyped(code);}
    virtual void OnMousePressed(vgui::MouseCode code)
    {
        if(code!=MOUSE_LEFT)return;int x,y;vgui::input()->GetCursorPos(x,y);ScreenToLocal(x,y);
        if((x>=GetWide()-MAX(24,m_nTabTall)&&y<MAX(24,m_nTabTall))||x<m_nX0||x>=m_nX1||y<m_nY0||y>=m_nY1){Close();return;}
        if(y>=m_nY0+m_nTabTall)return;
        if(x>m_nContentRight){m_nToolTab=x<m_nContentRight+6+(m_nX1-m_nContentRight-6)/2?0:1;m_pTools->ShowUtilities(m_nToolTab==1);Repaint();return;}
        for(int i=0;i<ARRAYSIZE(s_Tabs);++i){int x0,x1;TabRect(i,x0,x1);if(x>=x0&&x<x1){SelectTab(i);return;}}
    }

	MESSAGE_FUNC_INT( OnSpawnListSelected, "SpawnListSelected", index )
	{
		if ( g_GMPSpawnLists.IsValidIndex( index ) )
			SetSource( &g_GMPSpawnLists[index]->items );
	}

	// Dupes / Saves: the corner X deletes the file and its .txt.
	MESSAGE_FUNC_CHARPTR( OnDeleteFile, "DeleteFile", file )
	{
		if ( ( Q_strnicmp( file, "dupes/", 6 ) && Q_strnicmp( file, "saves/", 6 ) ) || Q_strstr( file, ".." ) )
			return;
		char info[MAX_PATH];
		Q_strncpy( info, file, sizeof( info ) );
		Q_SetExtension( info, ".txt", sizeof( info ) );
		filesystem->RemoveFile( file, "MOD" );
		filesystem->RemoveFile( info, "MOD" );
		SelectTab( m_nTab );
	}

	// The server writes the new dupe / save a frame or two after the Save tap.
	MESSAGE_FUNC( OnItemTapped, "ItemTapped" )
	{
		if ( m_nTab == TAB_DUPES || m_nTab == TAB_SAVES )
			m_flRefreshAt = Plat_FloatTime() + 0.5;
	}

	virtual void OnThink( void )
	{
		BaseClass::OnThink();
        if(IsVisible()&&enginevgui->IsGameUIVisible())Close();
        if(GetWide()!=ScreenWidth()||GetTall()!=ScreenHeight()){SetBounds(0,0,ScreenWidth(),ScreenHeight());InvalidateLayout(true,true);}
		if ( m_flRefreshAt && Plat_FloatTime() >= m_flRefreshAt )
		{
			m_flRefreshAt = 0;
			if ( m_nTab == TAB_DUPES || m_nTab == TAB_SAVES )
				SelectTab( m_nTab );
		}
	}

private:
	enum { TAB_SPAWNLISTS, TAB_ENTITIES, TAB_WEAPONS, TAB_NPCS, TAB_VEHICLES, TAB_DUPES, TAB_SAVES };
	static const char *const s_Tabs[7];

	void SelectTab( int tab )
	{
		m_nTab = tab;
		switch ( tab )
		{
		case TAB_SPAWNLISTS: if(g_GMPSpawnLists.Count())OnSpawnListSelected(m_pLists->Selected());else SetSource(NULL);break;
		case TAB_ENTITIES: SetSource( &g_GMPEntities ); break;
		case TAB_WEAPONS: SetSource( &g_GMPWeapons ); break;
		case TAB_NPCS: SetSource( &g_GMPNPCs ); break;
		case TAB_VEHICLES: SetSource( &g_GMPVehicleItems ); break;
		case TAB_DUPES:
			GMP_LoadFileItems( g_GMPDupeItems, "dupes", "dupe", true );
			SetSource( &g_GMPDupeItems );
			break;
		default:
			GMP_LoadFileItems( g_GMPSaveItems, "saves", "gms", false );
			SetSource( &g_GMPSaveItems );
			break;
		}
		InvalidateLayout();
		Repaint();
	}

    void SetSource(const CUtlVector<GMPSpawnItem> *items) { m_pSource=items;FilterItems(); }
    MESSAGE_FUNC(OnSearchChanged,"TextChanged") { FilterItems(); }
    void FilterItems() {
        char query[160];m_pSearch->GetText(query,sizeof(query));m_Filtered.RemoveAll();
        int header=-1;bool added=false;
        for(int i=0;m_pSource&&i<m_pSource->Count();++i){
            const GMPSpawnItem &item=(*m_pSource)[i];
            if(!*query){m_Filtered.AddToTail(item);continue;}
            if(item.model.IsEmpty()&&item.command.IsEmpty()){header=i;added=false;continue;}
            if(gmpui::Matches(query,item.text.Get(),item.model.Get())){
                if(header>=0&&!added){m_Filtered.AddToTail((*m_pSource)[header]);added=true;}
                m_Filtered.AddToTail(item);
            }
        }
        m_pGrid->SetItems(&m_Filtered);
    }


    void TabRect(int i,int &x0,int &x1)const{
        const int count=ARRAYSIZE(s_Tabs),width=m_nContentRight-m_nX0;
        x0=m_nX0+width*i/count;x1=m_nX0+width*(i+1)/count;
    }

	static vgui::HFont MakeFont( const char *family, int tall, int weight,vgui::HFont font=0 )
	{
		if(!font)font = vgui::surface()->CreateFont();
		vgui::surface()->SetFontGlyphSet( font, family, tall, weight, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS );
		return font;
	}

	CGMPSpawnListPanel *m_pLists;
	CGMPSpawnGrid *m_pGrid;
    CGMPSpawnTools *m_pTools;
    vgui::TextEntry *m_pSearch;
    const CUtlVector<GMPSpawnItem> *m_pSource;
    CUtlVector<GMPSpawnItem> m_Filtered;
	vgui::HFont m_hTabFont, m_hListFont, m_hHeaderFont, m_hLabelFont;
	int m_nTab,m_nToolTab,m_nContentRight, m_nX0, m_nX1, m_nY0, m_nY1, m_nTabTall;
	double m_flRefreshAt;
};

const char *const CGMPSpawnMenu::s_Tabs[7] = { "Spawnlists", "Entities", "Weapons", "NPCs", "Vehicles", "Dupes", "Saves" };

static vgui::DHANDLE<CGMPSpawnMenu> g_hGMPSpawnMenu;

// Touch look/move stays off while the menu is open (cdll_client_int.cpp IN_TouchEvent), or
// scrolling it would also turn the camera.
extern bool ( *g_pfnGMPTouchBlocked )( void );
static bool GMP_ContextMenuIsOpen( void );
static bool GMP_SpawnMenuIsOpen( void )
{
	return ( g_hGMPSpawnMenu.Get() && g_hGMPSpawnMenu->IsVisible() ) || GMP_ContextMenuIsOpen();
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
static void GMP_QMenuShutdown(){if(g_hGMPSpawnMenu.Get())g_hGMPSpawnMenu->Close();s_GMPQSkin.Shutdown();}
static ConCommand gmp_menu_down( "+menu", GMP_SpawnMenuDown, "GModPort: toggle the spawn menu" );
static ConCommand gmp_menu_up( "-menu", GMP_SpawnMenuUp );
#endif // CLIENT_DLL

#include "gmp_toolgun.inl"
#include "gmp_context_menu.inl"
