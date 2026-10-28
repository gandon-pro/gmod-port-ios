//========= GModPort ==========================================================//
//
// Purpose: GMod's entity modifiers (duplicator.RegisterEntityModifier): "colour", "material"
//          and "trail". The tool gun sets them, the duplicator and saves copy them.
//
//          The material override travels in a gmp_material entity (target + material name);
//          C_BaseAnimating::DrawModel asks g_pfnGMPMaterialOverride (patch_gmp_sandbox.py).
//
//=============================================================================//

#ifndef CLIENT_DLL
#include "SpriteTrail.h"
#endif

#ifdef CLIENT_DLL
#define CGMPMaterial C_GMPMaterial
#endif

#define GMP_MATERIAL_LEN	128

// GMod's material tool refuses names that are not paths (and the screen-space ones it lists).
static bool GMP_ValidMaterialName( const char *name )
{
	if ( !name || !*name || Q_strlen( name ) >= GMP_MATERIAL_LEN || Q_strstr( name, ".." ) )
		return false;
	for ( const char *p = name; *p; p++ )
		if ( !V_isalnum( *p ) && *p != '_' && *p != '/' && *p != '-' && *p != '.' )
			return false;
	return Q_strnicmp( name, "pp/", 3 ) != 0;
}

class CGMPMaterial : public CBaseEntity
{
public:
	DECLARE_CLASS( CGMPMaterial, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CGMPMaterial( void ) { m_szMaterial.GetForModify()[0] = 0; }

	CNetworkHandle( CBaseEntity, m_hTarget );
	CNetworkString( m_szMaterial, GMP_MATERIAL_LEN );

#ifndef CLIENT_DLL
	void	Spawn( void ) { SetThink( &CGMPMaterial::WatchThink ); SetNextThink( gpGlobals->curtime + 1.0f ); }
	int		UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
	int		ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_DONT_SAVE; }
	void	WatchThink( void )
	{
		if ( !m_hTarget.Get() )
		{
			UTIL_Remove( this );
			return;
		}
		SetNextThink( gpGlobals->curtime + 1.0f );
	}
#else
	static CUtlVector<C_GMPMaterial *> s_List;
	IMaterial *m_pMaterial;
	char	m_szLoaded[GMP_MATERIAL_LEN];

	void	OnDataChanged( DataUpdateType_t type )
	{
		BaseClass::OnDataChanged( type );
		if ( type == DATA_UPDATE_CREATED )
		{
			m_pMaterial = NULL;
			m_szLoaded[0] = 0;
			s_List.AddToTail( this );
		}
		if ( Q_strcmp( m_szLoaded, m_szMaterial ) )
		{
			Q_strncpy( m_szLoaded, m_szMaterial, sizeof( m_szLoaded ) );
			m_pMaterial = GMP_ValidMaterialName( m_szLoaded ) ? materials->FindMaterial( m_szLoaded, TEXTURE_GROUP_MODEL ) : NULL;
			if ( m_pMaterial && m_pMaterial->IsErrorMaterial() )
				m_pMaterial = NULL;
		}
	}
	void	UpdateOnRemove( void ) { s_List.FindAndRemove( this ); BaseClass::UpdateOnRemove(); }
#endif
};

IMPLEMENT_NETWORKCLASS_ALIASED( GMPMaterial, DT_GMPMaterial )

BEGIN_NETWORK_TABLE( CGMPMaterial, DT_GMPMaterial )
#ifdef CLIENT_DLL
	RecvPropEHandle( RECVINFO( m_hTarget ) ),
	RecvPropString( RECVINFO( m_szMaterial ) ),
