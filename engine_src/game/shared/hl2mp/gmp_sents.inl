//========= GModPort ==========================================================//
//
// Purpose: GMod's sandbox entities (gamemodes/sandbox/entities/entities/gmod_*.lua) as one
//          networked class: gmod_thruster, gmod_wheel, gmod_hoverball, gmod_balloon,
//          gmod_dynamite, gmod_lamp, gmod_light, gmod_emitter, gmod_button, gmod_cameraprop.
//
//          The server keeps each entity's GMod duplicator table (the MakeX argument names), so
//          the tool gun, the duplicator and saves all go through GMP_MakeSent / ApplySettings.
//          Numpad keys reach NumpadKey through gmp_constraints.inl's bindings.
//
//=============================================================================//

#ifdef CLIENT_DLL
#include "tier2/beamsegdraw.h"
#include "c_pixel_visibility.h"
#include "particles_simple.h"
#include "iefx.h"
#include "dlight.h"
#else
#include "explode.h"
#endif

#ifdef CLIENT_DLL
#define CGMPSent C_GMPSent
#endif

enum GMPSentKind_t
{
	GMP_SENT_THRUSTER, GMP_SENT_WHEEL, GMP_SENT_HOVERBALL, GMP_SENT_BALLOON, GMP_SENT_DYNAMITE,
	GMP_SENT_LAMP, GMP_SENT_LIGHT, GMP_SENT_EMITTER, GMP_SENT_BUTTON, GMP_SENT_CAMERA, GMP_SENT_COUNT
};

static const struct { const char *classname, *tool, *name, *model; } s_GMPSents[GMP_SENT_COUNT] =
{
	{ "gmod_thruster", "thruster", "Thruster", "models/dav0r/thruster.mdl" },
	{ "gmod_wheel", "wheel", "Wheel", "models/props_vehicles/carparts_wheel01a.mdl" },
	{ "gmod_hoverball", "hoverball", "Hoverball", "models/dav0r/hoverball.mdl" },
	{ "gmod_balloon", "balloon", "Balloon", "models/maxofs2d/balloon_classic.mdl" },
	{ "gmod_dynamite", "dynamite", "Dynamite", "models/dav0r/tnt/tnt.mdl" },
	{ "gmod_lamp", "lamp", "Lamp", "models/lamps/torch.mdl" },
	{ "gmod_light", "light", "Light", "models/maxofs2d/light_tubular.mdl" },
	{ "gmod_emitter", "emitter", "Emitter", "models/props_lab/tpplug.mdl" },
	{ "gmod_button", "button", "Button", "models/maxofs2d/button_05.mdl" },
	{ "gmod_cameraprop", "camera", "Camera", "models/dav0r/camera.mdl" },
};

static int GMP_SentKind( const char *classname )
{
	for ( int i = 0; classname && i < GMP_SENT_COUNT; i++ )
		if ( !Q_stricmp( classname, s_GMPSents[i].classname ) )
			return i;
	return -1;
}

// The choices string of a tool setting ("effect" of thruster / emitter): effects travel as indices.
static const char *GMP_SentChoices( const char *tool, const char *var )
{
	const int t = GMP_ToolIndex( tool );
	const int v = t >= 0 ? GMP_ToolVarIndex( s_GMPToolDefs[t], var ) : -1;
	return v >= 0 ? s_GMPToolDefs[t].vars[v].choices : "";
}

class CGMPSent : public CBaseAnimating
#ifndef CLIENT_DLL
	, public IGMPNumpadTarget, public IMotionEvent
#endif
{
public:
	DECLARE_CLASS( CGMPSent, CBaseAnimating );
	DECLARE_NETWORKCLASS();

	CGMPSent( void );

	int		Kind( void ) const { return m_iKind; }

	CNetworkVar( int, m_iKind );
	CNetworkVar( bool, m_bOn );
	CNetworkVar( int, m_iEffect );		// thruster / emitter: index into the tool's effect choices
	CNetworkVar( float, m_flParam1 );	// light: brightness; emitter: delay; lamp: fov
	CNetworkVar( float, m_flParam2 );	// light: radius; emitter: scale; lamp: distance
	CNetworkVector( m_vecOffset );		// thruster: nozzle (entity space); lamp: light origin

#ifndef CLIENT_DLL
	void	Precache( void );
	void	Spawn( void );
	void	UpdateOnRemove( void );
	int		ObjectCaps( void );
	int		OnTakeDamage( const CTakeDamageInfo &info );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	SentThink( void );

	// The duplicator table (GMod's MakeX arguments, plus Class / Model / Skin).
	KeyValues *GetTable( void ) const { return m_pTable; }
	CBasePlayer *GetPlayer( void ) const { return ToBasePlayer( m_hPlayer.Get() ); }
	void	SetPlayer( CBasePlayer *pPlayer ) { m_hPlayer = pPlayer; }
	// Merges pSettings into the table and applies it (keys are rebound to the owner).
	void	ApplySettings( KeyValues *pSettings );
	void	SetOn( bool on );
	void	SetDirection( int dir );

	void	NumpadKey( CBasePlayer *pPlayer, int key, bool down );
	simresult_e Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

private:
	float	F( const char *key, float def = 0 ) const { return m_pTable->GetFloat( key, def ); }
	int		I( const char *key, int def = 0 ) const { return m_pTable->GetInt( key, def ); }
	bool	B( const char *key, bool def = false ) const { return m_pTable->GetInt( key, def ? 1 : 0 ) != 0; }
	const char *S( const char *key, const char *def = "" ) const { return m_pTable->GetString( key, def ); }
	void	Bind( const char *key ) { GMP_NumpadBind( GetPlayer(), I( key ), this ); }
	void	KeySwitch( bool down ) { if ( B( "toggle" ) || B( "Toggle" ) ) { if ( down ) SetOn( !m_bOn ); } else SetOn( down ); }

	void	ThrusterForce( float mul );
	void	ThrusterAddMul( float mul, bool down );
	void	ThrusterSound( bool on );
	void	WheelDrive( float mul );
	void	LampUpdate( void );
	void	Explode( void );
	void	SetView( bool on );

	KeyValues	*m_pTable;
	EHANDLE		m_hPlayer;
	IPhysicsMotionController *m_pController;

	// thruster
	float		m_flMul;
	Vector		m_vecLinear;
	AngularImpulse m_vecAngular;
	float		m_flSwitchOff;
	CSoundPatch	*m_pSound;
	char		m_szSound[64];
	// wheel
	int			m_iDirection;
	float		m_flWheelMul;
	// hoverball
	float		m_flTargetZ, m_flZVelocity;
	// dynamite
	float		m_flExplodeTime;
	// lamp
	EHANDLE		m_hFlashlight;
	// button
	EHANDLE		m_hLastUser;
	float		m_flPose;
	// camera
	EHANDLE		m_hViewer;
#else
	void	OnDataChanged( DataUpdateType_t type );
	void	ClientThink( void );
	int		DrawModel( int flags );
	void	GetRenderBounds( Vector &mins, Vector &maxs );

private:
	void	EffectSpot( Vector &pos, Vector &normal ) const;
	void	DrawThruster( void );
	void	DrawGlow( const Vector &pos, float size, int r, int g, int b, int a, pixelvis_handle_t *pPixVis );
	void	ThrusterParticles( void );
	void	EmitterEffect( void );

	float	m_flSeed;
	float	m_flOnStart;
	float	m_flNextEffect;
	pixelvis_handle_t m_PixVis;
	CSmartPtr<CSimpleEmitter> m_pEmitter;
#endif

private:
	CGMPSent( const CGMPSent & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( GMPSent, DT_GMPSent )

BEGIN_NETWORK_TABLE( CGMPSent, DT_GMPSent )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iKind ) ),
	RecvPropBool( RECVINFO( m_bOn ) ),
	RecvPropInt( RECVINFO( m_iEffect ) ),
	RecvPropFloat( RECVINFO( m_flParam1 ) ),
	RecvPropFloat( RECVINFO( m_flParam2 ) ),
	RecvPropVector( RECVINFO( m_vecOffset ) ),
