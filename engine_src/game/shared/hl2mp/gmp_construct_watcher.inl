// gm_construct single-player horror encounter (personal build): a minute after the map loads a
// black citizen starts watching you from wherever you look, anywhere on the map. Five minutes
// later it runs at you through the walls, screaming, and the game crashes. Single-player only.
#include "igamesystem.h"
#ifndef CLIENT_DLL
#include <cstdlib>
#include "ai_basenpc.h"
#include "recipientfilter.h"

static ConVar gmp_construct_watcher( "gmp_construct_watcher", "1", FCVAR_ARCHIVE,
	"gm_construct watcher, including its deliberate crash ending (single player)" );
static ConVar gmp_construct_watcher_delay( "gmp_construct_watcher_delay", "60", 0, "Seconds alive on gm_construct before the watcher appears" );
static ConVar gmp_construct_watcher_chase( "gmp_construct_watcher_chase", "300", 0, "Seconds of watching before it runs at you" );
static ConVar gmp_construct_watcher_crash( "gmp_construct_watcher_crash", "1", 0, "0: end the chase without crashing (testing)" );

#define GMP_WATCHER_MODEL	"models/Humans/Group01/male_07.mdl"
// Episode content (content_hl2); the phone gets it through gmcontent/npcpack (extract_npc_content.py).
#define GMP_WATCHER_SCREAM	"npc/stalker/stalker_scream1.wav"
#define GMP_WATCHER_SCREAM_LEN	2.16f	// seconds: replayed back to back for the whole chase

class CGMPConstructWatcher : public CAutoGameSystemPerFrame
{
public:
	EHANDLE m_hNPC;
	float m_flAliveSince, m_flSpawned, m_flNext, m_flRelocateAt, m_flLastMove, m_flScreamAt, m_flCaughtAt, m_flGazeSince, m_flHiddenUntil;
	bool m_bChasing, m_bFinished;

