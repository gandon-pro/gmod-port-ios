//========= GModPort ==========================================================//
//
// Purpose: GMod constraints (constraint.lua) and numpad bindings. Server only, included from
//          gmp_toolgun.inl.
//
//          One gmp_constraint entity per GMod constraint. It keeps GMod's constraint table
//          (Type + the RegisterConstraint arguments except the entities), so the tool gun, the
//          duplicator and saves all build constraints through GMP_CreateConstraint().
//
//          gmp_numpad <key> <1|0>  (sent by the client numpad hook) runs the player's bindings.
//
//=============================================================================//

#include "rope.h"
#include "rope_shared.h"
#include "world.h"
#include "vphysics/constraints.h"
#include "phys_controller.h"

#define GMP_ROPE_DEFAULT_MATERIAL	"cable/rope"

//-----------------------------------------------------------------------------
// Numpad
//-----------------------------------------------------------------------------
class IGMPNumpadTarget
{
public:
	virtual void NumpadKey( CBasePlayer *pPlayer, int key, bool down ) = 0;
};

struct GMPNumpadBind_t
{
	int userid, key;
	EHANDLE ent;
};
static CUtlVector<GMPNumpadBind_t> s_GMPNumpadBinds;

static bool GMP_IsNumpadKey( int key ) { return key >= 37 && key <= 52; }	// KEY_PAD_0 .. KEY_PAD_DECIMAL

static void GMP_NumpadBind( CBasePlayer *pPlayer, int key, CBaseEntity *pEnt )
{
	if ( !pPlayer || !pEnt || !GMP_IsNumpadKey( key ) )
		return;
	const int userid = pPlayer->GetUserID();
	for ( int i = 0; i < s_GMPNumpadBinds.Count(); i++ )
		if ( s_GMPNumpadBinds[i].userid == userid && s_GMPNumpadBinds[i].key == key && s_GMPNumpadBinds[i].ent.Get() == pEnt )
			return;
	GMPNumpadBind_t bind = { userid, key, pEnt };
	s_GMPNumpadBinds.AddToTail( bind );
}

static void GMP_NumpadUnbind( CBaseEntity *pEnt )
{
	for ( int i = s_GMPNumpadBinds.Count() - 1; i >= 0; i-- )
		if ( s_GMPNumpadBinds[i].ent.Get() == pEnt )
			s_GMPNumpadBinds.Remove( i );
}

// numpad.Activate / Deactivate: runs pPlayer's bindings for key (also used by gmod_button).
static void GMP_NumpadRun( CBasePlayer *pPlayer, int key, bool down )
{
	if ( !pPlayer )
		return;
	const int userid = pPlayer->GetUserID();
	CUtlVector<EHANDLE> targets;
	for ( int i = s_GMPNumpadBinds.Count() - 1; i >= 0; i-- )
	{
		CBaseEntity *pEnt = s_GMPNumpadBinds[i].ent.Get();
		if ( !pEnt )
		{
			s_GMPNumpadBinds.Remove( i );
			continue;
		}
		if ( s_GMPNumpadBinds[i].userid == userid && s_GMPNumpadBinds[i].key == key )
			targets.AddToTail( s_GMPNumpadBinds[i].ent );
	}
	// The handlers may remove entities (dynamite), so the list is collected first.
	for ( int i = 0; i < targets.Count(); i++ )
	{
		IGMPNumpadTarget *pTarget = dynamic_cast<IGMPNumpadTarget *>( targets[i].Get() );
		if ( pTarget )
			pTarget->NumpadKey( pPlayer, key, down );
	}
}

CON_COMMAND( gmp_numpad, "GModPort: numpad key <key> <1 down | 0 up> (sent by the client)" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 3 )
		return;
	GMP_NumpadRun( pPlayer, atoi( args[1] ), atoi( args[2] ) != 0 );
}

//-----------------------------------------------------------------------------
// Shared helpers
//-----------------------------------------------------------------------------
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
// On a ragdoll bone other than the root it follows that bone's physics object every tick instead.
class CGMPRopeAnchor : public CBaseEntity
{
public:
	DECLARE_CLASS( CGMPRopeAnchor, CBaseEntity );
	int		ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	int		UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
	void	FollowBone( CBaseEntity *pEnt, int bone, IPhysicsObject *pPhys )
	{
		m_hBoneEnt = pEnt;
		m_iBone = bone;
		pPhys->WorldToLocal( &m_vecLocal, GetAbsOrigin() );
		SetThink( &CGMPRopeAnchor::BoneThink );
		SetNextThink( gpGlobals->curtime );
	}
	void	BoneThink( void )
	{
		IPhysicsObject *pPhys = GMP_ToolPhys( m_hBoneEnt, m_iBone );
		if ( !pPhys )
			return;
		Vector pos;
		pPhys->LocalToWorld( &pos, m_vecLocal );
		SetAbsOrigin( pos );
		SetNextThink( gpGlobals->curtime );
	}

private:
	EHANDLE	m_hBoneEnt;
	int		m_iBone;
	Vector	m_vecLocal;
};
LINK_ENTITY_TO_CLASS( gmp_rope_anchor, CGMPRopeAnchor );