#else
	SendPropInt( SENDINFO( m_iKind ), 4, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bOn ) ),
	SendPropInt( SENDINFO( m_iEffect ), 6 ),
	SendPropFloat( SENDINFO( m_flParam1 ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flParam2 ), 0, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecOffset ), 0, SPROP_NOSCALE ),
#endif
END_NETWORK_TABLE()

#ifndef CLIENT_DLL
LINK_ENTITY_TO_CLASS( gmod_thruster, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_wheel, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_hoverball, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_balloon, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_dynamite, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_lamp, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_light, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_emitter, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_button, CGMPSent );
LINK_ENTITY_TO_CLASS( gmod_cameraprop, CGMPSent );
#endif

CGMPSent::CGMPSent( void )
{
	m_iKind = 0;
	m_bOn = false;
	m_iEffect = 0;
	m_flParam1 = m_flParam2 = 0;
	m_vecOffset.Init();
#ifndef CLIENT_DLL
	m_pTable = new KeyValues( "Entity" );
	m_pController = NULL;
	m_flMul = 0;
	m_vecLinear.Init();
	m_vecAngular.Init();
	m_flSwitchOff = 0;
	m_pSound = NULL;
	m_szSound[0] = 0;
	m_iDirection = 1;
	m_flWheelMul = 0;
	m_flTargetZ = m_flZVelocity = 0;
	m_flExplodeTime = 0;
	m_flPose = 0;
#else
	m_flSeed = RandomFloat( 0, 1000 );
	m_flOnStart = -1;
	m_flNextEffect = 0;
	m_PixVis = 0;
#endif
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Server
//-----------------------------------------------------------------------------
#define GMP_BALLOON_POP_SOUND	"garrysmod/balloon_pop_cute.wav"

void CGMPSent::Precache( void )
{
	BaseClass::Precache();
	if ( GetModelName() != NULL_STRING )
		PrecacheModel( STRING( GetModelName() ) );
	PrecacheSound( GMP_BALLOON_POP_SOUND );
}

void CGMPSent::Spawn( void )
{
	m_iKind = MAX( 0, GMP_SentKind( GetClassname() ) );
	if ( GetModelName() == NULL_STRING || !*STRING( GetModelName() ) )
		SetModelName( AllocPooledString( s_GMPSents[m_iKind].model ) );
	Precache();
	SetModel( STRING( GetModelName() ) );
	SetMoveType( MOVETYPE_VPHYSICS );
	SetSolid( SOLID_VPHYSICS );
	IPhysicsObject *pPhys = VPhysicsInitNormal( SOLID_VPHYSICS, 0, false );
	if ( !pPhys )
		Warning( "GModPort: %s: model %s has no physics\n", GetClassname(), STRING( GetModelName() ) );
	m_takedamage = DAMAGE_EVENTS_ONLY;
	if ( m_iKind == GMP_SENT_EMITTER || m_iKind == GMP_SENT_CAMERA )
		SetCollisionGroup( COLLISION_GROUP_WEAPON );
	if ( m_iKind == GMP_SENT_EMITTER || m_iKind == GMP_SENT_LIGHT )
		AddEffects( EF_NOSHADOW );
	if ( pPhys && ( m_iKind == GMP_SENT_THRUSTER || m_iKind == GMP_SENT_HOVERBALL || m_iKind == GMP_SENT_BALLOON ) && physenv )
	{
		m_pController = physenv->CreateMotionController( this );
		m_pController->AttachObject( pPhys, true );
	}
	if ( pPhys && m_iKind == GMP_SENT_BALLOON )
	{
		pPhys->SetMass( 100 );
		pPhys->EnableGravity( false );
	}
	if ( m_iKind == GMP_SENT_THRUSTER )
	{
		// gmod_thruster: the nozzle is the top of the model, reverse thrust uses the bottom.
		m_vecOffset = Vector( 0, 0, CollisionProp()->OBBMaxs().z );
		m_pTable->SetFloat( "force", 2000 );
		m_pTable->SetInt( "damageable", 1 );
	}
	if ( m_iKind == GMP_SENT_HOVERBALL )
	{
		m_bOn = true;
		m_flTargetZ = GetAbsOrigin().z;
		if ( pPhys )
			pPhys->EnableGravity( false );
	}
	if ( pPhys )
		pPhys->Wake();
	m_pTable->SetString( "Class", GetClassname() );
	SetThink( &CGMPSent::SentThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

int CGMPSent::ObjectCaps( void )
{
	int caps = BaseClass::ObjectCaps() | FCAP_DONT_SAVE;
	if ( m_iKind == GMP_SENT_BUTTON || m_iKind == GMP_SENT_WHEEL )
		caps |= FCAP_IMPULSE_USE;
	return caps;
}

void CGMPSent::ApplySettings( KeyValues *pSettings )
{
	if ( pSettings && pSettings != m_pTable )
	{
		for ( KeyValues *sub = pSettings->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			if ( sub->GetFirstSubKey() )
				continue;	// Data / PhysicsObjects: read by the duplicator itself
			m_pTable->SetString( sub->GetName(), sub->GetString() );
		}
	}
	GMP_NumpadUnbind( this );
	IPhysicsObject *pPhys = VPhysicsGetObject();
	const char *tool = s_GMPSents[m_iKind].tool;

	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		m_iEffect = MAX( 0, GMP_ChoiceFind( GMP_SentChoices( tool, "effect" ), S( "effect", "fire" ) ) );
		ThrusterForce( m_flMul );
		if ( Q_strcmp( m_szSound, S( "soundname" ) ) )
		{
			ThrusterSound( false );
			if ( m_pSound )
				CSoundEnvelopeController::GetController().SoundDestroy( m_pSound );
			m_pSound = NULL;
			const int choice = GMP_ChoiceFind( GMP_SentChoices( tool, "soundname" ), S( "soundname" ) );
			Q_strncpy( m_szSound, choice >= 0 ? S( "soundname" ) : "", sizeof( m_szSound ) );
			if ( m_szSound[0] )
				PrecacheScriptSound( m_szSound );
			if ( m_bOn )
				ThrusterSound( true );
		}
		if ( pPhys && B( "nocollide" ) )
			pPhys->EnableCollisions( false );
		Bind( "key" );
		Bind( "key_bck" );
		break;
	case GMP_SENT_WHEEL:
		m_iDirection = I( "Direction", 1 ) < 0 ? -1 : 1;
		Bind( "key_f" );
		Bind( "key_r" );
		break;
	case GMP_SENT_HOVERBALL:
		if ( pPhys )
			pPhys->SetMass( 150 * clamp( F( "strength", 1 ), 0.1f, 20.0f ) );
		if ( pPhys && B( "nocollide" ) )
			pPhys->EnableCollisions( false );
		Bind( "key_u" );
		Bind( "key_d" );
		Bind( "key_o" );
		break;
	case GMP_SENT_BALLOON:
		SetRenderColor( clamp( I( "r", 255 ), 0, 255 ), clamp( I( "g", 255 ), 0, 255 ), clamp( I( "b", 255 ), 0, 255 ) );
		break;
	case GMP_SENT_DYNAMITE:
		Bind( "key" );
		break;
	case GMP_SENT_LAMP:
		SetRenderColor( clamp( I( "r", 255 ), 0, 255 ), clamp( I( "g", 255 ), 0, 255 ), clamp( I( "b", 255 ), 0, 255 ) );
		m_flParam1 = clamp( F( "fov", 90 ), 10.0f, 170.0f );
		m_flParam2 = clamp( F( "distance", 1024 ), 64.0f, 2048.0f );
		m_bOn = B( "on" );
		LampUpdate();
		Bind( "KeyDown" );
		break;
	case GMP_SENT_LIGHT:
		SetRenderColor( clamp( I( "lightr", 255 ), 0, 255 ), clamp( I( "lightg", 255 ), 0, 255 ), clamp( I( "lightb", 255 ), 0, 255 ) );
		m_flParam1 = clamp( F( "Brightness", 2 ), 0.0f, 10.0f );
		m_flParam2 = clamp( F( "Size", 256 ), 0.0f, 1024.0f );
		m_bOn = B( "on" );
		Bind( "KeyDown" );
		break;
	case GMP_SENT_EMITTER:
		m_iEffect = MAX( 0, GMP_ChoiceFind( GMP_SentChoices( tool, "effect" ), S( "effect", "sparks" ) ) );
		m_flParam1 = clamp( F( "delay", 1 ), 0.05f, 20.0f );
		m_flParam2 = clamp( F( "scale", 1 ), 0.0f, 6.0f );
		if ( m_pTable->FindKey( "on" ) )
			m_bOn = B( "on" );
		else if ( m_pTable->FindKey( "starton" ) )
			m_bOn = B( "starton" );
		if ( pPhys && B( "nocollide" ) )
			pPhys->EnableCollisions( false );
		Bind( "key" );
		break;
	case GMP_SENT_BUTTON:
		if ( pPhys && B( "nocollide" ) )
			pPhys->EnableCollisions( false );
		break;
	case GMP_SENT_CAMERA:
		if ( pPhys )
			pPhys->EnableMotion( !B( "locked" ) );
		Bind( "controlkey" );
		break;
	}
	if ( pPhys )
		pPhys->Wake();
}

void CGMPSent::SetOn( bool on )
{
	const bool was = m_bOn;
	m_bOn = on;
	m_pTable->SetInt( "on", on ? 1 : 0 );
	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		if ( on != was )
			ThrusterSound( on );
		if ( VPhysicsGetObject() )
			VPhysicsGetObject()->Wake();
		break;
	case GMP_SENT_HOVERBALL:
		if ( on && !was )
			m_flTargetZ = GetAbsOrigin().z;
		if ( VPhysicsGetObject() )
		{
			VPhysicsGetObject()->EnableGravity( !on );
			VPhysicsGetObject()->Wake();
		}
		break;
	case GMP_SENT_LAMP:
		if ( on != was )
			LampUpdate();
		break;
	case GMP_SENT_BUTTON:
		if ( on != was )
		{
			// gmod_button: numpad.Activate / Deactivate on the owner's key.
			GMP_NumpadRun( GetPlayer(), I( "key" ), on );
			SetNextThink( gpGlobals->curtime );
		}
		break;
	case GMP_SENT_CAMERA:
		SetView( on );
		break;
	}
}

// gmod_thruster ENT:SetForce
void CGMPSent::ThrusterForce( float mul )
{
	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( !pPhys )
		return;
	const Vector thrust( 0, 0, CollisionProp()->OBBMaxs().z ), thrustR( 0, 0, CollisionProp()->OBBMins().z );
	Vector worldPos, worldForce;
	pPhys->LocalToWorld( &worldPos, thrust );
	pPhys->LocalToWorldVector( &worldForce, -thrust );
	worldForce *= clamp( F( "force", 2000 ), 0.0f, 100000.0f ) * mul * 50;
	const bool motion = pPhys->IsMotionEnabled();
	pPhys->EnableMotion( true );
	Vector linear;
	pPhys->CalculateVelocityOffset( worldForce, worldPos, &linear, &m_vecAngular );
	pPhys->EnableMotion( motion );
	pPhys->WorldToLocalVector( &m_vecLinear, linear );
	m_vecOffset = mul >= 0 ? thrust : thrustR;
}

// gmod_thruster ENT:AddMul
void CGMPSent::ThrusterAddMul( float mul, bool down )
{
	if ( B( "toggle" ) )
	{
		if ( !down )
			return;
		m_flMul = ( m_flMul == mul ) ? 0 : mul;
	}
	else
		m_flMul += mul;
	ThrusterForce( m_flMul );
	SetOn( m_flMul != 0 );
}

void CGMPSent::ThrusterSound( bool on )
{
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
	if ( !on )
	{
		if ( m_pSound )
			controller.SoundChangeVolume( m_pSound, 0, 0.25f );
		return;
	}
	if ( !m_szSound[0] )
		return;
	if ( !m_pSound )
	{
		CPASAttenuationFilter filter( this );
		m_pSound = controller.SoundCreate( filter, entindex(), m_szSound );
	}
	if ( m_pSound )
		controller.Play( m_pSound, 0.5f, 100 );
}

// gmod_wheel ENT:Forward: the wheel's Motor constraint turns at Direction * mul.
void CGMPSent::WheelDrive( float mul )
{
	m_flWheelMul = mul;
	CUtlVector<CGMPConstraint *> motors;
	GMP_FindConstraints( this, "Motor", motors );
	for ( int i = 0; i < motors.Count(); i++ )
	{
		const float dir = motors[i]->GetTable()->GetFloat( "direction", 1 ) < 0 ? -1.0f : 1.0f;
		motors[i]->SetMotorScale( dir * m_iDirection * mul );
	}
}

void CGMPSent::SetDirection( int dir )
{
	m_iDirection = dir < 0 ? -1 : 1;
	m_pTable->SetInt( "Direction", m_iDirection );
	if ( m_flWheelMul != 0 )
		WheelDrive( m_flWheelMul );
}

// gmod_lamp: an env_projectedtexture in front of the lamp while it is on.
void CGMPSent::LampUpdate( void )
{
	UTIL_Remove( m_hFlashlight.Get() );
	m_hFlashlight = NULL;

	// Model offsets from gmod_lamp's list.
	const char *model = STRING( GetModelName() );
	Vector offset( 5, 0, 0 );
	if ( Q_stristr( model, "lamp_flashlight" ) || Q_stristr( model, "lamp_projector" ) )
		offset.Init( 8.5f, 0, 0 );
	else if ( Q_stristr( model, "light_spotlight01_lamp" ) )
		offset.Init( 9, 0, 4 );
	else if ( Q_stristr( model, "light_spotlight02_lamp" ) )
		offset.Init( 5.5f, 0, 0 );
	else if ( Q_stristr( model, "light_decklight01_off" ) )
		offset.Init( 3, 0, 0 );
	m_vecOffset = offset;
	if ( Q_stristr( model, "light_spotlight0" ) || Q_stristr( model, "light_decklight01" ) )
		m_nSkin = m_bOn ? 1 : 0;

	if ( !m_bOn )
		return;
	CBaseEntity *pLight = CreateEntityByName( "env_projectedtexture" );
	if ( !pLight )
		return;
	const float brightness = clamp( F( "brightness", 4 ), 0.0f, 8.0f );
	const color32 c = GetRenderColor();
	pLight->KeyValue( "enableshadows", "1" );
	pLight->KeyValue( "nearz", "12" );
	pLight->KeyValue( "farz", UTIL_VarArgs( "%f", m_flParam2.Get() ) );
	pLight->KeyValue( "lightfov", UTIL_VarArgs( "%f", m_flParam1.Get() ) );
	pLight->KeyValue( "texturename", S( "Texture", "effects/flashlight001" ) );
	pLight->KeyValue( "lightcolor", UTIL_VarArgs( "%d %d %d 255", (int)( c.r * brightness ), (int)( c.g * brightness ), (int)( c.b * brightness ) ) );
	Vector pos;
	EntityToWorldSpace( offset, &pos );
	pLight->SetAbsOrigin( pos );
	pLight->SetAbsAngles( GetAbsAngles() );
	DispatchSpawn( pLight );
	pLight->SetParent( this );
	pLight->Activate();
	m_hFlashlight = pLight;
}

void CGMPSent::Explode( void )
{
	m_flExplodeTime = 0;
	CBasePlayer *pPlayer = GetPlayer();
	const int damage = clamp( I( "Damage", 200 ), 0, 1500 );
	const Vector pos = WorldSpaceCenter();
	const bool remove = B( "remove" );
	if ( remove )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
		AddEffects( EF_NODRAW );
	}
	ExplosionCreate( pos, GetAbsAngles(), pPlayer ? (CBaseEntity *)pPlayer : (CBaseEntity *)this, damage, 300, true );
	if ( remove )
		UTIL_Remove( this );
}

