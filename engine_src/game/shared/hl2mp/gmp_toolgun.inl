//========= GModPort ==========================================================//
//
// Purpose: GMod's tool gun (gmod_tool) with the sandbox stools of gmp_tooldefs.inl.
//          Included from gmp_physgun.inl after gmp_constraints / gmp_sents / gmp_entmods / gmp_dupe.
//
//          gmod_toolmode (client, userinfo)  the stool (weld, rope, thruster...)
//          <tool>_<setting> (client, archived) GMod's TOOL.ClientConVar; the engine allows only 255
//          userinfo convars, so the client sends a tool's settings with gmp_tset when they change.
//          +attack / +attack2 / +reload  the stool's LeftClick / RightClick / Reload
//
//=============================================================================//

#ifdef CLIENT_DLL
#define CWeaponGMPToolGun C_WeaponGMPToolGun
#endif

#define GMP_TOOL_SHOOT_SOUND	"Airboat.FireGunRevDown"	// GMod's tool shot (hl2_game_sounds_vehicles.txt)

// Client only: on a listen server a shared declaration registers the server copy first, which
// is not USERINFO, so the server would never see the mode (it reads GetClientConVarValue).
#ifdef CLIENT_DLL
static ConVar gmod_toolmode( "gmod_toolmode", "weld", FCVAR_USERINFO | FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, "GModPort: tool gun mode (gmp_tooldefs.inl)" );
#endif

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Client: the settings convars and their sync to the server
//-----------------------------------------------------------------------------
static unsigned int s_nGMPToolSynced = 0;	// bit per tool: the server has its current settings
static EHANDLE s_hGMPToolSyncWeapon;		// a new tool gun (map change, respawn) sends everything again

static void GMP_ToolVarChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	const char *name = var->GetName();
	for ( int i = 0; i < ARRAYSIZE( s_GMPToolDefs ); i++ )
	{
		const int len = Q_strlen( s_GMPToolDefs[i].mode );
		if ( !Q_strnicmp( name, s_GMPToolDefs[i].mode, len ) && name[len] == '_' && GMP_ToolVarIndex( s_GMPToolDefs[i], name + len + 1 ) >= 0 )
			s_nGMPToolSynced &= ~( 1u << i );
	}
}

static struct GMPToolConVarsInit
{
	GMPToolConVarsInit()
	{
		COMPILE_TIME_ASSERT( ARRAYSIZE( s_GMPToolDefs ) <= 32 );
		for ( int i = 0; i < ARRAYSIZE( s_GMPToolDefs ); i++ )
		{
			for ( int v = 0; v < s_GMPToolDefs[i].numVars; v++ )
			{
				const GMPToolVar &var = s_GMPToolDefs[i].vars[v];
				char name[64];
				Q_snprintf( name, sizeof( name ), "%s_%s", s_GMPToolDefs[i].mode, var.name );
				// Registered by ConVar_Register at client init; they live as long as the dll.
				new ConVar( strdup( name ), var.def, FCVAR_ARCHIVE | FCVAR_SERVER_CAN_EXECUTE, var.label, GMP_ToolVarChanged );
			}
		}
	}
} s_GMPToolConVarsInit;

static void GMP_ToolSendSettings( int tool )
{
	const GMPToolDef &def = s_GMPToolDefs[tool];
	char cmd[512];
	int len = 0, args = 0;
	for ( int v = 0; v <= def.numVars; v++ )
	{
		char item[192] = "";
		if ( v < def.numVars )
		{
			char cvarName[96];
			Q_snprintf( cvarName, sizeof( cvarName ), "%s_%s", def.mode, def.vars[v].name );
			ConVarRef var( cvarName );
			char value[128];
			Q_strncpy( value, var.IsValid() ? var.GetString() : def.vars[v].def, sizeof( value ) );
			for ( char *p = value; *p; p++ )
				if ( *p == '"' || *p == ';' || *p == '\n' || *p == '\r' )
					*p = ' ';
			Q_snprintf( item, sizeof( item ), " %s \"%s\"", def.vars[v].name, value );
		}
		// Flush before the line or the argument list gets too long (CCommand: 512 chars, 64 args).
		if ( args && ( v == def.numVars || len + Q_strlen( item ) + 2 >= (int)sizeof( cmd ) || args >= 28 ) )
		{
			Q_strncat( cmd, "\n", sizeof( cmd ) );
			engine->ServerCmd( cmd );
			args = 0;
		}
		if ( v == def.numVars )
			break;
		if ( !args )
			len = Q_snprintf( cmd, sizeof( cmd ), "gmp_tset %s", def.mode );
		Q_strncat( cmd, item, sizeof( cmd ) );
		len = Q_strlen( cmd );
		args++;
	}
}

// Called every frame while the local player holds the tool gun.
static void GMP_ToolSync( C_BaseEntity *pWeapon, int tool )
{
	if ( s_hGMPToolSyncWeapon.Get() != pWeapon )
	{
		s_hGMPToolSyncWeapon = pWeapon;
		s_nGMPToolSynced = 0;
	}
	if ( tool < 0 || ( s_nGMPToolSynced & ( 1u << tool ) ) )
		return;
	s_nGMPToolSynced |= 1u << tool;
	if ( s_GMPToolDefs[tool].numVars )
		GMP_ToolSendSettings( tool );
}
#else
//-----------------------------------------------------------------------------
// Server: each player's tool settings ("u<userid>" > tool > setting)
//-----------------------------------------------------------------------------
static KeyValues *s_pGMPToolSettings = NULL;

CON_COMMAND( gmp_tset, "GModPort: gmp_tset <tool> <setting> <value> ... (sent by the client)" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	const int tool = args.ArgC() >= 2 ? GMP_ToolIndex( args[1] ) : -1;
	if ( !pPlayer || tool < 0 )
		return;
	if ( !s_pGMPToolSettings )
		s_pGMPToolSettings = new KeyValues( "GMPToolSettings" );
	char user[16];
	Q_snprintf( user, sizeof( user ), "u%d", pPlayer->GetUserID() );
	KeyValues *pTool = s_pGMPToolSettings->FindKey( user, true )->FindKey( s_GMPToolDefs[tool].mode, true );
	for ( int i = 2; i + 1 < args.ArgC(); i += 2 )
		if ( GMP_ToolVarIndex( s_GMPToolDefs[tool], args[i] ) >= 0 )
			pTool->SetString( args[i], args[i + 1] );
}