	CGMPConstructWatcher() : CAutoGameSystemPerFrame( "GMPConstructWatcher" ) { Reset(); }
	void Reset()
	{
		m_hNPC = NULL;
		m_flAliveSince = m_flGazeSince = -1;
		m_flSpawned = m_flNext = m_flRelocateAt = m_flLastMove = m_flScreamAt = m_flCaughtAt = m_flHiddenUntil = 0;
		m_bChasing = m_bFinished = false;
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

	// In the view cone (cone = cos of the angle off centre: 0.7 about 45 degrees, 0.94 about 20,
	// i.e. looked at) with nothing solid in between.
	bool Visible( CBasePlayer *p, const Vector &feet, float cone = 0.7f )
	{
		Vector forward, delta = feet + Vector( 0, 0, 60 ) - p->EyePosition();
		p->EyeVectors( &forward );
		VectorNormalize( delta );
		if ( DotProduct( forward, delta ) < cone )
			return false;
		trace_t sight;
		UTIL_TraceLine( p->EyePosition(), feet + Vector( 0, 0, 60 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &sight );
		return sight.fraction > 0.99f;
	}

	// A floor spot where the player is looking, a little off centre so it can go unnoticed.
	bool FindSpot( CBasePlayer *p, Vector &spot, const Vector *from = NULL )
	{
		for ( int i = 0; i < 32; i++ )
		{
			const float side = RandomFloat( 10, 32 ) * ( RandomInt( 0, 1 ) ? 1 : -1 );
			Vector dir;
			AngleVectors( QAngle( 0, p->EyeAngles().y + side, 0 ), &dir );
			trace_t ahead, floor, hull;
			UTIL_TraceLine( p->EyePosition(), p->EyePosition() + dir * RandomFloat( 420, 900 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &ahead );
			const Vector stand = ahead.endpos - dir * 40;
			const float dist = ( stand - p->EyePosition() ).Length2D();
			if ( dist < 350 || dist > 850 )	// close enough to make out, far enough to miss
				continue;
			UTIL_TraceLine( stand, stand - Vector( 0, 0, 512 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &floor );
			if ( !floor.DidHit() || floor.startsolid || floor.plane.normal.z < 0.7f )
				continue;
			spot = floor.endpos + Vector( 0, 0, 1 );
			if ( from && ( spot - *from ).Length2D() < 400 )
				continue;
			// Never inside a wall or a prop: a full standing hull, world and props alike.
			UTIL_TraceHull( spot, spot, Vector( -16, -16, 2 ), Vector( 16, 16, 72 ), MASK_NPCSOLID, p, COLLISION_GROUP_NONE, &hull );
			if ( hull.startsolid || hull.allsolid || ( enginetrace->GetPointContents( spot + Vector( 0, 0, 36 ) ) & MASK_SOLID ) || !Visible( p, spot ) )
				continue;
			return true;
		}
		return false;
	}

	// The crosshair is on its body (the aim ray passes within its hull), with a clear line.
	bool AimedAt( CBasePlayer *p, const Vector &feet )
	{
		Vector forward;
		p->EyeVectors( &forward );
		const Vector centre = feet + Vector( 0, 0, 36 );
		const float t = DotProduct( centre - p->EyePosition(), forward );
		if ( t <= 0 )
			return false;
		const Vector closest = p->EyePosition() + forward * t;
		if ( ( closest - centre ).Length2D() > 20 || fabsf( closest.z - centre.z ) > 38 )
			return false;
		trace_t sight;
		UTIL_TraceLine( p->EyePosition(), closest, MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &sight );
		return sight.fraction > 0.99f;
	}

	void Show( CAI_BaseNPC *n, bool show )
	{
		if ( show )
		{
			n->RemoveEffects( EF_NODRAW );
			n->AddEffects( EF_DIMLIGHT );
		}
		else
		{
			n->AddEffects( EF_NODRAW );
			n->RemoveEffects( EF_DIMLIGHT );
		}
		m_flHiddenUntil = 0;
		m_flGazeSince = -1;
	}

	// Keep its feet on the floor while it moves through walls (never sinks under the map).
	Vector OnFloor( const Vector &pos, float lastZ )
	{
		trace_t floor;
		UTIL_TraceLine( pos + Vector( 0, 0, 48 ), pos - Vector( 0, 0, 96 ), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &floor );
		if ( floor.DidHit() && !floor.startsolid && floor.plane.normal.z >= 0.7f )
			return floor.endpos + Vector( 0, 0, 1 );
		return Vector( pos.x, pos.y, lastZ );	// inside a wall or over a gap: hold its height
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
		n->AddEffects( EF_DIMLIGHT );	// a dim pool of light at its feet: a black figure in a pitch-black room is otherwise invisible
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
			if ( m_flSpawned > 0 )	// removed (cleanup / remover): it comes back after another delay
				Reset();
			if ( m_flAliveSince < 0 )
				m_flAliveSince = now;
			if ( now < m_flNext )
				return;
			m_flNext = now + 0.5f;	// no free spot in view yet: try again shortly
			if ( now - m_flAliveSince > gmp_construct_watcher_delay.GetFloat() )
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
			if ( m_flHiddenUntil <= 0 && now >= m_flRelocateAt && !Visible( p, n->GetAbsOrigin() ) )
			{
				Vector spot;
				if ( FindSpot( p, spot ) )
					n->SetAbsOrigin( spot );
				m_flRelocateAt = now + RandomFloat( 15, 30 );
			}
			// Keep the crosshair on it for 5 seconds and it vanishes, to turn up somewhere else.
			if ( m_flHiddenUntil > 0 )
			{
				Vector spot;
				const Vector old = n->GetAbsOrigin();
				if ( now >= m_flHiddenUntil && FindSpot( p, spot, &old ) )
				{
					n->SetAbsOrigin( spot );
					Show( n, true );
					m_flRelocateAt = now + RandomFloat( 15, 30 );
					DevMsg( "GModPort watcher: back at %.0f %.0f %.0f\n", spot.x, spot.y, spot.z );
				}
			}
			else if ( !AimedAt( p, n->GetAbsOrigin() ) )
				m_flGazeSince = -1;
			else if ( m_flGazeSince < 0 )
				m_flGazeSince = now;
			else if ( now - m_flGazeSince > 5 )
			{
				Show( n, false );
				m_flHiddenUntil = now + RandomFloat( 4, 10 );
				DevMsg( "GModPort watcher: aimed at, vanished\n" );
			}
			if ( now - m_flSpawned >= gmp_construct_watcher_chase.GetFloat() )
			{
				if ( m_flHiddenUntil > 0 )
				{
					Vector spot;
					if ( FindSpot( p, spot ) )
						n->SetAbsOrigin( spot );
					Show( n, true );	// the chase starts from wherever it is
				}
				m_bChasing = true;
				m_flScreamAt = now;
				DevMsg( "GModPort watcher: chase\n" );
			}
			return;
		}

		if ( now >= m_flScreamAt )
		{
			Scream( p, n );
			m_flScreamAt = now + GMP_WATCHER_SCREAM_LEN;
		}
		// Straight at the player through anything, a bit faster every second.
		Animate( n, ACT_RUN, 1.6f );
		const Vector target = p->GetAbsOrigin();
		Vector delta = target - n->GetAbsOrigin();
		const float dist = VectorNormalize( delta );
		const float speed = 420 + 40 * ( now - m_flSpawned - gmp_construct_watcher_chase.GetFloat() );
		n->SetAbsOrigin( OnFloor( dist <= speed * dt ? target : n->GetAbsOrigin() + delta * speed * dt, n->GetAbsOrigin().z ) );
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
#endif