static CBaseEntity *GMP_CreateRopeAnchor( CBaseEntity *pParent, int bone, const Vector &pos )
{
	CGMPRopeAnchor *pAnchor = static_cast<CGMPRopeAnchor *>( CreateEntityByName( "gmp_rope_anchor" ) );
	if ( !pAnchor )
		return NULL;
	pAnchor->SetAbsOrigin( pos );
	DispatchSpawn( pAnchor );
	IPhysicsObject *pBone = bone > 0 ? GMP_ToolPhys( pParent, bone ) : NULL;
	if ( pBone && pBone != GMP_ToolPhys( pParent, 0 ) )
		pAnchor->FollowBone( pParent, bone, pBone );
	else if ( pParent && !pParent->IsWorld() )
		pAnchor->SetParent( pParent );
	return pAnchor;
}

// constraint.CreateKeyframeRope: no rope for width <= 0, width clamped to 0.2..100.
// resize: the rope always spans its ends (elastic, winch, slider...); otherwise 'length' long.
static CRopeKeyframe *GMP_CreateRope( CBaseEntity *pA0, CBaseEntity *pA1, float width, const char *material, bool resize, float length )
{
	if ( width <= 0 || !pA0 || !pA1 )
		return NULL;
	char mat[MAX_PATH];
	Q_strncpy( mat, ( material && *material ) ? material : GMP_ROPE_DEFAULT_MATERIAL, sizeof( mat ) );
	if ( !Q_stristr( mat, ".vmt" ) )
		Q_strncat( mat, ".vmt", sizeof( mat ) );
	CRopeKeyframe *pRope = CRopeKeyframe::Create( pA0, pA1, 0, 0, 2, mat, 10 );
	if ( !pRope )
		return NULL;
	pRope->m_Width = clamp( width, 0.2f, 100.0f );
	pRope->EnableWind( false );
	pRope->m_Slack = 0;
	if ( resize )
		pRope->m_RopeFlags |= ROPE_RESIZE;
	else if ( length > 0 )
		pRope->m_RopeLength = (int)length;
	return pRope;
}

//-----------------------------------------------------------------------------
// gmp_constraint
//-----------------------------------------------------------------------------
class CGMPConstraint : public CLogicalEntity, public IPhysicsConstraintEvent, public IMotionEvent, public IGMPNumpadTarget
{
public:
	DECLARE_CLASS( CGMPConstraint, CLogicalEntity );

	CGMPConstraint( void );

	int		ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	bool	Build( KeyValues *pTable, CBaseEntity *const ents[4], CBasePlayer *pPlayer );
	void	UpdateOnRemove( void );
	void	WatchThink( void );

	bool	Uses( CBaseEntity *pEnt ) const;
	bool	IsType( const char *type ) const { return m_pTable && !Q_stricmp( m_pTable->GetString( "Type" ), type ); }
	KeyValues *GetTable( void ) const { return m_pTable; }
	CBaseEntity *GetEnt( int i ) const { return ( i >= 1 && i <= 4 ) ? m_hEnt[i - 1].Get() : NULL; }	// GMod's Ent1..Ent4
	bool	HasEnt( int i ) const { return i >= 1 && i <= 4 && m_bHasEnt[i - 1]; }
	// Motor: torque multiplier (signed, 0 = off). gmod_wheel drives it like phys_torque's Scale input.
	void	SetMotorScale( float scale ) { m_flMotorScale = scale; m_flMotorOff = 0; if ( m_pMotor ) m_pMotor->WakeObjects(); }
	bool	IsMotor( void ) const { return m_pMotor != NULL; }

	// IPhysicsConstraintEvent: a force limit broke the constraint.
	void	ConstraintBroken( IPhysicsConstraint * ) { if ( !( GetEFlags() & EFL_KILLME ) ) UTIL_Remove( this ); }
	// IMotionEvent: the motor.
	simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );
	// IGMPNumpadTarget: motor, winch, hydraulic, muscle.
	void	NumpadKey( CBasePlayer *pPlayer, int key, bool down );