static const char *GMP_ToolSetting( CBasePlayer *pPlayer, int tool, const char *name )
{
	if ( tool < 0 )
		return "";
	const int v = GMP_ToolVarIndex( s_GMPToolDefs[tool], name );
	const char *def = v >= 0 ? s_GMPToolDefs[tool].vars[v].def : "";
	if ( !pPlayer || !s_pGMPToolSettings )
		return def;
	char user[16];
	Q_snprintf( user, sizeof( user ), "u%d", pPlayer->GetUserID() );
	KeyValues *pUser = s_pGMPToolSettings->FindKey( user );
	KeyValues *pTool = pUser ? pUser->FindKey( s_GMPToolDefs[tool].mode ) : NULL;
	return pTool ? pTool->GetString( name, def ) : def;
}

// The GMod constraint type each constraint tool makes (its Reload removes them).
static const char *GMP_ToolConstraintType( const char *mode )
{
	static const char *const s_Types[][2] = {
		{ "axis", "Axis" }, { "ballsocket", "Ballsocket" }, { "elastic", "Elastic" }, { "hydraulic", "Hydraulic" },
		{ "motor", "Motor" }, { "muscle", "Muscle" }, { "pulley", "Pulley" }, { "rope", "Rope" }, { "slider", "Slider" },
		{ "weld", "Weld" }, { "winch", "Winch" }, { "nocollide", "NoCollide" } };
	for ( int i = 0; i < ARRAYSIZE( s_Types ); i++ )
		if ( !Q_stricmp( mode, s_Types[i][0] ) )
			return s_Types[i][1];
	return NULL;
}

// Rotates a single-object entity so its local normal faces -targetNormal, then moves it so its
// local point lands on targetPos (the auto-aligning axis / motor / easy weld).
static bool GMP_ToolAlign( CBaseEntity *pEnt, const Vector &localPos, const Vector &localNormal, const Vector &targetPos, const Vector &targetNormal )
{
	IPhysicsObject *list[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	if ( !pEnt || pEnt->IsWorld() || pEnt->VPhysicsGetObjectList( list, ARRAYSIZE( list ) ) != 1 )
		return false;
	IPhysicsObject *pPhys = list[0];
	matrix3x4_t toWorld;
	pPhys->GetPositionMatrix( &toWorld );
	Vector n1;
	VectorRotate( localNormal, toWorld, n1 );
	const Vector want = -targetNormal;
	Vector axis = CrossProduct( n1, want );
	const float sinA = VectorNormalize( axis ), cosA = DotProduct( n1, want );
	if ( sinA < 1e-4f )
	{
		if ( cosA > 0 )
			axis.Init();	// already facing
		else
		{
			Vector up( 0, 0, 1 );
			if ( fabsf( n1.z ) > 0.9f )
				up.Init( 1, 0, 0 );
			axis = CrossProduct( n1, up );
			VectorNormalize( axis );
		}
	}
	matrix3x4_t newWorld = toWorld;
	if ( axis != vec3_origin )
	{
		matrix3x4_t rot;
		MatrixBuildRotationAboutAxis( axis, RAD2DEG( atan2f( sinA, cosA ) ), rot );
		ConcatTransforms( rot, toWorld, newWorld );
	}
	Vector hit;
	VectorTransform( localPos, newWorld, hit );
	Vector origin;
	MatrixGetColumn( newWorld, 3, origin );
	origin += targetPos - hit;
	MatrixSetColumn( origin, 3, newWorld );
	QAngle angles;
	MatrixAngles( newWorld, angles, origin );
	pEnt->Teleport( &origin, &angles, &vec3_origin );
	pPhys->SetVelocity( &vec3_origin, &vec3_origin );
	return true;
}

static const char *GMP_BalloonModel( const char *choice, int &skin )
{
	skin = 0;
	if ( !Q_strnicmp( choice, "normal_skin", 11 ) )
		skin = clamp( atoi( choice + 11 ), 0, 3 );
	if ( !Q_stricmp( choice, "gman" ) )
		return "models/maxofs2d/balloon_gman.mdl";
	if ( !Q_stricmp( choice, "mossman" ) )
		return "models/maxofs2d/balloon_mossman.mdl";
	if ( !Q_stricmp( choice, "dog" ) )
		return "models/balloons/balloon_dog.mdl";
	if ( !Q_stricmp( choice, "heart" ) )
		return "models/balloons/balloon_classicheart.mdl";
	if ( !Q_stricmp( choice, "star" ) )
		return "models/balloons/balloon_star.mdl";
	return "models/maxofs2d/balloon_classic.mdl";
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// gmod_tool
//-----------------------------------------------------------------------------
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
		m_bAlt = false;
		m_flNextAuto = 0;
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

	CNetworkVar( int, m_iStage );	// the number of objects picked so far

private:
	void	Fire( CBasePlayer *pOwner, int button );
#ifndef CLIENT_DLL
	struct Pick_t
	{
		EHANDLE	ent;
		int		bone;
		Vector	local, localNormal;	// in the physics object's space; world space for the world
	};

	void	SyncTool( CBasePlayer *pOwner );
	void	UseTool( CBasePlayer *pOwner, const trace_t &tr, int button );
	void	ClearStage( void ) { m_iStage = 0; m_bAlt = false; for ( int i = 0; i < 4; i++ ) m_Picks[i].ent = NULL; }
	bool	Is( const char *mode ) const { return m_iTool >= 0 && !Q_stricmp( s_GMPToolDefs[m_iTool].mode, mode ); }

	// Settings
	const char *S( const char *name ) const { return GMP_ToolSetting( ToBasePlayer( GetOwner() ), m_iTool, name ); }
	float	F( const char *name ) const { return atof( S( name ) ); }
	int		I( const char *name ) const { return atoi( S( name ) ); }

	// Picks
	bool	SetPick( int i, const trace_t &tr );
	IPhysicsObject *PickPhys( int i ) const { return GMP_ToolPhys( m_Picks[i].ent, m_Picks[i].bone ); }
	bool	PickWorld( int i ) const { return m_Picks[i].ent.Get() && m_Picks[i].ent->IsWorld(); }
	Vector	PickPos( int i ) const;
	Vector	PickNormal( int i ) const;
	Vector	PickLocal( int i, const Vector &world ) const;
	void	PutPick( KeyValues *pTable, int i, int n ) const;
	void	Message( const char *text ) const { ClientPrint( ToBasePlayer( GetOwner() ), HUD_PRINTCENTER, text ); }

	// Tools
	void	ConstraintTool( CBasePlayer *pOwner, const trace_t &tr, int button );
	void	FinishConstraint( CBasePlayer *pOwner );
	CGMPConstraint *MakeConstraint( CBasePlayer *pOwner, KeyValues *pTable, CBaseEntity *pEnt1, CBaseEntity *pEnt2, CBaseEntity *pEnt4 = NULL );
	void	SentTool( CBasePlayer *pOwner, const trace_t &tr, int button );
	KeyValues *SentSettings( const char *classname, bool create ) const;
	CGMPSent *SpawnSent( CBasePlayer *pOwner, const char *classname, KeyValues *pTable, const trace_t &tr, const QAngle &ang, bool nearest );
	CGMPConstraint *SentWeld( CBasePlayer *pOwner, CBaseEntity *pSent, const trace_t &tr, bool nocollide );
	void	OtherTool( CBasePlayer *pOwner, const trace_t &tr, int button );

	int		m_iTool;
	Pick_t	m_Picks[4];
	bool	m_bAlt;			// the right-click variant (easy weld, axis without aligning) is in progress
	float	m_flNextAuto;	// paint: held fire

	DECLARE_ACTTABLE();
#endif

	CWeaponGMPToolGun( const CWeaponGMPToolGun & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponGMPToolGun, DT_WeaponGMPToolGun )

BEGIN_NETWORK_TABLE( CWeaponGMPToolGun, DT_WeaponGMPToolGun )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iStage ) ),
#else
	SendPropInt( SENDINFO( m_iStage ), 3, SPROP_UNSIGNED ),
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
	// Paint: holding fire keeps painting.
	if ( Is( "paint" ) && ( pOwner->m_nButtons & IN_ATTACK ) && !( pOwner->m_afButtonPressed & IN_ATTACK ) && gpGlobals->curtime >= m_flNextAuto )
	{
		Fire( pOwner, IN_ATTACK );
		WeaponIdle();
		return;
	}
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
	if ( button != IN_RELOAD )
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
	m_flNextAuto = gpGlobals->curtime + 0.05f;
	Vector start = pOwner->Weapon_ShootPosition();
	Vector forward, right, up;
	pOwner->EyeVectors( &forward, &right, &up );
	trace_t tr;
	UTIL_TraceLine( start, start + forward * MAX_TRACE_LENGTH, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr );
	if ( button != IN_RELOAD && tr.fraction < 1.0f )
	{
		UTIL_Tracer( start + forward * 16 + right * 6 - up * 4, tr.endpos, 0, TRACER_DONT_USE_ATTACHMENT, 6000, false, "AR2Tracer" );
		g_pEffects->Sparks( tr.endpos, 1, 1, &tr.plane.normal );
	}
	DevMsg( 2, "GModPort tool: %s, button %d, hit %s\n", m_iTool >= 0 ? s_GMPToolDefs[m_iTool].mode : "none", button,
		tr.m_pEnt ? tr.m_pEnt->GetClassname() : "nothing" );
	UseTool( pOwner, tr, button );