void CGMPSent::SetView( bool on )
{
	CBasePlayer *pPlayer = on ? GetPlayer() : ToBasePlayer( m_hViewer.Get() );
	if ( !pPlayer )
		return;
	if ( on )
	{
		pPlayer->SetViewEntity( this );
		m_hViewer = pPlayer;
	}
	else
	{
		if ( pPlayer->GetViewEntity() == this )
			pPlayer->SetViewEntity( NULL );
		m_hViewer = NULL;
	}
}

void CGMPSent::NumpadKey( CBasePlayer *pPlayer, int key, bool down )
{
	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		// Thruster_On / Thruster_Off with mul 1 (key) and -1 (key_bck)
		if ( key == I( "key" ) )
			ThrusterAddMul( down ? 1.0f : -1.0f, down );
		if ( key == I( "key_bck" ) )
			ThrusterAddMul( down ? -1.0f : 1.0f, down );
		break;
	case GMP_SENT_WHEEL:
	{
		const float mul = ( key == I( "key_f" ) ) ? 1.0f : -1.0f;
		if ( B( "Toggle" ) )
		{
			if ( down )
				WheelDrive( m_flWheelMul == mul ? 0.0f : mul );
		}
		else if ( down )
			WheelDrive( mul );
		else if ( m_flWheelMul == mul )
			WheelDrive( 0 );
		break;
	}
	case GMP_SENT_HOVERBALL:
		if ( key == I( "key_o" ) )
		{
			if ( down )
				SetOn( !m_bOn );
		}
		else
		{
			// HoverballUp / Down: SetZVelocity( +-1 ) while held
			const float dir = ( key == I( "key_u" ) ) ? 1.0f : -1.0f;
			m_flZVelocity = down ? dir * gpGlobals->interval_per_tick * 5000 : 0.0f;
			if ( VPhysicsGetObject() )
				VPhysicsGetObject()->Wake();
		}
		break;
	case GMP_SENT_DYNAMITE:
		if ( !down )
			break;
		if ( F( "delay" ) > 0 )
		{
			if ( !m_flExplodeTime )
				m_flExplodeTime = gpGlobals->curtime + F( "delay" );
			SetNextThink( gpGlobals->curtime );
		}
		else
			Explode();
		break;
	case GMP_SENT_LAMP:
	case GMP_SENT_LIGHT:
		if ( B( "Toggle" ) || B( "toggle" ) )
		{
			if ( down )
				SetOn( !m_bOn );
		}
		else
			SetOn( down );
		break;
	case GMP_SENT_EMITTER:
	case GMP_SENT_CAMERA:
		KeySwitch( down );
		break;
	}
}

