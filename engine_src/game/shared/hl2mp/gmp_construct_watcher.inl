// gm_construct single-player horror encounter (personal build): after a minute in the dark room a
// black citizen starts watching you from a rooftop where you look, more than 25 m away; aim at it
// or walk up and it moves to another roof. Five minutes later it runs at you through the walls
// and up onto roofs, faster than you, no noclip, screaming, and the game crashes. Single-player only.
#include "igamesystem.h"
#ifndef CLIENT_DLL
#include <cstdlib>
#include "ai_basenpc.h"
#include "recipientfilter.h"

static ConVar gmp_construct_watcher( "gmp_construct_watcher", "1", FCVAR_ARCHIVE,
	"gm_construct watcher, including its deliberate crash ending (single player)" );
static ConVar gmp_construct_watcher_delay( "gmp_construct_watcher_delay", "60", 0, "Seconds standing in gm_construct's dark room before the watcher appears" );
static ConVar gmp_construct_watcher_chase( "gmp_construct_watcher_chase", "300", 0, "Seconds of watching before it runs at you" );
static ConVar gmp_construct_watcher_crash( "gmp_construct_watcher_crash", "1", 0, "0: end the chase without crashing (testing)" );

#define GMP_WATCHER_MODEL	"models/Humans/Group01/male_07.mdl"
// Episode content (content_hl2); the phone gets it through gmcontent/npcpack (extract_npc_content.py).
#define GMP_WATCHER_SCREAM	"npc/stalker/stalker_scream1.wav"
#define GMP_WATCHER_SCREAM_LEN	2.16f	// seconds: replayed back to back for the whole chase
#define GMP_WATCHER_NEAR	1312.0f	// 25 m (1 unit = 1.905 cm): any closer and it vanishes to another roof

// gm_construct's dark room (env_soundscape "soundscape_inside_room_dark" at -4442 -1852 -136).
// Wall/floor/ceiling planes measured with brush traces from that point on the installed map.
static const Vector g_vecGMPDarkRoomMins( -5248, -2560, -144 );
static const Vector g_vecGMPDarkRoomMaxs( -3248, -1056, 160 );

static bool GMPInDarkRoom( const Vector &v )
{
	return v.x > g_vecGMPDarkRoomMins.x && v.x < g_vecGMPDarkRoomMaxs.x
		&& v.y > g_vecGMPDarkRoomMins.y && v.y < g_vecGMPDarkRoomMaxs.y
		&& v.z > g_vecGMPDarkRoomMins.z - 8 && v.z < g_vecGMPDarkRoomMaxs.z;
}

class CGMPConstructWatcher : public CAutoGameSystemPerFrame
{
public:
	EHANDLE m_hNPC;
	float m_flAliveSince, m_flSpawned, m_flNext, m_flRelocateAt, m_flLastMove, m_flScreamAt, m_flCaughtAt, m_flGazeSince, m_flAimedAt, m_flHiddenUntil;
	bool m_bArmed, m_bChasing, m_bFinished;