#endif
}

#ifndef CLIENT_DLL
void CWeaponGMPToolGun::SyncTool( CBasePlayer *pOwner )
{
	int tool = GMP_ToolIndex( engine->GetClientConVarValue( pOwner->entindex(), "gmod_toolmode" ) );
	if ( tool < 0 )
		tool = GMP_ToolIndex( "weld" );
	if ( tool != m_iTool )
	{
		m_iTool = tool;
		ClearStage();
	}
}

bool CWeaponGMPToolGun::SetPick( int i, const trace_t &tr )
{
	CBaseEntity *pEnt = tr.m_pEnt;
	IPhysicsObject *pPhys = GMP_ToolPhys( pEnt, tr.physicsbone );
	if ( !pPhys || pEnt->IsPlayer() )
		return false;
	Pick_t &pick = m_Picks[i];
	pick.ent = pEnt;
	pick.bone = pEnt->IsWorld() ? 0 : tr.physicsbone;
	if ( pEnt->IsWorld() )
	{
		pick.local = tr.endpos;
		pick.localNormal = tr.plane.normal;
	}
	else
	{
		pPhys->WorldToLocal( &pick.local, tr.endpos );
		pPhys->WorldToLocalVector( &pick.localNormal, tr.plane.normal );
	}
	return true;
}

Vector CWeaponGMPToolGun::PickPos( int i ) const
{
	IPhysicsObject *pPhys = PickPhys( i );
	if ( PickWorld( i ) || !pPhys )
		return m_Picks[i].local;
	Vector world;
	pPhys->LocalToWorld( &world, m_Picks[i].local );
	return world;
}

Vector CWeaponGMPToolGun::PickNormal( int i ) const
{
	IPhysicsObject *pPhys = PickPhys( i );
	if ( PickWorld( i ) || !pPhys )
		return m_Picks[i].localNormal;
	Vector world;
	pPhys->LocalToWorldVector( &world, m_Picks[i].localNormal );
	return world;
}

// A world position in pick i's GMod LPos space.
Vector CWeaponGMPToolGun::PickLocal( int i, const Vector &world ) const
{
	IPhysicsObject *pPhys = PickPhys( i );
	if ( PickWorld( i ) || !pPhys )
		return world;
	Vector local;
	pPhys->WorldToLocal( &local, world );
	return local;
}

void CWeaponGMPToolGun::PutPick( KeyValues *pTable, int i, int n ) const
{
	char key[16];
	Q_snprintf( key, sizeof( key ), "Bone%d", n );
	pTable->SetInt( key, m_Picks[i].bone );
	Q_snprintf( key, sizeof( key ), "LPos%d", n );
	GMP_SetVector( pTable, key, m_Picks[i].local );
}

CGMPConstraint *CWeaponGMPToolGun::MakeConstraint( CBasePlayer *pOwner, KeyValues *pTable, CBaseEntity *pEnt1, CBaseEntity *pEnt2, CBaseEntity *pEnt4 )
{
	CBaseEntity *ents[4] = { pEnt1, pEnt2, NULL, pEnt4 };
	CGMPConstraint *pC = GMP_CreateConstraint( pTable, ents, pOwner );
	const char *type = pTable->GetString( "Type" );
	char name[32];
	Q_strncpy( name, type, sizeof( name ) );
	pTable->deleteThis();
	if ( !pC )
	{
		Message( "Could not constrain these objects" );
		return NULL;
	}
	GMP_AddUndo( pOwner, pC, name );
	Msg( "gmod_tool %s: %s -> %s\n", name, pEnt1 ? pEnt1->GetClassname() : "?", pEnt2 ? pEnt2->GetClassname() : ( pEnt4 ? pEnt4->GetClassname() : "?" ) );
	return pC;
}