IMotionEvent::simresult_e CGMPSent::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	linear.Init();
	angular.Init();
	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		if ( !m_bOn )
			return SIM_NOTHING;
		linear = m_vecLinear;
		angular = m_vecAngular;
		return SIM_LOCAL_ACCELERATION;
	case GMP_SENT_BALLOON:
		linear.z = F( "force", 500 ) * 5000 * deltaTime;
		return SIM_GLOBAL_FORCE;
	case GMP_SENT_HOVERBALL:
	{
		if ( !m_bOn )
			return SIM_NOTHING;
		if ( m_flZVelocity != 0 )
			m_flTargetZ += m_flZVelocity * deltaTime * F( "speed", 1 );
		Vector pos, vel;
		pObject->GetPosition( &pos, NULL );
		pObject->GetVelocity( &vel, NULL );
		const float resistance = F( "resistance", 0 );
		const float distance = m_flTargetZ - pos.z;
		float exponent = distance * distance * ( distance < 0 ? -1.0f : 1.0f ) * deltaTime * 300;
		exponent -= vel.z * deltaTime * 600 * ( resistance + 1 );
		linear.z = clamp( exponent, -5000.0f, 5000.0f );
		if ( resistance > 0 )
		{
			linear.x = -vel.x * resistance;
			linear.y = -vel.y * resistance;
		}
		return SIM_GLOBAL_ACCELERATION;
	}
	}
	return SIM_NOTHING;
}