#else
	SendPropEHandle( SENDINFO( m_hTarget ) ),
	SendPropString( SENDINFO( m_szMaterial ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( gmp_material, CGMPMaterial );

#ifdef CLIENT_DLL
CUtlVector<C_GMPMaterial *> C_GMPMaterial::s_List;

extern IMaterial *( *g_pfnGMPMaterialOverride )( C_BaseAnimating *pEnt );

static IMaterial *GMP_MaterialOverride( C_BaseAnimating *pEnt )
{
	for ( int i = 0; i < C_GMPMaterial::s_List.Count(); i++ )
		if ( C_GMPMaterial::s_List[i]->m_hTarget.Get() == pEnt )
			return C_GMPMaterial::s_List[i]->m_pMaterial;
	return NULL;
}
static struct GMPMaterialHookInit { GMPMaterialHookInit() { g_pfnGMPMaterialOverride = GMP_MaterialOverride; } } s_GMPMaterialHookInit;

#else
//-----------------------------------------------------------------------------
// Server API
//-----------------------------------------------------------------------------
static CGMPMaterial *GMP_FindMaterialEnt( CBaseEntity *pEnt )
{
	for ( CBaseEntity *p = gEntList.FindEntityByClassname( NULL, "gmp_material" ); pEnt && p; p = gEntList.FindEntityByClassname( p, "gmp_material" ) )
	{
		CGMPMaterial *pMat = static_cast<CGMPMaterial *>( p );
		if ( pMat->m_hTarget.Get() == pEnt && !( p->GetEFlags() & EFL_KILLME ) )
			return pMat;
	}
	return NULL;
}

static const char *GMP_GetMaterial( CBaseEntity *pEnt )
{
	CGMPMaterial *pMat = GMP_FindMaterialEnt( pEnt );
	return pMat ? pMat->m_szMaterial.Get() : "";
}

// GMod's SetMaterial / the "material" modifier; "" clears it.
static void GMP_SetMaterial( CBaseEntity *pEnt, const char *material )
{
	if ( !pEnt || pEnt->IsWorld() )
		return;
	CGMPMaterial *pMat = GMP_FindMaterialEnt( pEnt );
	if ( !GMP_ValidMaterialName( material ) )
	{
		if ( pMat )
			UTIL_Remove( pMat );
		return;
	}
	if ( !pMat )
	{
		pMat = static_cast<CGMPMaterial *>( CreateEntityByName( "gmp_material" ) );
		if ( !pMat )
			return;
		pMat->m_hTarget = pEnt;
		DispatchSpawn( pMat );
	}
	Q_strncpy( pMat->m_szMaterial.GetForModify(), material, GMP_MATERIAL_LEN );
}

// The "colour" modifier: Color {r g b a}, RenderMode, RenderFX.
static void GMP_SetColour( CBaseEntity *pEnt, int r, int g, int b, int a, int mode, int fx )
{
	if ( !pEnt || pEnt->IsWorld() )
		return;
	pEnt->SetRenderColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), clamp( a, 0, 255 ) );
	pEnt->SetRenderMode( (RenderMode_t)clamp( mode, 0, (int)kRenderModeCount - 1 ) );
	pEnt->m_nRenderFX = clamp( fx, 0, (int)kRenderFxMax - 1 );
}

static bool GMP_HasColour( CBaseEntity *pEnt )
{
	const color32 c = pEnt->GetRenderColor();
	return c.r != 255 || c.g != 255 || c.b != 255 || c.a != 255 || pEnt->m_nRenderMode != kRenderNormal || pEnt->m_nRenderFX != kRenderFxNone;
}

static void GMP_ApplyColourMod( CBaseEntity *pEnt, KeyValues *pData )
{
	KeyValues *pColor = pData->FindKey( "Color" );
	GMP_SetColour( pEnt, pColor ? pColor->GetInt( "r", 255 ) : 255, pColor ? pColor->GetInt( "g", 255 ) : 255,
		pColor ? pColor->GetInt( "b", 255 ) : 255, pColor ? pColor->GetInt( "a", 255 ) : 255,
		pData->GetInt( "RenderMode" ), pData->GetInt( "RenderFX" ) );
}