void CWeaponGMPToolGun::UseTool( CBasePlayer *pOwner, const trace_t &tr, int button )
{
	if ( m_iTool < 0 )
		return;
	// Stale picks (the object was removed meanwhile) restart the tool.
	for ( int i = 0; i < m_iStage && i < 4; i++ )
		if ( !m_Picks[i].ent.Get() )
			ClearStage();

	if ( GMP_ToolConstraintType( s_GMPToolDefs[m_iTool].mode ) )
		ConstraintTool( pOwner, tr, button );
	else if ( Is( "balloon" ) || Is( "button" ) || Is( "dynamite" ) || Is( "emitter" ) || Is( "hoverball" ) || Is( "lamp" ) ||
		Is( "light" ) || Is( "thruster" ) || Is( "wheel" ) || Is( "camera" ) )
		SentTool( pOwner, tr, button );
	else
		OtherTool( pOwner, tr, button );
}

//-----------------------------------------------------------------------------
// Constraint tools
//-----------------------------------------------------------------------------
void CWeaponGMPToolGun::ConstraintTool( CBasePlayer *pOwner, const trace_t &tr, int button )
{
	const char *type = GMP_ToolConstraintType( s_GMPToolDefs[m_iTool].mode );
	CBaseEntity *pEnt = tr.fraction < 1.0f ? tr.m_pEnt : NULL;

	if ( button == IN_RELOAD )
	{
		const int removed = ( pEnt && !pEnt->IsWorld() ) ? GMP_RemoveConstraints( pEnt, type ) : 0;
		if ( !m_iStage && removed )
			Message( UTIL_VarArgs( "Removed %d %s constraint%s", removed, type, removed == 1 ? "" : "s" ) );
		ClearStage();
		return;
	}
	if ( !pEnt )
		return;

	if ( button == IN_ATTACK2 )
	{
		if ( Is( "nocollide" ) && m_iStage == 0 )
		{
			// No collisions with anything but the world.
			if ( pEnt->IsWorld() || pEnt->IsPlayer() )
				return;
			const bool off = pEnt->GetCollisionGroup() == COLLISION_GROUP_DEBRIS;
			pEnt->SetCollisionGroup( off ? COLLISION_GROUP_NONE : COLLISION_GROUP_DEBRIS );
			Message( off ? "Collisions restored" : "Collides with the world only" );
			return;
		}
		if ( ( Is( "hydraulic" ) || Is( "slider" ) || Is( "muscle" ) || Is( "winch" ) ) && m_iStage == 0 )
		{
			// The second point: straight out of the surface, up to whatever it meets.
			if ( !SetPick( 0, tr ) )
				return;
			trace_t tr2;
			UTIL_TraceLine( tr.endpos, tr.endpos + tr.plane.normal * 16384, MASK_SOLID, pEnt->IsWorld() ? NULL : pEnt, COLLISION_GROUP_NONE, &tr2 );
			if ( tr2.fraction >= 1.0f || !tr2.m_pEnt || !SetPick( 1, tr2 ) || PickPhys( 0 ) == PickPhys( 1 ) )
			{
				Message( "Nothing to attach to" );
				ClearStage();
				return;
			}
			FinishConstraint( pOwner );
			ClearStage();
			return;
		}
		if ( ( Is( "weld" ) || Is( "axis" ) ) && m_iStage == 0 )
		{
			// Easy weld / axis without aligning: pick the first object, finish with either button.
			if ( pEnt->IsWorld() || !SetPick( 0, tr ) )
				return;
			m_bAlt = true;
			m_iStage = 1;
			return;
		}
		if ( !( Is( "rope" ) || Is( "weld" ) || Is( "axis" ) ) || m_iStage == 0 )
		{
			if ( m_iStage )
				ClearStage();	// cancel
			return;
		}
		// Rope (new segment), easy weld and axis go on like a left click.
	}

	const bool worldFirst = Is( "rope" ) || Is( "elastic" ) || Is( "slider" ) || Is( "hydraulic" ) || Is( "muscle" ) || Is( "winch" );
	if ( Is( "pulley" ) && ( m_iStage == 1 || m_iStage == 2 ) )
	{
		// The two wheel positions: any surface.
		m_Picks[m_iStage].ent = GetWorldEntity();
		m_Picks[m_iStage].bone = 0;
		m_Picks[m_iStage].local = tr.endpos;
		m_Picks[m_iStage].localNormal = tr.plane.normal;
		m_iStage++;
		return;
	}
	const int slot = Is( "pulley" ) && m_iStage == 3 ? 3 : m_iStage;
	if ( pEnt->IsPlayer() || ( pEnt->IsWorld() && ( ( m_iStage == 0 && !worldFirst ) || Is( "nocollide" ) || Is( "pulley" ) ) ) )
	{
		Message( m_iStage == 0 ? "Pick a physics object" : "Pick another physics object" );
		return;
	}
	if ( !SetPick( slot, tr ) )
	{
		Message( "That has no physics" );
		return;
	}
	if ( m_iStage == 0 )
	{
		m_iStage = 1;
		return;
	}
	const int first = 0;
	if ( PickPhys( first ) == PickPhys( slot ) || ( PickWorld( first ) && PickWorld( slot ) ) )
	{
		Message( "Pick another object" );
		return;
	}
	if ( Is( "pulley" ) )
	{
		FinishConstraint( pOwner );
		ClearStage();
		return;
	}
	FinishConstraint( pOwner );
	if ( Is( "rope" ) && button == IN_ATTACK2 )
	{
		// A new segment from this point.
		m_Picks[0] = m_Picks[1];
		m_iStage = 1;
		return;
	}
	ClearStage();
}