private:
	float	F( const char *key, float def = 0 ) const { return m_pTable->GetFloat( key, def ); }
	bool	B( const char *key ) const { return m_pTable->GetInt( key, 0 ) != 0; }
	const char *S( const char *key ) const { return m_pTable->GetString( key, "" ); }
	int		Bone( int i ) const;
	IPhysicsObject *Phys( int i ) const { return GMP_ToolPhys( GetEnt( i ), Bone( i ) ); }
	Vector	LPos( int i ) const { return GMP_ParseVector( m_pTable->GetString( UTIL_VarArgs( "LPos%d", i ) ) ); }
	Vector	WPos( int i ) const;
	void	SetBreak( constraint_breakableparams_t &params, float forcelimit, float torquelimit ) const;
	void	Keep( IPhysicsConstraint *pConstraint );
	void	NoCollide( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2 );
	IPhysicsConstraint *CreateHinge( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, const Vector &wpos1, const Vector &wpos2, float forcelimit, float torquelimit, float friction );
	IPhysicsConstraint *CreateSlider( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, const Vector &wpos1, const Vector &wpos2 );
	bool	CreateSpring( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, float constant, float damping, float rdamping, bool stretchOnly );
	void	ElasticConsts( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, bool fixed, float &constant, float &damping ) const;
	void	AddRope( int i1, const Vector &w1, int i2, const Vector &w2, float width, const char *material, bool resize, float length );
	void	StartController( int type, float minLen, float maxLen );
	void	ControllerThink( void );

	KeyValues			*m_pTable;
	EHANDLE				m_hEnt[4];
	bool				m_bHasEnt[4];
	IPhysicsConstraint	*m_pConstraint[2];
	IPhysicsSpring		*m_pSpring;
	IPhysicsObject		*m_pNoCollide[2];
	EHANDLE				m_hRope[3], m_hAnchor[6], m_hExtra;

	// Motor (phys_torque)
	IPhysicsMotionController *m_pMotor;
	Vector	m_vecMotorAxis;		// local to Phys1
	int		m_iMotorDir;
	bool	m_bMotorOn;
	float	m_flMotorOff;		// forcetime
	float	m_flMotorScale;

	// Winch controller (gmod_winch_controller)
	bool	m_bController;
	int		m_iCtrlType;		// 0 winch / hydraulic, 1 muscle
	int		m_iDir;
	float	m_flLength, m_flMin, m_flMax, m_flCTime, m_flLast;
	bool	m_bExpanded;
};
LINK_ENTITY_TO_CLASS( gmp_constraint, CGMPConstraint );

CGMPConstraint::CGMPConstraint( void )
{
	m_pTable = NULL;
	m_pSpring = NULL;
	m_pMotor = NULL;
	m_pConstraint[0] = m_pConstraint[1] = NULL;
	m_pNoCollide[0] = m_pNoCollide[1] = NULL;
	for ( int i = 0; i < 4; i++ )
		m_bHasEnt[i] = false;
	m_vecMotorAxis.Init();
	m_iMotorDir = 1;
	m_bMotorOn = false;
	m_flMotorOff = 0;
	m_flMotorScale = 0;
	m_bController = false;
	m_iCtrlType = 0;
	m_iDir = 0;
	m_flLength = m_flMin = m_flMax = m_flCTime = m_flLast = 0;
	m_bExpanded = false;
}

int CGMPConstraint::Bone( int i ) const
{
	// Keepupright stores "Bone"; the others Bone1..Bone4.
	return m_pTable->GetInt( UTIL_VarArgs( "Bone%d", i ), m_pTable->GetInt( "Bone", 0 ) );
}

// GMod: the world's LPos is a world position; other objects' are in the physics object's space.
Vector CGMPConstraint::WPos( int i ) const
{
	const Vector lpos = LPos( i );
	CBaseEntity *pEnt = GetEnt( i );
	IPhysicsObject *pPhys = Phys( i );
	if ( !pEnt || pEnt->IsWorld() || !pPhys )
		return lpos;
	Vector world;
	pPhys->LocalToWorld( &world, lpos );
	return world;
}

bool CGMPConstraint::Uses( CBaseEntity *pEnt ) const
{
	for ( int i = 0; pEnt && i < 4; i++ )
		if ( m_bHasEnt[i] && m_hEnt[i].Get() == pEnt )
			return true;
	return false;
}

void CGMPConstraint::SetBreak( constraint_breakableparams_t &params, float forcelimit, float torquelimit ) const
{
	params.Defaults();
	params.strength = 1.0f;
	params.forceLimit = lbs2kg( forcelimit );
	params.torqueLimit = lbs2kg( torquelimit );
}

void CGMPConstraint::Keep( IPhysicsConstraint *pConstraint )
{
	if ( !pConstraint )
		return;
	pConstraint->SetGameData( (void *)static_cast<CBaseEntity *>( this ) );
	m_pConstraint[m_pConstraint[0] ? 1 : 0] = pConstraint;
}

void CGMPConstraint::NoCollide( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2 )
{
	if ( m_pNoCollide[0] || !pPhys1 || !pPhys2 || pPhys1 == g_PhysWorldObject || pPhys2 == g_PhysWorldObject )
		return;
	PhysDisableEntityCollisions( pPhys1, pPhys2 );
	m_pNoCollide[0] = pPhys1;
	m_pNoCollide[1] = pPhys2;
}