static void GMP_WriteColourMod( CBaseEntity *pEnt, KeyValues *pData )
{
	const color32 c = pEnt->GetRenderColor();
	KeyValues *pColor = pData->FindKey( "Color", true );
	pColor->SetInt( "r", c.r );
	pColor->SetInt( "g", c.g );
	pColor->SetInt( "b", c.b );
	pColor->SetInt( "a", c.a );
	pData->SetInt( "RenderMode", pEnt->m_nRenderMode );
	pData->SetInt( "RenderFX", pEnt->m_nRenderFX );
}

// The "trail" modifier: Color {r g b a}, Length, StartSize, EndSize, Material.
struct GMPTrail_t
{
	EHANDLE ent, trail;
	KeyValues *pData;
};
static CUtlVector<GMPTrail_t> s_GMPTrails;

static int GMP_FindTrail( CBaseEntity *pEnt )
{
	for ( int i = s_GMPTrails.Count() - 1; i >= 0; i-- )
	{
		if ( !s_GMPTrails[i].ent.Get() || !s_GMPTrails[i].trail.Get() )
		{
			UTIL_Remove( s_GMPTrails[i].trail.Get() );
			s_GMPTrails[i].pData->deleteThis();
			s_GMPTrails.Remove( i );
			continue;
		}
		if ( s_GMPTrails[i].ent.Get() == pEnt )
			return i;
	}
	return -1;
}

static KeyValues *GMP_GetTrail( CBaseEntity *pEnt )
{
	const int i = GMP_FindTrail( pEnt );
	return i >= 0 ? s_GMPTrails[i].pData : NULL;
}

// pData NULL (or a zero size) clears the trail.
static void GMP_SetTrail( CBaseEntity *pEnt, KeyValues *pData )
{
	if ( !pEnt || pEnt->IsWorld() )
		return;
	const int old = GMP_FindTrail( pEnt );
	if ( old >= 0 )
	{
		UTIL_Remove( s_GMPTrails[old].trail.Get() );
		s_GMPTrails[old].pData->deleteThis();
		s_GMPTrails.Remove( old );
	}
	if ( !pData )
		return;
	const float startSize = clamp( pData->GetFloat( "StartSize", 32 ), 0.0f, 128.0f );
	const float endSize = clamp( pData->GetFloat( "EndSize", 0 ), 0.0f, 128.0f );
	const float length = clamp( pData->GetFloat( "Length", 5 ), 0.1f, 10.0f );
	const char *material = pData->GetString( "Material", "trails/lol" );
	if ( startSize <= 0 && endSize <= 0 )
		return;
	if ( !GMP_ValidMaterialName( material ) || !Q_strnicmp( material, "pp/", 3 ) )
		return;
	char vmt[MAX_PATH];
	Q_snprintf( vmt, sizeof( vmt ), "%s.vmt", material );
	CBaseEntity::PrecacheModel( vmt );
	CSpriteTrail *pTrail = CSpriteTrail::SpriteTrailCreate( vmt, pEnt->GetAbsOrigin(), false );
	if ( !pTrail )
		return;
	KeyValues *pColor = pData->FindKey( "Color" );
	pTrail->SetAttachment( pEnt, 0 );
	pTrail->SetTransparency( kRenderTransAlpha, pColor ? pColor->GetInt( "r", 255 ) : 255, pColor ? pColor->GetInt( "g", 255 ) : 255,
		pColor ? pColor->GetInt( "b", 255 ) : 255, pColor ? pColor->GetInt( "a", 255 ) : 255, kRenderFxNone );
	pTrail->SetStartWidth( startSize );
	pTrail->SetEndWidth( endSize );
	pTrail->SetLifeTime( length );
	pTrail->SetTextureResolution( 1.0f / ( ( startSize + endSize ) * 0.5f ) );
	GMPTrail_t &entry = s_GMPTrails[s_GMPTrails.AddToTail()];
	entry.ent = pEnt;
	entry.trail = pTrail;
	entry.pData = pData->MakeCopy();
	entry.pData->SetName( "trail" );
}
#endif // !CLIENT_DLL