int CGMPSent::OnTakeDamage( const CTakeDamageInfo &info )
{
	VPhysicsTakeDamage( info );
	if ( m_iKind == GMP_SENT_BALLOON )
	{
		// gmod_balloon: pop in the balloon's colour.
		const color32 c = GetRenderColor();
		CEffectData data;
		data.m_vOrigin = WorldSpaceCenter();
		data.m_vStart = Vector( c.r, c.g, c.b );
		DispatchEffect( "GMPBalloonPop", data );
		EmitSound( GMP_BALLOON_POP_SOUND );
		UTIL_Remove( this );
	}
	else if ( m_iKind == GMP_SENT_THRUSTER && B( "damageable" ) )
	{
		SetOn( true );
		m_flSwitchOff = gpGlobals->curtime + 5;
		SetNextThink( gpGlobals->curtime );
	}
	return 0;
}

void CGMPSent::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;
	if ( m_iKind == GMP_SENT_WHEEL )
	{
		SetDirection( -m_iDirection );	// gmod_wheel: use flips the direction
		return;
	}
	if ( m_iKind != GMP_SENT_BUTTON )
		return;
	if ( B( "toggle" ) )
	{
		SetOn( !m_bOn );
		return;
	}
	if ( m_hLastUser.Get() || m_bOn )
		return;
	m_hLastUser = pActivator;
	SetOn( true );
}

void CGMPSent::SentThink( void )
{
	float next = 0.1f;
	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		if ( m_flSwitchOff && gpGlobals->curtime >= m_flSwitchOff )
		{
			m_flSwitchOff = 0;
			SetOn( m_flMul != 0 );
		}
		break;
	case GMP_SENT_DYNAMITE:
		if ( m_flExplodeTime && gpGlobals->curtime >= m_flExplodeTime )
		{
			Explode();
			return;
		}
		break;
	case GMP_SENT_BUTTON:
	{
		// Hold buttons stay on while the last user keeps +use down.
		CBasePlayer *pUser = ToBasePlayer( m_hLastUser.Get() );
		if ( m_bOn && !B( "toggle" ) && ( !pUser || !( pUser->m_nButtons & IN_USE ) ) )
		{
			m_hLastUser = NULL;
			SetOn( false );
		}
		const float target = m_bOn ? 1.0f : 0.0f;
		if ( m_flPose != target )
		{
			m_flPose = Approach( target, m_flPose, gpGlobals->interval_per_tick * 5 );
			SetPoseParameter( "switch", m_flPose );
			next = 0;
		}
		else if ( m_bOn )
			next = 0;
		break;
	}
	}
	SetNextThink( gpGlobals->curtime + next );
}

void CGMPSent::UpdateOnRemove( void )
{
	GMP_NumpadUnbind( this );
	if ( m_pController && physenv )
		physenv->DestroyMotionController( m_pController );
	m_pController = NULL;
	if ( m_pSound )
		CSoundEnvelopeController::GetController().SoundDestroy( m_pSound );
	m_pSound = NULL;
	UTIL_Remove( m_hFlashlight.Get() );
	SetView( false );
	if ( m_pTable )
		m_pTable->deleteThis();
	m_pTable = NULL;
	BaseClass::UpdateOnRemove();
}

// MakeThruster / MakeLamp / ...: spawns 'classname' from a duplicator table (Model, Pos, Angle,
// the entity's own fields) owned by pPlayer.
static CGMPSent *GMP_MakeSent( CBasePlayer *pPlayer, const char *classname, KeyValues *pTable, const Vector &pos, const QAngle &ang )
{
	const int kind = GMP_SentKind( classname );
	if ( kind < 0 )
		return NULL;
	const char *model = pTable ? pTable->GetString( "Model", pTable->GetString( "model", "" ) ) : "";
	if ( kind == GMP_SENT_EMITTER || kind == GMP_SENT_CAMERA || !*model || !Q_stristr( model, ".mdl" ) )
		model = s_GMPSents[kind].model;
	CGMPSent *pEnt = static_cast<CGMPSent *>( CreateEntityByName( classname ) );
	if ( !pEnt )
		return NULL;
	pEnt->SetModelName( AllocPooledString( model ) );
	pEnt->SetAbsOrigin( pos );
	pEnt->SetAbsAngles( ang );
	pEnt->SetPlayer( pPlayer );
	if ( pTable )
		pEnt->m_nSkin = pTable->GetInt( "Skin", pTable->GetInt( "skin", 0 ) );
	DispatchSpawn( pEnt );
	pEnt->Activate();
	pEnt->GetTable()->SetString( "Model", model );
	pEnt->ApplySettings( pTable );
	return pEnt;
}

#else
//-----------------------------------------------------------------------------
// Client
//-----------------------------------------------------------------------------
void C_GMPSent::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	if ( type == DATA_UPDATE_CREATED )
		SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void C_GMPSent::GetRenderBounds( Vector &mins, Vector &maxs )
{
	BaseClass::GetRenderBounds( mins, maxs );
	// Room for the flames (gmod_thruster grows its bounds by 128 the same way).
	if ( m_iKind == GMP_SENT_THRUSTER )
	{
		mins -= Vector( 160, 160, 160 );
		maxs += Vector( 160, 160, 160 );
	}
}