void CWeaponGMPToolGun::FinishConstraint( CBasePlayer *pOwner )
{
	CBaseEntity *pEnt1 = m_Picks[0].ent, *pEnt2 = m_Picks[1].ent;
	KeyValues *t = new KeyValues( "Constraint" );
	t->SetString( "Type", GMP_ToolConstraintType( s_GMPToolDefs[m_iTool].mode ) );
	const float dist = ( PickPos( 0 ) - PickPos( 1 ) ).Length();

	if ( Is( "weld" ) )
	{
		if ( m_bAlt )
			GMP_ToolAlign( pEnt1, m_Picks[0].local, m_Picks[0].localNormal, PickPos( 1 ), PickNormal( 1 ) );
		PutPick( t, 0, 1 );
		PutPick( t, 1, 2 );
		t->SetFloat( "forcelimit", F( "forcelimit" ) );
		t->SetInt( "nocollide", I( "nocollide" ) );
	}
	else if ( Is( "axis" ) || Is( "motor" ) )
	{
		const bool motor = Is( "motor" );
		const Vector hit2 = PickPos( 1 ), norm2 = PickNormal( 1 );
		if ( !m_bAlt )
			GMP_ToolAlign( pEnt1, m_Picks[0].local, m_Picks[0].localNormal, hit2 + norm2 * ( motor ? 0.2f : 0.0f ), norm2 );
		t->SetInt( "Bone1", m_Picks[0].bone );
		t->SetInt( "Bone2", m_Picks[1].bone );
		GMP_SetVector( t, "LPos1", PickLocal( 0, hit2 + norm2 * ( motor ? 64.0f : 1.0f ) ) );
		GMP_SetVector( t, "LPos2", m_Picks[1].local );
		t->SetFloat( "forcelimit", F( "forcelimit" ) );
		t->SetInt( "nocollide", I( "nocollide" ) );
		if ( motor )
		{
			t->SetFloat( "friction", F( "friction" ) );
			t->SetFloat( "torque", F( "torque" ) );
			t->SetFloat( "forcetime", F( "forcetime" ) );
			t->SetInt( "toggle", I( "toggle" ) );
			t->SetInt( "numpadkey_fwd", I( "fwd" ) );
			t->SetInt( "numpadkey_bwd", I( "bwd" ) );
			t->SetInt( "direction", 1 );
		}
		else
		{
			t->SetFloat( "torquelimit", F( "torquelimit" ) );
			t->SetFloat( "friction", F( "hingefriction" ) );
		}
	}
	else if ( Is( "ballsocket" ) )
	{
		t->SetInt( "Bone1", m_Picks[0].bone );
		t->SetInt( "Bone2", m_Picks[1].bone );
		GMP_SetVector( t, "LPos", m_Picks[1].local );
		t->SetFloat( "forcelimit", F( "forcelimit" ) );
		t->SetFloat( "torquelimit", F( "torquelimit" ) );
		t->SetInt( "nocollide", I( "nocollide" ) );
	}
	else if ( Is( "nocollide" ) )
	{
		t->SetInt( "Bone1", m_Picks[0].bone );
		t->SetInt( "Bone2", m_Picks[1].bone );
	}
	else if ( Is( "pulley" ) )
	{
		PutPick( t, 0, 1 );
		PutPick( t, 3, 4 );
		GMP_SetVector( t, "WPos2", m_Picks[1].local );
		GMP_SetVector( t, "WPos3", m_Picks[2].local );
		t->SetFloat( "forcelimit", F( "forcelimit" ) );
		t->SetInt( "rigid", I( "rigid" ) );
		t->SetFloat( "width", F( "width" ) );
		t->SetString( "material", S( "material" ) );
		MakeConstraint( pOwner, t, pEnt1, NULL, m_Picks[3].ent );
		return;
	}
	else
	{
		// Rope-like: two points.
		PutPick( t, 0, 1 );
		PutPick( t, 1, 2 );
		if ( Is( "rope" ) )
		{
			t->SetFloat( "length", dist );
			t->SetFloat( "addlength", F( "addlength" ) );
			t->SetInt( "rigid", I( "rigid" ) );
			t->SetFloat( "forcelimit", F( "forcelimit" ) );
		}
		else if ( Is( "elastic" ) )
		{
			t->SetFloat( "constant", F( "constant" ) );
			t->SetFloat( "damping", F( "damping" ) );
			t->SetFloat( "rdamping", F( "rdamping" ) );
			t->SetInt( "stretchonly", I( "stretch_only" ) );
		}
		else if ( Is( "winch" ) )
		{
			t->SetInt( "fwd_bind", I( "fwd_group" ) );
			t->SetInt( "bwd_bind", I( "bwd_group" ) );
			t->SetFloat( "fwd_speed", F( "fwd_speed" ) );
			t->SetFloat( "bwd_speed", F( "bwd_speed" ) );
		}
		else if ( Is( "hydraulic" ) || Is( "muscle" ) )
		{
			const float add = F( "addlength" );
			t->SetFloat( "Length1", dist );
			t->SetFloat( "Length2", dist + add );
			t->SetInt( "key", I( "group" ) );
			t->SetInt( "fixed", I( "fixed" ) );
			if ( Is( "hydraulic" ) )
			{
				t->SetFloat( "speed", F( "speed" ) );
				t->SetInt( "toggle", I( "toggle" ) );
			}
			else
			{
				t->SetFloat( "period", F( "period" ) );
				t->SetFloat( "amplitude", fabsf( add ) / 2 );
				t->SetInt( "starton", I( "starton" ) );
			}
		}
		const bool winch = Is( "winch" );
		t->SetFloat( "width", F( winch ? "rope_width" : "width" ) );
		t->SetString( "material", S( winch ? "rope_material" : "material" ) );
	}
	MakeConstraint( pOwner, t, pEnt1, pEnt2 );
}

//-----------------------------------------------------------------------------
// Entity tools (gmp_sents.inl)
//-----------------------------------------------------------------------------
// The SENT's duplicator fields from the tool settings; 'create' adds the ones only set on spawn.
KeyValues *CWeaponGMPToolGun::SentSettings( const char *classname, bool create ) const
{
	KeyValues *t = new KeyValues( "Entity" );
	switch ( GMP_SentKind( classname ) )
	{
	case GMP_SENT_THRUSTER:
		t->SetInt( "key", I( "keygroup" ) );
		t->SetInt( "key_bck", I( "keygroup_back" ) );
		t->SetFloat( "force", F( "force" ) );
		t->SetInt( "toggle", I( "toggle" ) );
		t->SetString( "effect", S( "effect" ) );
		t->SetInt( "damageable", I( "damageable" ) );
		t->SetString( "soundname", S( "soundname" ) );
		t->SetInt( "nocollide", I( "collision" ) );
		break;
	case GMP_SENT_WHEEL:
		t->SetInt( "key_f", I( "fwd" ) );
		t->SetInt( "key_r", I( "bck" ) );
		t->SetInt( "Toggle", I( "toggle" ) );
		t->SetFloat( "BaseTorque", F( "torque" ) );
		if ( create )
			t->SetInt( "Direction", 1 );
		break;
	case GMP_SENT_HOVERBALL:
		t->SetInt( "key_u", I( "keyup" ) );
		t->SetInt( "key_d", I( "keydn" ) );
		t->SetInt( "key_o", I( "keyon" ) );
		t->SetFloat( "speed", F( "speed" ) );
		t->SetFloat( "resistance", F( "resistance" ) );
		t->SetFloat( "strength", F( "strength" ) );
		break;
	case GMP_SENT_BALLOON:
		t->SetInt( "r", I( "r" ) );
		t->SetInt( "g", I( "g" ) );
		t->SetInt( "b", I( "b" ) );
		t->SetFloat( "force", F( "force" ) );
		break;
	case GMP_SENT_DYNAMITE:
		t->SetInt( "key", I( "group" ) );
		t->SetInt( "Damage", I( "damage" ) );
		t->SetFloat( "delay", F( "delay" ) );
		t->SetInt( "remove", I( "remove" ) );
		break;
	case GMP_SENT_LAMP:
		t->SetInt( "r", I( "r" ) );
		t->SetInt( "g", I( "g" ) );
		t->SetInt( "b", I( "b" ) );
		t->SetFloat( "fov", F( "fov" ) );
		t->SetFloat( "distance", F( "distance" ) );
		t->SetFloat( "brightness", F( "brightness" ) );
		t->SetString( "Texture", S( "texture" ) );
		t->SetInt( "KeyDown", I( "key" ) );
		t->SetInt( "Toggle", I( "toggle" ) );
		if ( create )
			t->SetInt( "on", 1 );
		break;
	case GMP_SENT_LIGHT:
		t->SetInt( "lightr", I( "r" ) );
		t->SetInt( "lightg", I( "g" ) );
		t->SetInt( "lightb", I( "b" ) );
		t->SetFloat( "Brightness", F( "brightness" ) );
		t->SetFloat( "Size", F( "size" ) );
		t->SetInt( "KeyDown", I( "key" ) );
		t->SetInt( "Toggle", I( "toggle" ) );
		if ( create )
			t->SetInt( "on", 1 );
		break;
	case GMP_SENT_EMITTER:
		t->SetInt( "key", I( "key" ) );
		t->SetFloat( "delay", F( "delay" ) );
		t->SetFloat( "scale", F( "scale" ) );
		t->SetInt( "toggle", I( "toggle" ) );
		t->SetInt( "starton", I( "starton" ) );
		t->SetString( "effect", S( "effect" ) );
		if ( create )
			t->SetInt( "on", I( "starton" ) );
		break;
	case GMP_SENT_BUTTON:
		t->SetInt( "key", I( "keygroup" ) );
		t->SetInt( "toggle", I( "toggle" ) );
		t->SetString( "description", S( "description" ) );
		break;
	case GMP_SENT_CAMERA:
		t->SetInt( "controlkey", I( "key" ) );
		t->SetInt( "toggle", I( "toggle" ) );
		t->SetInt( "locked", I( "locked" ) );
		break;
	}
	return t;
}

