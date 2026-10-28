// gm_construct single-player horror encounter (personal build): stand in a dark room for a minute
// and a black citizen starts watching you from wherever you look. Five minutes later it runs at
// you through the walls, screaming, and the game crashes. Single-player gm_construct only.
#include "igamesystem.h"
#ifdef CLIENT_DLL
static bool GMPWatcherMap()
{
	char map[MAX_PATH];
	Q_FileBase( engine->GetLevelName(), map, sizeof( map ) );
	return !Q_stricmp( map, "gm_construct" );
}

// Baked lighting at the eyes, twice a second. The server has no lighting query, so the client
// reports it (gmp_watcher_sample). Client game systems get Update, not FrameUpdate*Think.
class CGMPWatcherLight : public CAutoGameSystemPerFrame
{
	float m_flNext;
public:
	CGMPWatcherLight() : CAutoGameSystemPerFrame( "GMPWatcherLight" ), m_flNext( 0 ) {}
	virtual void LevelShutdownPreEntity() { m_flNext = 0; }
	virtual void Update( float frametime )
	{
		// The server decides whether this is a lone listen-server host (hl2mp runs maxplayers 2).
		if ( !engine->IsInGame() || !GMPWatcherMap() || gpGlobals->curtime < m_flNext )
			return;
		m_flNext = gpGlobals->curtime + 0.5f;
		C_BasePlayer *p = C_BasePlayer::GetLocalPlayer();
		if ( !p )
			return;
		const Vector light = engine->GetLightForPoint( p->EyePosition(), true );
		const bool dark = p->IsAlive() && light.x < 0.06f && light.y < 0.06f && light.z < 0.06f;
		DevMsg( 2, "GModPort watcher light: %.3f %.3f %.3f\n", light.x, light.y, light.z );
		engine->ServerCmd( dark ? "gmp_watcher_sample 1\n" : "gmp_watcher_sample 0\n" );
	}
};
static CGMPWatcherLight g_GMPWatcherLight;
#else
#include <cstdlib>
#include "ai_basenpc.h"
#include "recipientfilter.h"

static ConVar gmp_construct_watcher( "gmp_construct_watcher", "1", FCVAR_ARCHIVE,
	"gm_construct watcher, including its deliberate crash ending (single player)" );
static ConVar gmp_construct_watcher_dark( "gmp_construct_watcher_dark", "60", 0, "Seconds in the dark before the watcher appears" );
static ConVar gmp_construct_watcher_chase( "gmp_construct_watcher_chase", "300", 0, "Seconds of watching before it runs at you" );
static ConVar gmp_construct_watcher_crash( "gmp_construct_watcher_crash", "1", 0, "0: end the chase without crashing (testing)" );
static ConVar gmp_construct_watcher_force( "gmp_construct_watcher_force", "0", FCVAR_CHEAT, "Count any indoor spot as dark (testing in light)" );

#define GMP_WATCHER_MODEL	"models/Humans/Group01/male_07.mdl"
#define GMP_WATCHER_SCREAM	"ambient/voices/citizen_beaten3.wav"

class CGMPConstructWatcher : public CAutoGameSystemPerFrame
{
public:
	EHANDLE m_hNPC;
	float m_flDarkSince, m_flLastSample, m_flSpawned, m_flNext, m_flRelocateAt, m_flLastMove, m_flScreamAt, m_flCaughtAt;
	bool m_bDark, m_bChasing, m_bFinished;

	CGMPConstructWatcher() : CAutoGameSystemPerFrame( "GMPConstructWatcher" ) { Reset(); }
	void Reset()
	{
		m_hNPC = NULL;
		m_flDarkSince = m_flLastSample = -1;
		m_flSpawned = m_flNext = m_flRelocateAt = m_flLastMove = m_flScreamAt = m_flCaughtAt = 0;
		m_bDark = m_bChasing = m_bFinished = false;
	}
	virtual void LevelInitPostEntity() { Reset(); }
	virtual void LevelShutdownPreEntity() { Reset(); }