// The nozzle in world space and the direction the effect points.
void C_GMPSent::EffectSpot( Vector &pos, Vector &normal ) const
{
	EntityToWorldSpace( m_vecOffset, &pos );
	normal = pos - GetAbsOrigin();
	if ( VectorNormalize( normal ) < 0.001f )
		AngleVectors( GetAbsAngles(), NULL, NULL, &normal );
}

static IMaterial *GMP_SentMaterial( const char *name )
{
	IMaterial *pMat = materials->FindMaterial( name, TEXTURE_GROUP_CLIENT_EFFECTS );
	return ( pMat && !pMat->IsErrorMaterial() ) ? pMat : NULL;
}

static void GMP_BeamPoint( CBeamSegDraw &beam, const Vector &pos, float width, float tex, float r, float g, float b, float a )
{
	BeamSeg_t seg;
	seg.m_vPos = pos;
	seg.m_flWidth = width;
	seg.m_flTexCoord = tex;
	seg.m_vColor.Init( r, g, b );
	seg.m_flAlpha = a;
	beam.NextSeg( &seg );
}

// gmod_thruster's "fire" and "plasma" ThrusterEffects
void C_GMPSent::DrawThruster( void )
{
	char value[64], label[64];
	if ( !GMP_ChoiceAt( GMP_SentChoices( "thruster", "effect" ), m_iEffect, value, sizeof( value ), label, sizeof( label ) ) )
		return;
	const bool fire = !Q_stricmp( value, "fire" ), plasma = !Q_stricmp( value, "plasma" );
	if ( !fire && !plasma )
		return;
	Vector pos, normal;
	EffectSpot( pos, normal );
	const Vector extent = CollisionProp()->OBBMaxs() - CollisionProp()->OBBMins();
	CMatRenderContextPtr pRenderContext( materials );

	if ( fire )
	{
		static IMaterial *s_pFire = GMP_SentMaterial( "effects/fire_cloud1" );
		if ( !s_pFire )
			return;
		const float size = MIN( MIN( extent.x, extent.y ), 50.0f );
		const float scale = clamp( ( gpGlobals->curtime - m_flOnStart ) * 5, 0.0f, 1.0f );
		float scroll = fmodf( m_flSeed - gpGlobals->curtime * 10, 1000.0f );
		const float widths[2][2] = { { 32, 32 }, { 16, 16 } };
		for ( int i = 0; i < 2; i++ )
		{
			CBeamSegDraw beam;
			beam.Start( pRenderContext, 3, s_pFire );
			GMP_BeamPoint( beam, pos, size * scale, scroll, 0, 0, 1, 0.5f );
			GMP_BeamPoint( beam, pos + normal * 60 * scale, widths[i][0] * scale, scroll + 1, 1, 1, 1, 0.5f );
			GMP_BeamPoint( beam, pos + normal * 148 * scale, widths[i][1] * scale, scroll + 3, 1, 1, 1, 0 );
			beam.End();
			scroll *= 0.65f;
		}
		return;
	}

	static IMaterial *s_pPlasma = GMP_SentMaterial( "effects/strider_muzzle" );
	if ( !s_pPlasma )
		return;
	const float size = MIN( extent.x, extent.y ) * 1.5f;
	float scroll = fmodf( m_flSeed - gpGlobals->curtime * 20, 1000.0f );
	for ( int i = 0; i < 3; i++ )
	{
		scroll *= 0.9f;
		CBeamSegDraw beam;
		beam.Start( pRenderContext, 3, s_pPlasma );
		GMP_BeamPoint( beam, pos, size, scroll, 0, 1, 1, 1 );
		GMP_BeamPoint( beam, pos + normal * 8, size, scroll + 0.01f, 1, 1, 1, 1 );
		GMP_BeamPoint( beam, pos + normal * 64, size, scroll + 0.02f, 0, 1, 1, 0 );
		beam.End();
	}
}

// A glow sprite, faded by how much of it is on screen (pixel visibility).
void C_GMPSent::DrawGlow( const Vector &pos, float size, int r, int g, int b, int a, pixelvis_handle_t *pPixVis )
{
	static IMaterial *s_pGlow = GMP_SentMaterial( "sprites/light_glow02_add" );
	if ( !s_pGlow )
		return;
	float visible = 1;
	if ( pPixVis )
	{
		pixelvis_queryparams_t params;
		params.Init( pos, 3 );
		visible = PixelVisibility_FractionVisible( params, pPixVis );
	}
	if ( visible <= 0 )
		return;
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( s_pGlow );
	color32 c = { (byte)r, (byte)g, (byte)b, (byte)( a * visible ) };
	DrawSprite( pos, size * 0.5f, size * 0.5f, c );
}

int C_GMPSent::DrawModel( int flags )
{
	const int ret = BaseClass::DrawModel( flags );
	if ( !( flags & STUDIO_RENDER ) || !m_bOn )
		return ret;
	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		if ( m_flOnStart >= 0 )
			DrawThruster();
		break;
	case GMP_SENT_HOVERBALL:
		DrawGlow( WorldSpaceCenter(), 22 + sinf( gpGlobals->curtime * 6 ) * 3, 70, 180, 255, 255, &m_PixVis );
		break;
	case GMP_SENT_LIGHT:
	{
		const color32 c = GetRenderColor();
		Vector up;
		AngleVectors( GetAbsAngles(), NULL, NULL, &up );
		DrawGlow( GetAbsOrigin() - up * 5, 64, c.r, c.g, c.b, 64, &m_PixVis );
		break;
	}
	case GMP_SENT_LAMP:
	{
		const color32 c = GetRenderColor();
		Vector pos;
		EntityToWorldSpace( m_vecOffset, &pos );
		DrawGlow( pos, 32, c.r, c.g, c.b, 200, &m_PixVis );
		break;
	}
	}
	return ret;
}