// Spawns at the hit point; 'nearest' pushes it out of the surface (GMod's NearestPoint offset).
CGMPSent *CWeaponGMPToolGun::SpawnSent( CBasePlayer *pOwner, const char *classname, KeyValues *pTable, const trace_t &tr, const QAngle &ang, bool nearest )
{
	CGMPSent *pSent = GMP_MakeSent( pOwner, classname, pTable, tr.endpos, ang );
	pTable->deleteThis();
	if ( !pSent )
	{
		Message( "Could not create that" );
		return NULL;
	}
	if ( nearest )
	{
		const Vector cur = pSent->GetAbsOrigin();
		Vector np;
		pSent->CollisionProp()->CalcNearestPoint( cur - tr.plane.normal * 512, &np );
		const Vector pos = tr.endpos + ( cur - np );
		pSent->Teleport( &pos, NULL, &vec3_origin );
	}
	return pSent;
}

CGMPConstraint *CWeaponGMPToolGun::SentWeld( CBasePlayer *pOwner, CBaseEntity *pSent, const trace_t &tr, bool nocollide )
{
	CBaseEntity *pEnt = tr.m_pEnt;
	if ( !pSent || !pEnt || pEnt->IsWorld() || pEnt->IsPlayer() || !GMP_ToolPhys( pEnt, tr.physicsbone ) )
		return NULL;
	KeyValues *t = new KeyValues( "Constraint" );
	t->SetString( "Type", "Weld" );
	t->SetInt( "Bone1", 0 );
	t->SetInt( "Bone2", tr.physicsbone );
	t->SetFloat( "forcelimit", 0 );
	t->SetInt( "nocollide", nocollide ? 1 : 0 );
	CBaseEntity *ents[4] = { pSent, pEnt, NULL, NULL };
	CGMPConstraint *pC = GMP_CreateConstraint( t, ents, pOwner );
	t->deleteThis();
	return pC;
}