// phys_hinge at wpos1 turning around wpos1 -> wpos2.
IPhysicsConstraint *CGMPConstraint::CreateHinge( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, const Vector &wpos1, const Vector &wpos2, float forcelimit, float torquelimit, float friction )
{
	constraint_hingeparams_t hinge;
	hinge.Defaults();
	hinge.worldPosition = wpos1;
	hinge.worldAxisDirection = wpos2 - wpos1;
	VectorNormalize( hinge.worldAxisDirection );
	UTIL_SnapDirectionToAxis( hinge.worldAxisDirection );
	hinge.hingeAxis.SetAxisFriction( 0, 0, friction * 1000.0f );
	SetBreak( hinge.constraint, forcelimit, torquelimit );
	return physenv->CreateHingeConstraint( pPhys1, pPhys2, NULL, hinge );
}

IPhysicsConstraint *CGMPConstraint::CreateSlider( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, const Vector &wpos1, const Vector &wpos2 )
{
	Vector axis = wpos2 - wpos1;
	VectorNormalize( axis );
	UTIL_SnapDirectionToAxis( axis );
	constraint_slidingparams_t sliding;
	sliding.Defaults();
	sliding.InitWithCurrentObjectState( pPhys1, pPhys2, axis );
	sliding.limitMin = sliding.limitMax = 0;	// no limit
	sliding.friction = 0;
	sliding.constraint.strength = 1.0f;
	return physenv->CreateSlidingConstraint( pPhys1, pPhys2, NULL, sliding );
}

bool CGMPConstraint::CreateSpring( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, float constant, float damping, float rdamping, bool stretchOnly )
{
	springparams_t spring;
	memset( &spring, 0, sizeof( spring ) );
	spring.constant = constant;
	spring.damping = damping;
	spring.relativeDamping = rdamping;
	spring.startPosition = WPos( 1 );
	spring.endPosition = WPos( 2 );
	spring.naturalLength = ( spring.endPosition - spring.startPosition ).Length();
	spring.useLocalPositions = false;
	spring.onlyStretch = stretchOnly;
	m_pSpring = physenv->CreateSpring( pPhys1, pPhys2, &spring );
	return m_pSpring != NULL;
}

// constraint.CalcElasticConsts
void CGMPConstraint::ElasticConsts( IPhysicsObject *pPhys1, IPhysicsObject *pPhys2, bool fixed, float &constant, float &damping ) const
{
	float minMass;
	if ( GetEnt( 1 ) && GetEnt( 1 )->IsWorld() )
		minMass = pPhys2->GetMass();
	else if ( GetEnt( 2 ) && GetEnt( 2 )->IsWorld() )
		minMass = pPhys1->GetMass();
	else
		minMass = MIN( pPhys1->GetMass(), pPhys2->GetMass() );
	constant = minMass * ( fixed ? 100.0f : 50.0f );
	damping = constant * ( fixed ? 0.2f : 0.1f );
}

void CGMPConstraint::AddRope( int i1, const Vector &w1, int i2, const Vector &w2, float width, const char *material, bool resize, float length )
{
	if ( width <= 0 )
		return;
	int slot = 0;
	while ( slot < 3 && m_hRope[slot].Get() )
		slot++;
	if ( slot >= 3 )
		return;
	// i == 0: a fixed world point (pulley wheels).
	CBaseEntity *pA0 = GMP_CreateRopeAnchor( i1 ? GetEnt( i1 ) : NULL, i1 ? Bone( i1 ) : 0, w1 );
	CBaseEntity *pA1 = GMP_CreateRopeAnchor( i2 ? GetEnt( i2 ) : NULL, i2 ? Bone( i2 ) : 0, w2 );
	m_hAnchor[slot * 2] = pA0;
	m_hAnchor[slot * 2 + 1] = pA1;
	m_hRope[slot] = GMP_CreateRope( pA0, pA1, width, material, resize, length );
}