// gmod_thruster's "smoke" (and this port's "sparks") ThrusterEffects
void C_GMPSent::ThrusterParticles( void )
{
	char value[64], label[64];
	if ( !GMP_ChoiceAt( GMP_SentChoices( "thruster", "effect" ), m_iEffect, value, sizeof( value ), label, sizeof( label ) ) )
		return;
	Vector pos, normal;
	EffectSpot( pos, normal );
	if ( !Q_stricmp( value, "sparks" ) )
	{
		if ( gpGlobals->curtime < m_flNextEffect )
			return;
		m_flNextEffect = gpGlobals->curtime + 0.05f;
		g_pEffects->Sparks( pos, 1, 2, &normal );
		return;
	}
	if ( Q_stricmp( value, "smoke" ) || gpGlobals->curtime < m_flNextEffect )
		return;
	m_flNextEffect = gpGlobals->curtime + 0.015f;
	if ( !m_pEmitter )
		m_pEmitter = CSimpleEmitter::Create( "gmp_thruster_smoke" );
	if ( !m_pEmitter )
		return;
	static PMaterialHandle s_hSmoke = m_pEmitter->GetPMaterial( "particles/smokey" );
	m_pEmitter->SetSortOrigin( pos );
	const Vector extent = CollisionProp()->OBBMaxs() - CollisionProp()->OBBMins();
	const float size = MIN( extent.x, extent.y ) / 2;
	Vector dir = ( pos + normal * 32 + RandomVector( -3, 3 ) ) - GetAbsOrigin();
	VectorNormalize( dir );
	SimpleParticle *p = m_pEmitter->AddSimpleParticle( s_hSmoke, pos + RandomVector( -3, 3 ), 2.0f, (unsigned char)clamp( size, 1.0f, 255.0f ) );
	if ( !p )
		return;
	const float velScale = RandomFloat( 10, 30 ) * 10 / clamp( GetAbsVelocity().Length() / 200, 1.0f, 10.0f );
	p->m_vecVelocity = dir * velScale + Vector( 0, 0, 16 );
	p->m_uchStartAlpha = (unsigned char)RandomInt( 50, 150 );
	p->m_uchEndAlpha = 0;
	p->m_uchStartSize = (unsigned char)clamp( size, 1.0f, 255.0f );
	p->m_uchEndSize = (unsigned char)RandomInt( 64, 128 );
	p->m_flRoll = RandomFloat( 0, 360 );
	p->m_flRollDelta = RandomFloat( -0.2f, 0.2f );
	p->m_uchColor[0] = 200;
	p->m_uchColor[1] = 200;
	p->m_uchColor[2] = 210;
}

// gmod_emitter's EffectType list, mapped to the HL2 client effects this build has.
void C_GMPSent::EmitterEffect( void )
{
	char value[64], label[64];
	if ( !GMP_ChoiceAt( GMP_SentChoices( "emitter", "effect" ), m_iEffect, value, sizeof( value ), label, sizeof( label ) ) )
		return;
	const QAngle ang = GetAbsAngles();
	Vector fwd;
	AngleVectors( ang, &fwd );
	const Vector pos = GetAbsOrigin() + fwd * 12;
	const float scale = m_flParam2;

	CEffectData data;
	data.m_vOrigin = pos;
	data.m_vStart = pos;
	data.m_vNormal = fwd;
	data.m_vAngles = ang;
	data.m_flScale = 1;
	data.m_flMagnitude = 1;
	data.m_flRadius = 1;
	data.m_hEntity = GetRefEHandle();
	const char *effect = NULL;

	if ( !Q_stricmp( value, "sparks" ) )
	{
		g_pEffects->Sparks( pos, MAX( 1, (int)( scale / 2 ) ), MAX( 1, (int)( 8 - scale ) ), &fwd );
		return;
	}
	if ( !Q_stricmp( value, "smoke" ) )
	{
		if ( !m_pEmitter )
			m_pEmitter = CSimpleEmitter::Create( "gmp_emitter_smoke" );
		if ( !m_pEmitter )
			return;
		static PMaterialHandle s_hSmoke = m_pEmitter->GetPMaterial( "particles/smokey" );
		m_pEmitter->SetSortOrigin( pos );
		for ( int i = 0; i < 6; i++ )
		{
			SimpleParticle *p = m_pEmitter->AddSimpleParticle( s_hSmoke, pos + RandomVector( -4, 4 ), RandomFloat( 1.5f, 2.5f ), 16 );
			if ( !p )
				break;
			p->m_vecVelocity = fwd * RandomFloat( 20, 60 ) + RandomVector( -15, 15 ) + Vector( 0, 0, 20 );
			p->m_uchStartAlpha = 120;
			p->m_uchEndAlpha = 0;
			p->m_uchStartSize = (unsigned char)clamp( 8 * MAX( scale, 0.5f ), 1.0f, 255.0f );
			p->m_uchEndSize = (unsigned char)clamp( 40 * MAX( scale, 0.5f ), 1.0f, 255.0f );
			p->m_flRoll = RandomFloat( 0, 360 );
			p->m_flRollDelta = RandomFloat( -0.5f, 0.5f );
			p->m_uchColor[0] = p->m_uchColor[1] = p->m_uchColor[2] = 160;
		}
		return;
	}
	if ( !Q_stricmp( value, "stunstickimpact" ) )
		effect = "StunstickImpact";
	else if ( !Q_stricmp( value, "manhacksparks" ) )
	{
		effect = "ManhackSparks";
		data.m_vNormal = fwd * scale;
	}
	else if ( !Q_stricmp( value, "bloodspray" ) )
	{
		effect = "bloodspray";
		data.m_vNormal = fwd * MAX( 3.0f, scale ) / 3;
		data.m_flScale = scale + 9;
		data.m_nColor = 0;
		data.m_fFlags = 3;
	}
	else if ( !Q_stricmp( value, "striderblood" ) )
	{
		effect = "StriderBlood";
		data.m_flScale = MAX( scale, 0.5f );
	}
	else if ( !Q_stricmp( value, "shells" ) )
		effect = "ShellEject";
	else if ( !Q_stricmp( value, "rifleshells" ) )
		effect = "RifleShellEject";
	else if ( !Q_stricmp( value, "shotgunshells" ) )
		effect = "ShotgunShellEject";
	else if ( !Q_stricmp( value, "cball_explode" ) )
		effect = "cball_explode";
	else if ( !Q_stricmp( value, "cball_bounce" ) )
	{
		effect = "cball_bounce";
		data.m_vNormal = fwd * scale / 4;
		data.m_flRadius = scale * 2;
	}
	else if ( !Q_stricmp( value, "thumperdust" ) )
	{
		effect = "ThumperDust";
		data.m_flScale = scale * 24;
	}
	else if ( !Q_stricmp( value, "ar2impact" ) )
		effect = "AR2Impact";
	else if ( !Q_stricmp( value, "ar2explosion" ) )
	{
		effect = "AR2Explosion";
		data.m_flRadius = scale * 4;
	}
	else if ( !Q_stricmp( value, "explosion" ) )
	{
		effect = "Explosion";
		data.m_fFlags = 4;
	}
	else if ( !Q_stricmp( value, "helicoptermegabomb" ) )
		effect = "HelicopterMegaBomb";
	else if ( !Q_stricmp( value, "underwaterexplosion" ) )
	{
		effect = "WaterSurfaceExplosion";
		data.m_fFlags = 4;
	}
	else if ( !Q_stricmp( value, "watersplash" ) || !Q_stricmp( value, "dirtywatersplash" ) )
	{
		effect = "watersplash";
		data.m_flScale = scale * 2;
		data.m_fFlags = !Q_stricmp( value, "dirtywatersplash" ) ? 1 : 0;
	}
	else if ( !Q_stricmp( value, "glassimpact" ) )
		effect = "GlassImpact";
	else if ( !Q_stricmp( value, "bloodimpact" ) )
		effect = "BloodImpact";
	if ( effect )
		DispatchEffectToCallback( effect, data );
}