void CWeaponGMPToolGun::SentTool( CBasePlayer *pOwner, const trace_t &tr, int button )
{
	const char *mode = s_GMPToolDefs[m_iTool].mode;
	const char *classname = NULL;
	for ( int i = 0; i < GMP_SENT_COUNT; i++ )
		if ( !Q_stricmp( s_GMPSents[i].tool, mode ) )
			classname = s_GMPSents[i].classname;
	CBaseEntity *pEnt = tr.fraction < 1.0f ? tr.m_pEnt : NULL;
	if ( !classname || button == IN_RELOAD || ( !pEnt && !Is( "camera" ) ) || ( pEnt && pEnt->IsPlayer() ) )
		return;
	const int kind = GMP_SentKind( classname );
	CGMPSent *pHit = ( pEnt && !Q_stricmp( pEnt->GetClassname(), classname ) ) ? static_cast<CGMPSent *>( pEnt ) : NULL;

	// Lamp right click: copy its settings into ours.
	if ( kind == GMP_SENT_LAMP && button == IN_ATTACK2 )
	{
		if ( !pHit )
			return;
		KeyValues *t = pHit->GetTable();
		engine->ClientCommand( pOwner->edict(), "lamp_r %d;lamp_g %d;lamp_b %d;lamp_fov %g;lamp_distance %g;lamp_brightness %g;lamp_texture \"%s\";lamp_model \"%s\";lamp_key %d;lamp_toggle %d\n",
			t->GetInt( "r", 255 ), t->GetInt( "g", 255 ), t->GetInt( "b", 255 ), t->GetFloat( "fov", 90 ), t->GetFloat( "distance", 1024 ),
			t->GetFloat( "brightness", 4 ), t->GetString( "Texture", "effects/flashlight001" ), STRING( pHit->GetModelName() ),
			t->GetInt( "KeyDown", 37 ), t->GetInt( "Toggle", 1 ) );
		return;
	}
	// Clicking one of ours updates it (the wheel only with the right button).
	if ( pHit && ( kind != GMP_SENT_WHEEL || button == IN_ATTACK2 ) )
	{
		KeyValues *t = SentSettings( classname, false );
		if ( kind == GMP_SENT_BALLOON )
		{
			int skin;
			GMP_BalloonModel( S( "model" ), skin );
			if ( !Q_stricmp( STRING( pHit->GetModelName() ), "models/maxofs2d/balloon_classic.mdl" ) )
				pHit->m_nSkin = skin;
		}
		pHit->ApplySettings( t );
		t->deleteThis();
		if ( kind == GMP_SENT_WHEEL )
		{
			CUtlVector<CGMPConstraint *> motors;
			GMP_FindConstraints( pHit, "Motor", motors );
			for ( int i = 0; i < motors.Count(); i++ )
			{
				motors[i]->GetTable()->SetFloat( "torque", F( "torque" ) );
				motors[i]->GetTable()->SetFloat( "friction", F( "friction" ) );
			}
		}
		Message( UTIL_VarArgs( "Updated %s", s_GMPSents[kind].name ) );
		return;
	}
	if ( kind == GMP_SENT_WHEEL && button == IN_ATTACK2 )
		return;

	KeyValues *t = SentSettings( classname, true );
	const char *undo = s_GMPSents[kind].name;
	QAngle ang;
	VectorAngles( tr.plane.normal, ang );
	QAngle up = ang;
	up.x += 90;	// the model's up axis along the normal

	GMP_UndoBeginGroup();
	CGMPSent *pSent = NULL;
	CBaseEntity *pExtra = NULL;
	switch ( kind )
	{
	case GMP_SENT_THRUSTER:
	case GMP_SENT_HOVERBALL:
	case GMP_SENT_BUTTON:
	case GMP_SENT_EMITTER:
	case GMP_SENT_DYNAMITE:
	{
		if ( kind != GMP_SENT_EMITTER )
			t->SetString( "Model", S( "model" ) );
		pSent = SpawnSent( pOwner, classname, t, tr, kind == GMP_SENT_EMITTER ? ang : up, true );
		const bool weld = kind == GMP_SENT_THRUSTER || kind == GMP_SENT_HOVERBALL ||
			( ( kind == GMP_SENT_BUTTON || kind == GMP_SENT_EMITTER ) && button == IN_ATTACK );
		if ( pSent && weld )
			pExtra = SentWeld( pOwner, pSent, tr, kind == GMP_SENT_THRUSTER && I( "collision" ) );
		break;
	}
	case GMP_SENT_LAMP:
		t->SetString( "Model", S( "model" ) );
		pSent = SpawnSent( pOwner, classname, t, tr, ang, true );
		break;
	case GMP_SENT_CAMERA:
		if ( button == IN_ATTACK2 )
			t->SetInt( "locked", 1 );
		{
			trace_t eye;
			eye = tr;
			eye.endpos = pOwner->EyePosition();
			pSent = SpawnSent( pOwner, classname, t, eye, pOwner->EyeAngles(), false );
		}
		break;
	case GMP_SENT_WHEEL:
	{
		t->SetString( "Model", S( "model" ) );
		QAngle wheelAng = ang;
		wheelAng.x += F( "rx" );
		wheelAng.y += F( "ry" );
		wheelAng.z += F( "rz" );
		pSent = SpawnSent( pOwner, classname, t, tr, wheelAng, true );
		IPhysicsObject *pPhys = pSent ? pSent->VPhysicsGetObject() : NULL;
		if ( !pSent || !SetPick( 1, tr ) )
			break;
		if ( !pPhys )
		{
			UTIL_Remove( pSent );
			pSent = NULL;
			break;
		}
		Vector lpos1;
		pPhys->WorldToLocal( &lpos1, pSent->GetAbsOrigin() + tr.plane.normal );
		KeyValues *m = new KeyValues( "Constraint" );
		m->SetString( "Type", "Motor" );
		m->SetInt( "Bone1", 0 );
		m->SetInt( "Bone2", m_Picks[1].bone );
		GMP_SetVector( m, "LPos1", lpos1 );
		GMP_SetVector( m, "LPos2", m_Picks[1].local );
		m->SetFloat( "friction", F( "friction" ) );
		m->SetFloat( "torque", F( "torque" ) );
		m->SetFloat( "forcetime", 0 );
		m->SetInt( "nocollide", I( "nocollide" ) );
		m->SetInt( "toggle", I( "toggle" ) );
		m->SetFloat( "forcelimit", F( "forcelimit" ) );
		m->SetInt( "direction", 1 );
		CBaseEntity *ents[4] = { pSent, tr.m_pEnt, NULL, NULL };
		pExtra = GMP_CreateConstraint( m, ents, pOwner );
		m->deleteThis();
		m_Picks[1].ent = NULL;
		if ( !pExtra )
		{
			Message( "Could not attach the wheel" );
			UTIL_Remove( pSent );
			pSent = NULL;
		}
		break;
	}
	case GMP_SENT_BALLOON:
	case GMP_SENT_LIGHT:
	{
		const bool balloon = kind == GMP_SENT_BALLOON;
		int skin = 0;
		t->SetString( "Model", balloon ? GMP_BalloonModel( S( "model" ), skin ) : s_GMPSents[kind].model );
		t->SetInt( "Skin", skin );
		const float length = balloon ? F( "ropelength" ) : clamp( F( "ropelength" ), 4.0f, 1024.0f );
		trace_t at;
		at = tr;
		if ( !balloon )
			at.endpos = tr.endpos + tr.plane.normal * 8;
		pSent = SpawnSent( pOwner, classname, t, at, balloon ? QAngle( 0, 0, 0 ) : up, true );
		IPhysicsObject *pPhys = pSent ? pSent->VPhysicsGetObject() : NULL;
		if ( !pSent || !pPhys || button == IN_ATTACK2 || length <= 0 || !SetPick( 1, tr ) )
			break;
		// The rope from the bottom of the balloon / the top of the light to the hit point.
		Vector bottom;
		pSent->CollisionProp()->CalcNearestPoint( pSent->GetAbsOrigin() - tr.plane.normal * 512, &bottom );
		Vector lpos1;
		pPhys->WorldToLocal( &lpos1, bottom );
		KeyValues *r = new KeyValues( "Constraint" );
		r->SetString( "Type", "Rope" );
		r->SetInt( "Bone1", 0 );
		r->SetInt( "Bone2", m_Picks[1].bone );
		GMP_SetVector( r, "LPos1", lpos1 );
		GMP_SetVector( r, "LPos2", m_Picks[1].local );
		r->SetFloat( "length", length );
		r->SetFloat( "addlength", 0 );
		r->SetFloat( "forcelimit", 0 );
		r->SetFloat( "width", balloon ? 0.5f : 1.0f );
		r->SetString( "material", balloon ? "cable/rope" : S( "ropematerial" ) );
		r->SetInt( "rigid", 0 );
		CBaseEntity *ents[4] = { pSent, tr.m_pEnt, NULL, NULL };
		pExtra = GMP_CreateConstraint( r, ents, pOwner );
		r->deleteThis();
		m_Picks[1].ent = NULL;
		break;
	}
	default:
		t->deleteThis();
		break;
	}
	if ( pSent )
	{
		GMP_AddUndo( pOwner, pSent, undo );
		if ( pExtra )
			GMP_AddUndo( pOwner, pExtra, undo );
		Msg( "gmod_tool %s: %s%s\n", mode, classname, pExtra ? " (attached)" : "" );
	}
	GMP_UndoEndGroup();
}