	CGMPConstructWatcher() : CAutoGameSystemPerFrame( "GMPConstructWatcher" ) { Reset(); }
	void Reset()
	{
		m_hNPC = NULL;
		m_flAliveSince = m_flGazeSince = -1;
		m_flSpawned = m_flNext = m_flRelocateAt = m_flLastMove = m_flScreamAt = m_flCaughtAt = m_flAimedAt = m_flHiddenUntil = 0;
		m_bArmed = m_bChasing = m_bFinished = false;
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

	// Next to a drop of a storey or more: the edge of a roof, not a slope of the hill.
	bool AtEdge( const Vector &spot )
	{
		for ( int a = 0; a < 360; a += 45 )
		{
			Vector dir;
			AngleVectors( QAngle( 0, a, 0 ), &dir );
			const Vector probe = spot + dir * 96 + Vector( 0, 0, 32 );
			trace_t down;
			UTIL_TraceLine( probe, probe - Vector( 0, 0, 192 ), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &down );
			if ( !down.startsolid && down.fraction > 0.7f )	// nothing within 100 units below
				return true;
		}
		return false;
	}

	// A rooftop the player is looking at, a little off centre so it can go unnoticed: the topmost
	// surface under the sky, well above the player's feet and at a roof edge.
	bool FindSpot( CBasePlayer *p, Vector &spot, const Vector *from = NULL )
	{
		trace_t sky;
		UTIL_TraceLine( p->EyePosition(), p->EyePosition() + Vector( 0, 0, 8192 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &sky );
		if ( sky.DidHit() && !( sky.surface.flags & SURF_SKY ) )	// gm_construct has nothing at all above the open map
			return false;	// indoors: no roofs in view
		const float top = sky.endpos.z - 8;
		for ( int i = 0; i < 64; i++ )
		{
			const float side = RandomFloat( 6, 35 ) * ( RandomInt( 0, 1 ) ? 1 : -1 );
			Vector dir;
			AngleVectors( QAngle( 0, p->EyeAngles().y + side, 0 ), &dir );
			const Vector column = p->EyePosition() + dir * RandomFloat( GMP_WATCHER_NEAR + 140, GMP_WATCHER_NEAR + 1300 );
			trace_t floor, hull;
			UTIL_TraceLine( Vector( column.x, column.y, top ), Vector( column.x, column.y, top - 8192 ), MASK_SOLID_BRUSHONLY, p, COLLISION_GROUP_NONE, &floor );
			if ( !floor.DidHit() || floor.startsolid || floor.plane.normal.z < 0.9f || ( floor.surface.flags & ( SURF_SKY | SURF_NODRAW ) ) )
				continue;	// outside the map, a slope, or the sky box
			spot = floor.endpos + Vector( 0, 0, 1 );
			// A building's roof, not the top of the map's boundary walls (z ~2850 on gm_construct).
			if ( spot.z < p->GetAbsOrigin().z + 128 || spot.z > p->GetAbsOrigin().z + 1600 || !AtEdge( spot ) )
				continue;
			if ( from && SameRoof( *from, spot ) )	// never back on the roof it was seen on
				continue;
			// Never inside a wall or a prop: a full standing hull, world and props alike.
			UTIL_TraceHull( spot, spot, Vector( -16, -16, 2 ), Vector( 16, 16, 72 ), MASK_NPCSOLID, p, COLLISION_GROUP_NONE, &hull );
			if ( hull.startsolid || hull.allsolid || ( enginetrace->GetPointContents( spot + Vector( 0, 0, 36 ) ) & MASK_SOLID ) || !Visible( p, spot ) )
				continue;
			return true;
		}
		return false;
	}

	// One roof: the same height and either near (roofs have skylights and trenches) or roof
	// footing under most of the line between the two spots.
	bool SameRoof( const Vector &a, const Vector &b )
	{
		if ( fabsf( a.z - b.z ) > 24 )
			return false;
		if ( ( a - b ).Length2D() < 1000 )
			return true;
		int footing = 0;
		for ( int s = 1; s < 16; s++ )
		{
			const Vector probe = a + ( b - a ) * ( s / 16.0f ) + Vector( 0, 0, 16 );
			trace_t down;
			UTIL_TraceLine( probe, probe - Vector( 0, 0, 64 ), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &down );
			if ( !down.startsolid && down.DidHit() )
				footing++;
		}
		return footing >= 12;	// a street between them leaves most probes without footing
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
		// Body size plus ~4 degrees: at 400-800 units the bare body is a fraction of a degree,
		// too thin to hold with touch aiming (it only worked up close).
		const float slack = t * 0.07f;
		if ( ( closest - centre ).Length2D() > 20 + slack || fabsf( closest.z - centre.z ) > 38 + slack )
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

	// Keep its feet on the floor while it moves through walls (never sinks under the map); off a
	// roof edge it drops at falling speed.
	Vector OnFloor( const Vector &pos, float lastZ, float dt )
	{
		trace_t floor;
		UTIL_TraceLine( pos + Vector( 0, 0, 48 ), pos - Vector( 0, 0, 4096 ), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &floor );
		if ( floor.DidHit() && !floor.startsolid && floor.plane.normal.z >= 0.7f )
			return Vector( pos.x, pos.y, Max( floor.endpos.z + 1, lastZ - 900 * dt ) );
		return Vector( pos.x, pos.y, lastZ );	// inside a wall: hold its height
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
			// A minute without a break in the dark room arms it; leaving early starts the count over.
			if ( !m_bArmed )
			{
				if ( !GMPInDarkRoom( p->GetAbsOrigin() ) )
					m_flAliveSince = -1;
				else if ( m_flAliveSince < 0 )
					m_flAliveSince = now;
				else if ( now - m_flAliveSince >= gmp_construct_watcher_delay.GetFloat() )
				{
					m_bArmed = true;
					DevMsg( "GModPort watcher: armed in the dark room\n" );
				}
				return;
			}
			if ( now < m_flNext )
				return;
			m_flNext = now + 0.5f;	// it shows up on a roof once the player is outside looking at one
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
				const Vector old = n->GetAbsOrigin();
				if ( FindSpot( p, spot, &old ) )
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
			else if ( ( n->GetAbsOrigin() - p->GetAbsOrigin() ).Length2D() < GMP_WATCHER_NEAR )
			{
				Show( n, false );	// walked up within 25 m: gone, then on another roof
				m_flHiddenUntil = now + RandomFloat( 4, 10 );
				DevMsg( "GModPort watcher: approached, vanished\n" );
			}
			else if ( !AimedAt( p, n->GetAbsOrigin() ) )
			{
				if ( now - m_flAimedAt > 0.6f )	// brief slips off it keep the 5 s count
					m_flGazeSince = -1;
			}
			else if ( ( m_flAimedAt = now ), m_flGazeSince < 0 )
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
				DevMsg( "GModPort watcher: chase from %.0f %.0f %.0f\n", n->GetAbsOrigin().x, n->GetAbsOrigin().y, n->GetAbsOrigin().z );
			}
			return;
		}

		if ( now >= m_flScreamAt )
		{
			Scream( p, n );
			m_flScreamAt = now + GMP_WATCHER_SCREAM_LEN;
		}
		// No noclip escape while it runs (console, bind or touch button alike).
		if ( p->GetMoveType() == MOVETYPE_NOCLIP )
			p->SetMoveType( MOVETYPE_WALK );
		// Straight at the player through anything, always faster than the player (sprint included)
		// and a bit faster every second.
		Animate( n, ACT_RUN, 1.6f );
		const Vector target = p->GetAbsOrigin(), from = n->GetAbsOrigin();
		Vector flat( target.x - from.x, target.y - from.y, 0 );
		const float flatDist = VectorNormalize( flat );
		const float dist = ( target - from ).Length();
		const float speed = Max( 500.0f, p->GetAbsVelocity().Length() * 1.3f ) + 40 * ( now - m_flSpawned - gmp_construct_watcher_chase.GetFloat() );
		const float step = speed * dt;
		// The player is up on a roof: climb the wall in front straight up, then run on over the top.
		trace_t wall;
		UTIL_TraceLine( from + Vector( 0, 0, 8 ), from + Vector( 0, 0, 8 ) + flat * 32, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &wall );
		if ( target.z > from.z + 40 && flatDist > 24 && ( wall.startsolid || ( wall.fraction < 1 && wall.plane.normal.z < 0.7f ) ) )
			n->SetAbsOrigin( from + Vector( 0, 0, Min( step, target.z - from.z ) ) );
		else
			n->SetAbsOrigin( OnFloor( flatDist <= step ? Vector( target.x, target.y, from.z ) : from + flat * step, from.z, dt ) );
		Face( n, target );
		if ( !m_flCaughtAt && dist < 48 )
		{
			m_flCaughtAt = now;
			DevMsg( "GModPort watcher: caught at %.0f %.0f %.0f\n", n->GetAbsOrigin().x, n->GetAbsOrigin().y, n->GetAbsOrigin().z );
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