bool CGMPConstraint::Build( KeyValues *pTable, CBaseEntity *const ents[4], CBasePlayer *pPlayer )
{
	if ( !physenv || !pTable )
		return false;
	m_pTable = pTable->MakeCopy();
	m_pTable->SetName( "Constraint" );
	for ( int i = 0; i < 4; i++ )
	{
		m_hEnt[i] = ents[i];
		m_bHasEnt[i] = ents[i] != NULL;
	}
	const char *type = S( "Type" );
	IPhysicsObject *pPhys1 = Phys( 1 ), *pPhys2 = Phys( 2 );
	const bool pair = pPhys1 && pPhys2 && pPhys1 != pPhys2;
	bool ok = false;

	if ( !Q_stricmp( type, "Weld" ) )
	{
		if ( pair )
		{
			constraint_fixedparams_t fixed;
			fixed.Defaults();
			fixed.InitWithCurrentObjectState( pPhys2, pPhys1 );
			SetBreak( fixed.constraint, F( "forcelimit" ), 0 );
			Keep( physenv->CreateFixedConstraint( pPhys2, pPhys1, NULL, fixed ) );
			if ( B( "nocollide" ) )
				NoCollide( pPhys1, pPhys2 );
			ok = m_pConstraint[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Rope" ) )
	{
		if ( pair )
		{
			const Vector w1 = WPos( 1 ), w2 = WPos( 2 );
			const float total = F( "length" ) + F( "addlength" );
			constraint_lengthparams_t length;
			length.Defaults();
			length.objectPosition[0] = LPos( 1 );
			length.objectPosition[1] = LPos( 2 );
			length.totalLength = MAX( total, 1.0f );
			length.minLength = B( "rigid" ) ? length.totalLength : 0;
			SetBreak( length.constraint, F( "forcelimit" ), 0 );
			Keep( physenv->CreateLengthConstraint( pPhys1, pPhys2, NULL, length ) );
			AddRope( 1, w1, 2, w2, F( "width" ), S( "material" ), false, total );
			ok = m_pConstraint[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Elastic" ) )
	{
		if ( pair && CreateSpring( pPhys1, pPhys2, F( "constant" ), F( "damping" ), F( "rdamping" ), B( "stretchonly" ) ) )
		{
			AddRope( 1, WPos( 1 ), 2, WPos( 2 ), F( "width" ), S( "material" ), true, 0 );
			ok = true;
		}
	}
	else if ( !Q_stricmp( type, "Keepupright" ) )
	{
		CBaseEntity *pEnt = GetEnt( 1 );
		if ( pEnt && !pEnt->IsWorld() && pPhys1 )
		{
			const QAngle ang = GMP_ParseAngle( S( "Ang" ) );
			m_hExtra = CreateKeepUpright( pEnt->GetAbsOrigin(), ang, pEnt, F( "angularlimit", 999999 ), true );
			ok = m_hExtra.Get() != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Slider" ) )
	{
		if ( pair )
		{
			const Vector w1 = WPos( 1 ), w2 = WPos( 2 );
			Keep( CreateSlider( pPhys1, pPhys2, w1, w2 ) );
			AddRope( 1, w1, 2, w2, F( "width" ), S( "material" ), true, 0 );
			ok = m_pConstraint[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Axis" ) || !Q_stricmp( type, "Motor" ) )
	{
		if ( pair )
		{
			const Vector w1 = WPos( 1 );
			Vector w2 = WPos( 2 );
			const char *localAxis = S( "LocalAxis" );
			if ( *localAxis )
				pPhys1->LocalToWorld( &w2, GMP_ParseVector( localAxis ) );
			else
			{
				Vector local;
				pPhys1->WorldToLocal( &local, w2 );
				GMP_SetVector( m_pTable, "LocalAxis", local );
			}
			const bool motor = !Q_stricmp( type, "Motor" );
			Keep( CreateHinge( pPhys1, pPhys2, w1, w2, F( "forcelimit" ), motor ? 0 : F( "torquelimit" ), F( "friction" ) ) );
			if ( B( "nocollide" ) )
				NoCollide( pPhys1, pPhys2 );
			ok = m_pConstraint[0] != NULL;
			if ( ok && motor )
			{
				// phys_torque on Phys1 around the hinge axis, kept in Phys1's space.
				Vector axis = w2 - w1;
				VectorNormalize( axis );
				UTIL_SnapDirectionToAxis( axis );
				matrix3x4_t matrix;
				pPhys1->GetPositionMatrix( &matrix );
				VectorIRotate( axis, matrix, m_vecMotorAxis );
				m_iMotorDir = F( "direction", 1 ) < 0 ? -1 : 1;
				m_pMotor = physenv->CreateMotionController( this );
				if ( m_pMotor )
					m_pMotor->AttachObject( pPhys1, true );
				GMP_NumpadBind( pPlayer, (int)F( "numpadkey_fwd" ), this );
				GMP_NumpadBind( pPlayer, (int)F( "numpadkey_bwd" ), this );
			}
		}
	}
	else if ( !Q_stricmp( type, "AdvBallsocket" ) )
	{
		if ( pair )
		{
			constraint_ragdollparams_t ragdoll;
			ragdoll.Defaults();
			matrix3x4_t constraintToWorld, objToWorld, worldToObj;
			SetIdentityMatrix( constraintToWorld );
			MatrixSetColumn( WPos( 1 ), 3, constraintToWorld );
			pPhys1->GetPositionMatrix( &objToWorld );
			MatrixInvert( objToWorld, worldToObj );
			ConcatTransforms( worldToObj, constraintToWorld, ragdoll.constraintToReference );
			pPhys2->GetPositionMatrix( &objToWorld );
			MatrixInvert( objToWorld, worldToObj );
			ConcatTransforms( worldToObj, constraintToWorld, ragdoll.constraintToAttached );
			ragdoll.onlyAngularLimits = B( "onlyrotation" );
			ragdoll.axes[0].SetAxisFriction( F( "xmin" ), F( "xmax" ), F( "xfric" ) );
			ragdoll.axes[1].SetAxisFriction( F( "ymin" ), F( "ymax" ), F( "yfric" ) );
			ragdoll.axes[2].SetAxisFriction( F( "zmin" ), F( "zmax" ), F( "zfric" ) );
			SetBreak( ragdoll.constraint, F( "forcelimit" ), F( "torquelimit" ) );
			Keep( physenv->CreateRagdollConstraint( pPhys1, pPhys2, NULL, ragdoll ) );
			if ( B( "nocollide" ) )
				NoCollide( pPhys1, pPhys2 );
			ok = m_pConstraint[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Ballsocket" ) )
	{
		if ( pair )
		{
			Vector wpos = GMP_ParseVector( S( "LPos" ) );
			if ( GetEnt( 2 ) && !GetEnt( 2 )->IsWorld() )
				pPhys2->LocalToWorld( &wpos, GMP_ParseVector( S( "LPos" ) ) );
			constraint_ballsocketparams_t ball;
			ball.Defaults();
			ball.InitWithCurrentObjectState( pPhys1, pPhys2, wpos );
			SetBreak( ball.constraint, F( "forcelimit" ), F( "torquelimit" ) );
			Keep( physenv->CreateBallsocketConstraint( pPhys1, pPhys2, NULL, ball ) );
			if ( B( "nocollide" ) )
				NoCollide( pPhys1, pPhys2 );
			ok = m_pConstraint[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "NoCollide" ) )
	{
		if ( pair )
		{
			NoCollide( pPhys1, pPhys2 );
			ok = m_pNoCollide[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Pulley" ) )
	{
		IPhysicsObject *pPhys4 = Phys( 4 );
		if ( pPhys1 && pPhys4 && pPhys1 != pPhys4 )
		{
			const Vector w1 = WPos( 1 ), w4 = WPos( 4 );
			const Vector p0 = GMP_ParseVector( S( "WPos2" ) ), p1 = GMP_ParseVector( S( "WPos3" ) );
			constraint_pulleyparams_t pulley;
			pulley.Defaults();
			pulley.pulleyPosition[0] = p0;
			pulley.pulleyPosition[1] = p1;
			pulley.objectPosition[0] = LPos( 1 );
			pulley.objectPosition[1] = LPos( 4 );
			pulley.gearRatio = 1.0f;
			// phys_pulley: addlength + both sides; GMod's addlength is |WPos3 - WPos4|.
			pulley.totalLength = ( p1 - w4 ).Length() + ( w1 - p0 ).Length() + ( w4 - p1 ).Length() * pulley.gearRatio;
			pulley.isRigid = B( "rigid" );
			SetBreak( pulley.constraint, F( "forcelimit" ), 0 );
			Keep( physenv->CreatePulleyConstraint( pPhys1, pPhys4, NULL, pulley ) );
			const float width = F( "width" );
			AddRope( 1, w1, 0, p0, width, S( "material" ), true, 0 );
			AddRope( 0, p0, 0, p1, width, S( "material" ), true, 0 );
			AddRope( 0, p1, 4, w4, width, S( "material" ), true, 0 );
			ok = m_pConstraint[0] != NULL;
		}
	}
	else if ( !Q_stricmp( type, "Winch" ) || !Q_stricmp( type, "Hydraulic" ) || !Q_stricmp( type, "Muscle" ) )
	{
		if ( pair )
		{
			const bool winch = !Q_stricmp( type, "Winch" ), muscle = !Q_stricmp( type, "Muscle" );
			const bool fixed = !winch && F( "fixed" ) == 1;
			float constant, damping;
			ElasticConsts( pPhys1, pPhys2, false, constant, damping );
			if ( CreateSpring( pPhys1, pPhys2, constant, damping, 0, winch ) )
			{
				const Vector w1 = WPos( 1 ), w2 = WPos( 2 );
				AddRope( 1, w1, 2, w2, F( "width" ), S( "material" ), true, 0 );
				if ( fixed )
					Keep( CreateSlider( pPhys1, pPhys2, w1, w2 ) );
				if ( winch )
				{
					StartController( 0, 1, FLT_MAX );
					GMP_NumpadBind( pPlayer, (int)F( "fwd_bind" ), this );
					GMP_NumpadBind( pPlayer, (int)F( "bwd_bind" ), this );
				}
				else
				{
					const float l1 = F( "Length1" ), l2 = F( "Length2" );
					StartController( muscle ? 1 : 0, MIN( l1, l2 ), MAX( l1, l2 ) );
					GMP_NumpadBind( pPlayer, (int)F( "key" ), this );
					if ( muscle && B( "starton" ) )
						m_iDir = 1;
				}
				ok = true;
			}
		}
	}
	else
		Warning( "GModPort: unknown constraint type '%s'\n", type );

	if ( !ok )
		return false;
	if ( pPhys1 )
		pPhys1->Wake();
	if ( pPhys2 )
		pPhys2->Wake();
	SetThink( &CGMPConstraint::WatchThink );
	SetNextThink( gpGlobals->curtime + ( m_bController ? 0.01f : 0.5f ) );
	return true;
}

void CGMPConstraint::StartController( int type, float minLen, float maxLen )
{
	m_bController = true;
	m_iCtrlType = type;
	m_iDir = 0;
	m_flMin = minLen;
	m_flMax = maxLen;
	m_flLast = gpGlobals->curtime;
	m_flLength = ( WPos( 1 ) - WPos( 2 ) ).Length();
	m_bExpanded = m_flLength >= m_flMax;
	m_flCTime = 0;
	if ( type == 1 )
	{
		const float amp = F( "amplitude" ), per = F( "period" );
		if ( amp != 0 && per != 0 )
			m_flCTime = asinf( clamp( ( m_flLength - m_flMin ) / ( amp * 2 ) - 1, -1.0f, 1.0f ) ) / ( per * M_PI_F );
	}
}

// gmod_winch_controller ENT:Think
void CGMPConstraint::ControllerThink( void )
{
	const float dt = gpGlobals->curtime - m_flLast;
	m_flLast = gpGlobals->curtime;
	if ( m_iDir == 0 )
		return;
	float length = m_flLength;
	if ( m_iCtrlType == 0 )
	{
		const float fwd = F( "fwd_speed", F( "speed", 64 ) ), bwd = F( "bwd_speed", fwd );
		const float dist = m_iDir > 0 ? fwd * dt : -bwd * dt;
		if ( dist == 0 )
			return;
		const bool toggle = m_pTable->GetInt( "toggle", 1 ) != 0;
		length += dist;
		if ( length < m_flMin )
		{
			length = m_flMin;
			if ( toggle )
				m_iDir = 0;
		}
		if ( m_flMax < FLT_MAX )
		{
			m_bExpanded = length > m_flMax;
			if ( m_bExpanded )
			{
				length = m_flMax;
				if ( toggle )
					m_iDir = 0;
			}
		}
	}
	else
	{
		const float amp = F( "amplitude" ), per = F( "period" );
		if ( per == 0 )
			return;
		const float spos = clamp( ( sinf( m_flCTime * M_PI_F * per ) + 1 ) * ( amp / 2 ), 0.0f, amp );
		length = m_flMin + spos;
		m_flCTime += dt;
	}
	m_flLength = length;
	if ( m_pSpring )
		m_pSpring->SetSpringLength( length );
}

void CGMPConstraint::WatchThink( void )
{
	for ( int i = 0; i < 4; i++ )
	{
		if ( m_bHasEnt[i] && !m_hEnt[i].Get() )
		{
			UTIL_Remove( this );
			return;
		}
	}
	if ( m_bController )
	{
		ControllerThink();
		SetNextThink( gpGlobals->curtime + 0.01f );
	}
	else
		SetNextThink( gpGlobals->curtime + 0.5f );
}

IMotionEvent::simresult_e CGMPConstraint::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear.Init();
	angular.Init();
	if ( m_flMotorScale != 0 && m_flMotorOff > 0 && gpGlobals->curtime >= m_flMotorOff )
	{
		m_bMotorOn = false;
		m_flMotorScale = 0;
	}
	if ( m_flMotorScale == 0 )
		return SIM_NOTHING;
	angular = m_vecMotorAxis * ( F( "torque" ) * m_flMotorScale );
	return SIM_LOCAL_ACCELERATION;
}

void CGMPConstraint::NumpadKey( CBasePlayer *pPlayer, int key, bool down )
{
	if ( m_pMotor )
	{
		// MotorControl
		const int dir = ( key == (int)F( "numpadkey_fwd" ) ) ? 1 : -1;
		const bool toggle = B( "toggle" );
		if ( toggle )
		{
			if ( !down )
				return;
			if ( m_bMotorOn && m_iMotorDir != dir )
				m_iMotorDir = dir;
			else
			{
				m_iMotorDir = dir;
				m_bMotorOn = !m_bMotorOn;
			}
		}
		else if ( down )
		{
			m_iMotorDir = dir;
			m_bMotorOn = true;
		}
		else if ( m_iMotorDir == dir )
			m_bMotorOn = false;
		m_flMotorScale = m_bMotorOn ? (float)m_iMotorDir : 0.0f;
		if ( m_bMotorOn )
		{
			const float forcetime = F( "forcetime" );
			m_flMotorOff = forcetime > 0 ? gpGlobals->curtime + forcetime : 0;
			m_pMotor->WakeObjects();
		}
		return;
	}
	if ( !m_bController )
		return;
	if ( IsType( "Winch" ) )
	{
		const int dir = ( key == (int)F( "fwd_bind" ) ) ? 1 : -1;
		if ( B( "toggle" ) )
		{
			if ( down )
				m_iDir = ( m_iDir == dir ) ? 0 : dir;	// WinchToggle
		}
		else if ( down )
			m_iDir = dir;								// WinchOn
		else
			m_iDir = 0;									// WinchOff
	}
	else if ( IsType( "Hydraulic" ) )
	{
		if ( m_pTable->GetInt( "toggle", 1 ) )
		{
			if ( down )	// HydraulicToggle
				m_iDir = ( m_iDir == 0 ) ? ( m_bExpanded ? -1 : 1 ) : -m_iDir;
		}
		else
			m_iDir = down ? 1 : -1;						// HydraulicDir
	}
	else if ( down )
		m_iDir = m_iDir ? 0 : 1;						// MuscleToggle
	m_flLast = gpGlobals->curtime;
	if ( Phys( 1 ) )
		Phys( 1 )->Wake();
	if ( Phys( 2 ) )
		Phys( 2 )->Wake();
}

void CGMPConstraint::UpdateOnRemove( void )
{
	GMP_NumpadUnbind( this );
	// vphysics invalidates constraints and springs whose object was deleted; destroying them after that is safe.
	for ( int i = 0; i < 2; i++ )
	{
		if ( m_pConstraint[i] && physenv )
			physenv->DestroyConstraint( m_pConstraint[i] );
		m_pConstraint[i] = NULL;
	}
	if ( m_pSpring && physenv )
		physenv->DestroySpring( m_pSpring );
	m_pSpring = NULL;
	if ( m_pMotor && physenv )
		physenv->DestroyMotionController( m_pMotor );
	m_pMotor = NULL;
	if ( m_pNoCollide[0] )
	{
		// Only while both objects still exist: re-read them from the entities.
		for ( int i = 1; i <= 2; i++ )
			if ( m_bHasEnt[i - 1] && !GetEnt( i ) )
				m_pNoCollide[0] = NULL;
		IPhysicsObject *pPhys1 = Phys( 1 ), *pPhys2 = Phys( 2 );
		if ( m_pNoCollide[0] && pPhys1 && pPhys2 )
		{
			PhysEnableEntityCollisions( pPhys1, pPhys2 );
			pPhys1->Wake();
			pPhys2->Wake();
		}
		m_pNoCollide[0] = m_pNoCollide[1] = NULL;
	}
	for ( int i = 0; i < 3; i++ )
		UTIL_Remove( m_hRope[i].Get() );
	for ( int i = 0; i < 6; i++ )
		UTIL_Remove( m_hAnchor[i].Get() );
	UTIL_Remove( m_hExtra.Get() );
	if ( m_pTable )
		m_pTable->deleteThis();
	m_pTable = NULL;
	BaseClass::UpdateOnRemove();
}

// Builds a GMod constraint. pTable: Type + arguments (Bone1, LPos1, forcelimit...); ents: Ent1..Ent4.
static CGMPConstraint *GMP_CreateConstraint( KeyValues *pTable, CBaseEntity *const ents[4], CBasePlayer *pPlayer )
{
	CGMPConstraint *pC = static_cast<CGMPConstraint *>( CreateEntityByName( "gmp_constraint" ) );
	if ( !pC )
		return NULL;
	DispatchSpawn( pC );
	if ( !pC->Build( pTable, ents, pPlayer ) )
	{
		UTIL_Remove( pC );
		return NULL;
	}
	return pC;
}

// All live constraints on pEnt (of 'type', or any).
static void GMP_FindConstraints( CBaseEntity *pEnt, const char *type, CUtlVector<CGMPConstraint *> &out )
{
	for ( CBaseEntity *p = gEntList.FindEntityByClassname( NULL, "gmp_constraint" ); p; p = gEntList.FindEntityByClassname( p, "gmp_constraint" ) )
	{
		CGMPConstraint *pC = static_cast<CGMPConstraint *>( p );
		if ( ( p->GetEFlags() & EFL_KILLME ) || !pC->GetTable() || ( pEnt && !pC->Uses( pEnt ) ) || ( type && !pC->IsType( type ) ) )
			continue;
		out.AddToTail( pC );
	}
}

static int GMP_RemoveConstraints( CBaseEntity *pEnt, const char *type )
{
	CUtlVector<CGMPConstraint *> list;
	GMP_FindConstraints( pEnt, type, list );
	for ( int i = 0; i < list.Count(); i++ )
		UTIL_Remove( list[i] );
	return list.Count();
}

// Everything connected to pEnt through constraints (GetAllConstrainedEntitiesAndConstraints).
static void GMP_GetConstrained( CBaseEntity *pEnt, CUtlVector<EHANDLE> &ents, CUtlVector<CGMPConstraint *> &constraints )
{
	if ( !pEnt || pEnt->IsWorld() || ents.Find( EHANDLE( pEnt ) ) >= 0 )
		return;
	ents.AddToTail( pEnt );
	CUtlVector<CGMPConstraint *> list;
	GMP_FindConstraints( pEnt, NULL, list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		if ( constraints.Find( list[i] ) >= 0 )
			continue;
		constraints.AddToTail( list[i] );
		for ( int e = 1; e <= 4; e++ )
			GMP_GetConstrained( list[i]->GetEnt( e ), ents, constraints );
	}
}