	// Listen server with only its host connected (hl2mp's default maxplayers is 2, so not maxClients).
	bool Allowed()
	{
		if ( !gmp_construct_watcher.GetBool() || engine->IsDedicatedServer() || Q_stricmp( STRING( gpGlobals->mapname ), "gm_construct" ) )
			return false;
		int players = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *p = UTIL_PlayerByIndex( i );
			if ( p && p->IsConnected() )
				players++;
		}
		return players == 1 && UTIL_GetListenServerHost();
	}

	// Dark and indoors (a non-sky ceiling): the dark room, not a shadow outside.
	void Sample( CBasePlayer *p, bool dark )
	{
		if ( !Allowed() || p != UTIL_GetListenServerHost() )
			return;
		trace_t roof;
		UTIL_TraceLine( p->EyePosition(), p->EyePosition() + Vector( 0, 0, 2048 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &roof );
		m_bDark = ( dark || gmp_construct_watcher_force.GetBool() ) && roof.DidHit() && !( roof.surface.flags & SURF_SKY );
		m_flLastSample = gpGlobals->curtime;
		DevMsg( 2, "GModPort watcher sample: %d, indoors %d\n", dark, roof.DidHit() && !( roof.surface.flags & SURF_SKY ) );
		if ( !m_bDark )
			m_flDarkSince = -1;
		else if ( m_flDarkSince < 0 )
			m_flDarkSince = m_flLastSample;
	}

	// In the view cone (about 45 degrees off centre) with nothing solid in between.
	bool Visible( CBasePlayer *p, const Vector &feet )
	{
		Vector forward, delta = feet + Vector( 0, 0, 60 ) - p->EyePosition();
		p->EyeVectors( &forward );
		VectorNormalize( delta );
		if ( DotProduct( forward, delta ) < 0.7f )
			return false;
		trace_t sight;
		UTIL_TraceLine( p->EyePosition(), feet + Vector( 0, 0, 60 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &sight );
		return sight.fraction > 0.99f;
	}

	// A floor spot where the player is looking, a little off centre so it can go unnoticed.
	bool FindSpot( CBasePlayer *p, Vector &spot )
	{
		for ( int i = 0; i < 32; i++ )
		{
			const float side = RandomFloat( 10, 32 ) * ( RandomInt( 0, 1 ) ? 1 : -1 );
			Vector dir;
			AngleVectors( QAngle( 0, p->EyeAngles().y + side, 0 ), &dir );
			trace_t ahead, floor, hull;
			UTIL_TraceLine( p->EyePosition(), p->EyePosition() + dir * RandomFloat( 250, 1000 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &ahead );
			const Vector stand = ahead.endpos - dir * 40;
			if ( ( stand - p->EyePosition() ).Length2D() < 160 )
				continue;
			UTIL_TraceLine( stand, stand - Vector( 0, 0, 512 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &floor );
			if ( !floor.DidHit() || floor.startsolid || floor.plane.normal.z < 0.7f )
				continue;
			spot = floor.endpos + Vector( 0, 0, 1 );
			UTIL_TraceHull( spot, spot, Vector( -13, -13, 0 ), Vector( 13, 13, 72 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &hull );
			if ( hull.startsolid || !Visible( p, spot ) )
				continue;
			return true;
		}
		return false;
	}

	void Animate( CAI_BaseNPC *n, Activity act, float rate )
	{
		const int seq = n->SelectWeightedSequence( act );
		if ( seq >= 0 && n->GetSequence() != seq )
			n->ResetSequence( seq );
		n->SetPlaybackRate( rate );
	}

	void Face( CAI_BaseNPC *n, const Vector &target )
	{
		QAngle face;
		VectorAngles( target - n->GetAbsOrigin(), face );
		n->SetAbsAngles( QAngle( 0, face.y, 0 ) );
	}

	// npc_citizen with its AI switched off: this drives the pose and the position itself, and
	// it is not solid, so it stands in and runs through walls.
	bool Spawn( CBasePlayer *p )
	{
		Vector spot;
		if ( !FindSpot( p, spot ) )
			return false;
		GMPNPCPrecacheScope precache;	// gmp_npc_server.inl: spawned mid-game
		CBaseEntity::PrecacheModel( GMP_WATCHER_MODEL );
		CBaseEntity::PrecacheSound( GMP_WATCHER_SCREAM );
		CAI_BaseNPC *n = dynamic_cast<CAI_BaseNPC *>( CreateEntityByName( "npc_citizen" ) );
		if ( !n )
			return false;
		n->KeyValue( "model", GMP_WATCHER_MODEL );
		n->AddSpawnFlags( SF_NPC_GAG | ( 1 << 20 ) );	// silent, not commandable (SF_CITIZEN_NOT_COMMANDABLE)
		n->SetAbsOrigin( spot );
		if ( DispatchSpawn( n ) < 0 )
		{
			UTIL_Remove( n );
			return false;
		}
		n->Activate();
		n->AddEFlags( EFL_NO_THINK_FUNCTION );
		n->SetMoveType( MOVETYPE_NONE );
		n->AddSolidFlags( FSOLID_NOT_SOLID );
		n->m_takedamage = DAMAGE_NO;
		n->SetRenderColor( 0, 0, 0 );
		Face( n, p->GetAbsOrigin() );
		Animate( n, ACT_IDLE, 1 );
		m_hNPC = n;
		m_flSpawned = m_flLastMove = gpGlobals->curtime;
		m_flRelocateAt = m_flSpawned + RandomFloat( 15, 30 );
		DevMsg( "GModPort watcher: spawned at %.0f %.0f %.0f\n", spot.x, spot.y, spot.z );
		return true;
	}

	void Scream( CBasePlayer *p, CBaseEntity *n )
	{
		CSingleUserRecipientFilter filter( p );
		EmitSound_t ep;
		ep.m_nChannel = CHAN_VOICE;
		ep.m_pSoundName = GMP_WATCHER_SCREAM;
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_140dB;
		CBaseEntity::EmitSound( filter, n->entindex(), ep );
	}

	virtual void FrameUpdatePostEntityThink()
	{
		if ( m_bFinished )
			return;
		CBasePlayer *p = UTIL_GetListenServerHost();
		if ( !Allowed() || !p || !p->IsAlive() )
		{
			if ( m_hNPC )
				UTIL_Remove( m_hNPC );
			Reset();
			return;
		}
		const float now = gpGlobals->curtime;
		CAI_BaseNPC *n = m_hNPC ? m_hNPC->MyNPCPointer() : NULL;
		if ( !n )
		{
			if ( m_flSpawned > 0 )	// removed (cleanup / remover): it may come back after another minute in the dark
				Reset();
			if ( now < m_flNext )
				return;
			m_flNext = now + 0.5f;
			if ( m_flLastSample < 0 || now - m_flLastSample > 1.5f )
				m_flDarkSince = -1;
			if ( m_bDark && m_flDarkSince >= 0 && now - m_flDarkSince > gmp_construct_watcher_dark.GetFloat() )
				Spawn( p );
			return;
		}
		n->StudioFrameAdvance();
		const float dt = clamp( now - m_flLastMove, 0.0f, 0.1f );
		m_flLastMove = now;

		if ( !m_bChasing )
		{
			Face( n, p->GetAbsOrigin() );
			// Look away and it is somewhere else you are looking.
			if ( now >= m_flRelocateAt && !Visible( p, n->GetAbsOrigin() ) )
			{
				Vector spot;
				if ( FindSpot( p, spot ) )
					n->SetAbsOrigin( spot );
				m_flRelocateAt = now + RandomFloat( 15, 30 );
			}
			if ( now - m_flSpawned >= gmp_construct_watcher_chase.GetFloat() )
			{
				m_bChasing = true;
				m_flScreamAt = now;
				DevMsg( "GModPort watcher: chase\n" );
			}
			return;
		}

		if ( now >= m_flScreamAt )
		{
			Scream( p, n );
			m_flScreamAt = now + 2.5f;
		}
		// Straight at the player through anything, a bit faster every second.
		Animate( n, ACT_RUN, 1.6f );
		const Vector target = p->GetAbsOrigin();
		Vector delta = target - n->GetAbsOrigin();
		const float dist = VectorNormalize( delta );
		const float speed = 420 + 40 * ( now - m_flSpawned - gmp_construct_watcher_chase.GetFloat() );
		n->SetAbsOrigin( dist <= speed * dt ? target : n->GetAbsOrigin() + delta * speed * dt );
		Face( n, target );
		if ( !m_flCaughtAt && dist < 48 )
		{
			m_flCaughtAt = now;
			DevMsg( "GModPort watcher: caught\n" );
		}
		if ( m_flCaughtAt && now - m_flCaughtAt > 0.4f )
		{
			m_bFinished = true;
			if ( gmp_construct_watcher_crash.GetBool() )
				std::abort();	// the ending: a real crash of this (single-player) process
			UTIL_Remove( n );
		}
	}
};
static CGMPConstructWatcher g_GMPConstructWatcher;

CON_COMMAND( gmp_watcher_sample, "Lighting sample for the gm_construct watcher (sent by the client)" )
{
	CBasePlayer *p = UTIL_GetCommandClient();
	if ( p && args.ArgC() == 2 )
		g_GMPConstructWatcher.Sample( p, !Q_strcmp( args[1], "1" ) );
}
#endif