//-----------------------------------------------------------------------------
// Remover, physprop, colour, material, paint, trails, duplicator
//-----------------------------------------------------------------------------
static bool GMP_ToolCanRemove( CBaseEntity *pEnt )
{
	return pEnt && !pEnt->IsWorld() && !pEnt->IsPlayer() && !( pEnt->GetOwnerEntity() && pEnt->GetOwnerEntity()->IsPlayer() ) &&
		!( pEnt->GetEFlags() & EFL_KILLME );
}

void CWeaponGMPToolGun::OtherTool( CBasePlayer *pOwner, const trace_t &tr, int button )
{
	CBaseEntity *pEnt = tr.fraction < 1.0f ? tr.m_pEnt : NULL;
	if ( Is( "duplicator" ) )
	{
		if ( button == IN_ATTACK )
			GMP_DupePaste( pOwner, tr );
		else if ( button == IN_ATTACK2 )
			GMP_DupeCopy( pOwner, pEnt );
		return;
	}
	if ( Is( "paint" ) )
	{
		if ( button == IN_RELOAD )
		{
			if ( pEnt && !pEnt->IsWorld() )
				pEnt->RemoveAllDecals();
			else if ( pEnt )
				engine->ClientCommand( pOwner->edict(), "r_cleardecals\n" );
			return;
		}
		if ( pEnt )
		{
			trace_t decal;
			decal = tr;
			UTIL_DecalTrace( &decal, S( "decal" ) );
		}
		return;
	}
	if ( !pEnt || pEnt->IsPlayer() )
		return;

	if ( Is( "remover" ) )
	{
		if ( button == IN_RELOAD )
		{
			const int removed = pEnt->IsWorld() ? 0 : GMP_RemoveConstraints( pEnt, NULL );
			if ( removed )
				Message( UTIL_VarArgs( "Removed %d constraint%s", removed, removed == 1 ? "" : "s" ) );
			return;
		}
		if ( !GMP_ToolCanRemove( pEnt ) )
			return;
		if ( button == IN_ATTACK2 )
		{
			CUtlVector<EHANDLE> ents;
			CUtlVector<CGMPConstraint *> constraints;
			GMP_GetConstrained( pEnt, ents, constraints );
			int removed = 0;
			for ( int i = 0; i < ents.Count(); i++ )
			{
				if ( GMP_ToolCanRemove( ents[i].Get() ) )
				{
					UTIL_Remove( ents[i].Get() );
					removed++;
				}
			}
			Message( UTIL_VarArgs( "Removed %d object%s", removed, removed == 1 ? "" : "s" ) );
			return;
		}
		Msg( "gmod_tool remover: %s\n", pEnt->GetClassname() );
		UTIL_Remove( pEnt );
		return;
	}
	if ( pEnt->IsWorld() )
		return;

	if ( Is( "physprop" ) )
	{
		IPhysicsObject *pPhys = GMP_ToolPhys( pEnt, tr.physicsbone );
		const int surface = physprops->GetSurfaceIndex( S( "material" ) );
		if ( button != IN_ATTACK || !pPhys || surface < 0 )
			return;
		pPhys->SetMaterialIndex( surface );
		pPhys->EnableGravity( I( "gravity_toggle" ) != 0 );
		pPhys->Wake();
	}
	else if ( Is( "colour" ) )
	{
		if ( button == IN_ATTACK )
			GMP_SetColour( pEnt, I( "r" ), I( "g" ), I( "b" ), I( "a" ), I( "mode" ), I( "fx" ) );
		else if ( button == IN_RELOAD )
			GMP_SetColour( pEnt, 255, 255, 255, 255, kRenderNormal, kRenderFxNone );
		else
		{
			const color32 c = pEnt->GetRenderColor();
			engine->ClientCommand( pOwner->edict(), "colour_r %d;colour_g %d;colour_b %d;colour_a %d;colour_mode %d;colour_fx %d\n",
				c.r, c.g, c.b, c.a, (int)pEnt->m_nRenderMode, (int)pEnt->m_nRenderFX );
		}
	}
	else if ( Is( "material" ) )
	{
		if ( button == IN_ATTACK )
			GMP_SetMaterial( pEnt, S( "override" ) );
		else if ( button == IN_RELOAD )
			GMP_SetMaterial( pEnt, "" );
		else
			engine->ClientCommand( pOwner->edict(), "material_override \"%s\"\n", GMP_GetMaterial( pEnt ) );
	}
	else if ( Is( "trails" ) )
	{
		if ( button == IN_ATTACK2 )
			GMP_SetTrail( pEnt, NULL );
		else if ( button == IN_ATTACK )
		{
			KeyValues *t = new KeyValues( "trail" );
			KeyValues *c = t->FindKey( "Color", true );
			c->SetInt( "r", I( "r" ) );
			c->SetInt( "g", I( "g" ) );
			c->SetInt( "b", I( "b" ) );
			c->SetInt( "a", I( "a" ) );
			t->SetFloat( "Length", F( "length" ) );
			t->SetFloat( "StartSize", F( "startsize" ) );
			t->SetFloat( "EndSize", F( "endsize" ) );
			t->SetString( "Material", S( "material" ) );
			GMP_SetTrail( pEnt, t );
			t->deleteThis();
		}
	}
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

	int tool = GMP_ToolIndex( gmod_toolmode.GetString() );
	if ( tool < 0 )
		tool = GMP_ToolIndex( "weld" );
	GMP_ToolSync( this, tool );

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

	const GMPToolDef &def = s_GMPToolDefs[tool];
	const int stage = m_iStage;
	const char *left = def.left;
	if ( stage == 1 && def.left1 )
		left = def.left1;
	else if ( stage == 2 && def.left2 )
		left = def.left2;
	else if ( stage >= 3 )
		left = "Select an object to finish the Pulley constraint";
	char lines[3][160];
	int count = 0;
	if ( left )
		Q_snprintf( lines[count++], sizeof( lines[0] ), "Fire: %s", left );
	if ( def.right && ( stage == 0 || !Q_stricmp( def.mode, "rope" ) ) )
		Q_snprintf( lines[count++], sizeof( lines[0] ), "Alt fire: %s", def.right );
	else if ( stage > 0 )
		Q_snprintf( lines[count++], sizeof( lines[0] ), "Alt fire: cancel" );
	if ( def.reload )
		Q_snprintf( lines[count++], sizeof( lines[0] ), "Reload: %s", def.reload );

	wchar_t title[64], text[3][160];
	g_pVGuiLocalize->ConvertANSIToUnicode( def.name, title, sizeof( title ) );
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