void C_GMPSent::ClientThink( void )
{
	if ( !m_bOn )
	{
		m_flOnStart = -1;
		return;
	}
	if ( m_flOnStart < 0 )
		m_flOnStart = gpGlobals->curtime;
	switch ( m_iKind )
	{
	case GMP_SENT_THRUSTER:
		ThrusterParticles();
		break;
	case GMP_SENT_EMITTER:
		if ( gpGlobals->curtime >= m_flNextEffect )
		{
			m_flNextEffect = gpGlobals->curtime + MAX( (float)m_flParam1, 0.05f );
			EmitterEffect();
		}
		break;
	case GMP_SENT_LIGHT:
	{
		// gmod_light: a dynamic light refreshed every frame.
		dlight_t *dl = effects->CL_AllocDlight( entindex() );
		if ( !dl )
			break;
		const color32 c = GetRenderColor();
		Vector up;
		AngleVectors( GetAbsAngles(), NULL, NULL, &up );
		dl->origin = GetAbsOrigin() - up * 8;
		dl->color.r = c.r;
		dl->color.g = c.g;
		dl->color.b = c.b;
		dl->color.exponent = (signed char)clamp( (int)m_flParam1, 0, 10 );
		dl->radius = m_flParam2;
		dl->decay = m_flParam2 * 5;
		dl->die = gpGlobals->curtime + 1;
		break;
	}
	}
}

// gmod_balloon's balloon_pop effect: a puff in the balloon's colour.
static void GMP_BalloonPopCallback( const CEffectData &data )
{
	CSmartPtr<CSimpleEmitter> pEmitter = CSimpleEmitter::Create( "gmp_balloon_pop" );
	if ( !pEmitter )
		return;
	pEmitter->SetSortOrigin( data.m_vOrigin );
	PMaterialHandle hMat = pEmitter->GetPMaterial( "particle/particle_noisesphere" );
	for ( int i = 0; i < 32; i++ )
	{
		SimpleParticle *p = pEmitter->AddSimpleParticle( hMat, data.m_vOrigin + RandomVector( -4, 4 ), RandomFloat( 0.4f, 0.8f ), 6 );
		if ( !p )
			break;
		p->m_vecVelocity = RandomVector( -1, 1 ).Normalized() * RandomFloat( 100, 300 );
		p->m_uchColor[0] = (unsigned char)clamp( data.m_vStart.x, 0.0f, 255.0f );
		p->m_uchColor[1] = (unsigned char)clamp( data.m_vStart.y, 0.0f, 255.0f );
		p->m_uchColor[2] = (unsigned char)clamp( data.m_vStart.z, 0.0f, 255.0f );
		p->m_uchStartAlpha = 255;
		p->m_uchEndAlpha = 0;
		p->m_uchStartSize = (unsigned char)RandomInt( 2, 4 );
		p->m_uchEndSize = 0;
		p->m_flRoll = RandomFloat( 0, 360 );
		p->m_flRollDelta = RandomFloat( -4, 4 );
	}
}
DECLARE_CLIENT_EFFECT( "GMPBalloonPop", GMP_BalloonPopCallback );

//-----------------------------------------------------------------------------
// Numpad: keyboard numpad keys, or the touch numpad (+gmp_kp0 .. +gmp_kp9, +gmp_kpenter...).
//-----------------------------------------------------------------------------
extern void ( *g_pfnGMPKeyEvent )( int eventcode, ButtonCode_t keynum );

static void GMP_NumpadSend( int key, bool down )
{
	static bool s_bDown[16];
	if ( key < KEY_PAD_0 || key > KEY_PAD_DECIMAL || s_bDown[key - KEY_PAD_0] == down )
		return;
	s_bDown[key - KEY_PAD_0] = down;
	char cmd[32];
	Q_snprintf( cmd, sizeof( cmd ), "gmp_numpad %d %d\n", key, down ? 1 : 0 );
	engine->ClientCmd_Unrestricted( cmd );
}

static void GMP_NumpadKeyEvent( int eventcode, ButtonCode_t keynum )
{
	GMP_NumpadSend( keynum, eventcode != 0 );
}

static struct GMPNumpadHookInit_t { GMPNumpadHookInit_t() { g_pfnGMPKeyEvent = GMP_NumpadKeyEvent; } } s_GMPNumpadHookInit;

// "+gmp_kp7" / "-gmp_kp7", "+gmp_kpenter"...
static void GMP_NumpadCommand( const CCommand &args )
{
	const char *name = args[0];
	const bool down = name[0] == '+';
	const char *key = name + 7;	// after "+gmp_kp"
	static const char *const s_Names[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "slash", "star", "minus", "plus", "enter", "dot" };
	for ( int i = 0; i < ARRAYSIZE( s_Names ); i++ )
		if ( !Q_stricmp( key, s_Names[i] ) )
			GMP_NumpadSend( KEY_PAD_0 + i, down );
}
#define GMP_NUMPAD_CMD( n ) \
	static ConCommand s_GMPKpDown_##n( "+gmp_kp" #n, GMP_NumpadCommand, "GModPort: numpad key down" ); \
	static ConCommand s_GMPKpUp_##n( "-gmp_kp" #n, GMP_NumpadCommand, "GModPort: numpad key up" );
GMP_NUMPAD_CMD( 0 ) GMP_NUMPAD_CMD( 1 ) GMP_NUMPAD_CMD( 2 ) GMP_NUMPAD_CMD( 3 ) GMP_NUMPAD_CMD( 4 )
GMP_NUMPAD_CMD( 5 ) GMP_NUMPAD_CMD( 6 ) GMP_NUMPAD_CMD( 7 ) GMP_NUMPAD_CMD( 8 ) GMP_NUMPAD_CMD( 9 )
GMP_NUMPAD_CMD( slash ) GMP_NUMPAD_CMD( star ) GMP_NUMPAD_CMD( minus ) GMP_NUMPAD_CMD( plus )
GMP_NUMPAD_CMD( enter ) GMP_NUMPAD_CMD( dot )
#endif // CLIENT_DLL
