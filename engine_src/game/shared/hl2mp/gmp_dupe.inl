//========= GModPort ==========================================================//
//
// Purpose: GMod's duplicator (copy / paste, dupes/*.dupe), saves (saves/*.gms) and the vehicles
//          of the spawn menu's Vehicles tab.
//
//          A dupe is GMod's table: Entities { <index> { Class, Model, Skin, Pos, Angle,
//          PhysicsObjects { <bone> { Pos, Angle, Frozen } }, EntityMods { colour, material, trail },
//          the SENT's own fields } }, Constraints { <n> { Type, Entity { <1..4> { Index, World,
//          Bone } }, the constraint's fields } }, Mins, Maxs. Positions are in the dupe's frame:
//          the bottom centre of the copied contraption, turned to the copier's view yaw.
//
//          Saves hold the same tables in world space plus the map and the player; they are this
//          port's own (GMod's .gms is an engine save game) and only load on the map they were made on.
//
//=============================================================================//

struct GMPVehicleDef
{
	const char *name, *category, *classname, *model, *script;
};
// GMod's list.Get( "Vehicles" ) for the HL2 vehicles this game dll has (no jalopy: episodic).
static const GMPVehicleDef s_GMPVehicles[] = {
	{ "Jeep", "Half-Life 2", "prop_vehicle_jeep", "models/buggy.mdl", "scripts/vehicles/jeep_test.txt" },
	{ "Airboat", "Half-Life 2", "prop_vehicle_airboat", "models/airboat.mdl", "scripts/vehicles/airboat.txt" },
	{ "Pod", "Half-Life 2", "prop_vehicle_prisoner_pod", "models/vehicles/prisoner_pod_inner.mdl", "scripts/vehicles/prisoner_pod.txt" },
	{ "Jeep Seat", "Chairs", "prop_vehicle_prisoner_pod", "models/nova/jeep_seat.mdl", "scripts/vehicles/prisoner_pod.txt" },
	{ "Airboat Seat", "Chairs", "prop_vehicle_prisoner_pod", "models/nova/airboat_seat.mdl", "scripts/vehicles/prisoner_pod.txt" },
	{ "Wooden Chair", "Chairs", "prop_vehicle_prisoner_pod", "models/nova/chair_wood01.mdl", "scripts/vehicles/prisoner_pod.txt" },
	{ "Chair", "Chairs", "prop_vehicle_prisoner_pod", "models/nova/chair_plastic01.mdl", "scripts/vehicles/prisoner_pod.txt" },
	{ "Office Chair", "Chairs", "prop_vehicle_prisoner_pod", "models/nova/chair_office01.mdl", "scripts/vehicles/prisoner_pod.txt" },
	{ "Big Office Chair", "Chairs", "prop_vehicle_prisoner_pod", "models/nova/chair_office02.mdl", "scripts/vehicles/prisoner_pod.txt" },
};

// A file name the dupe / save commands accept: letters, digits, _ - and spaces.
static bool GMP_ValidFileName( const char *name )
{
	if ( !name || !*name || Q_strlen( name ) > 64 )
		return false;
	for ( const char *p = name; *p; p++ )
		if ( !V_isalnum( *p ) && *p != '_' && *p != '-' && *p != ' ' )
			return false;
	return true;
}

#ifndef CLIENT_DLL
#include <time.h>

//-----------------------------------------------------------------------------
// Vehicles
//-----------------------------------------------------------------------------
static int GMP_VehicleByModel( const char *model )
{
	for ( int i = 0; model && i < ARRAYSIZE( s_GMPVehicles ); i++ )
		if ( !Q_stricmp( model, s_GMPVehicles[i].model ) )
			return i;
	return -1;
}

static CBaseEntity *GMP_MakeVehicle( const char *classname, const char *model, const char *script, const Vector &pos, const QAngle &ang )
{
	if ( Q_strnicmp( classname, "prop_vehicle_", 13 ) || !filesystem->FileExists( model, "GAME" ) || !filesystem->FileExists( script, "GAME" ) )
		return NULL;
	GMPNPCPrecacheScope precache;
	CBaseEntity *pEnt = CreateEntityByName( classname );
	if ( !pEnt )
		return NULL;
	pEnt->KeyValue( "model", model );
	pEnt->KeyValue( "vehiclescript", script );
	pEnt->SetAbsOrigin( pos );
	pEnt->SetAbsAngles( ang );
	if ( DispatchSpawn( pEnt ) < 0 )
	{
		UTIL_Remove( pEnt );
		return NULL;
	}
	pEnt->Activate();
	return pEnt;
}

// gmp_vehicle <name>: the Vehicles tab; on the surface you look at, facing you (GMod's Spawn_Vehicle).
CON_COMMAND( gmp_vehicle, "GModPort: gmp_vehicle <name> - spawn a vehicle from the Vehicles tab" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || !pPlayer->IsAlive() || args.ArgC() < 2 )
		return;
	const GMPVehicleDef *def = NULL;
	for ( int i = 0; i < ARRAYSIZE( s_GMPVehicles ); i++ )
		if ( !Q_stricmp( args.ArgS(), s_GMPVehicles[i].name ) )
			def = &s_GMPVehicles[i];
	if ( !def )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Unknown vehicle.\n" );
		return;
	}
	Vector start = pPlayer->EyePosition(), forward;
	pPlayer->EyeVectors( &forward );
	trace_t tr;
	UTIL_TraceLine( start, start + forward * 2048, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction >= 1.0f )
		return;
	CBaseEntity *pEnt = GMP_MakeVehicle( def->classname, def->model, def->script, tr.endpos, QAngle( 0, pPlayer->EyeAngles().y + 180, 0 ) );
	if ( !pEnt )
	{
		ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Could not spawn %s (%s missing?)\n", def->name, def->model ) );
		Warning( "gmp_vehicle: %s failed (%s, %s)\n", def->name, def->model, def->script );
		return;
	}
	const Vector origin = tr.endpos - tr.plane.normal * MIN( pEnt->CollisionProp()->OBBMins().z, 0.0f ) + tr.plane.normal * 2;
	pEnt->Teleport( &origin, NULL, NULL );
	GMP_AddUndo( pPlayer, pEnt, "Vehicle" );
	Msg( "gmp_vehicle: %s (%s)\n", def->name, def->classname );
}

//-----------------------------------------------------------------------------
// Dupe frames
//-----------------------------------------------------------------------------
static void GMP_FrameToLocal( const matrix3x4_t &frame, const Vector &pos, const QAngle &ang, Vector &outPos, QAngle &outAng )
{
	matrix3x4_t world, inv, local;
	AngleMatrix( ang, pos, world );
	MatrixInvert( frame, inv );
	ConcatTransforms( inv, world, local );
	MatrixAngles( local, outAng, outPos );
}

static void GMP_FrameToWorld( const matrix3x4_t &frame, const Vector &pos, const QAngle &ang, Vector &outPos, QAngle &outAng )
{
	matrix3x4_t local, world;
	AngleMatrix( ang, pos, local );
	ConcatTransforms( frame, local, world );
	MatrixAngles( world, outAng, outPos );
}

static Vector GMP_FramePoint( const matrix3x4_t &frame, const Vector &pos, bool toLocal )
{
	Vector out;
	if ( toLocal )
		VectorITransform( pos, frame, out );
	else
		VectorTransform( pos, frame, out );
	return out;
}

// The world-space keys of a constraint table: LPos<i> of a world end, Ballsocket's LPos on the
// world, the Pulley's wheels.
static void GMP_FrameConstraintKeys( KeyValues *t, const bool world[4], const matrix3x4_t &frame, bool toLocal )
{
	char key[16];
	for ( int i = 0; i < 4; i++ )
	{
		Q_snprintf( key, sizeof( key ), "LPos%d", i + 1 );
		if ( world[i] && t->FindKey( key ) )
			GMP_SetVector( t, key, GMP_FramePoint( frame, GMP_ParseVector( t->GetString( key ) ), toLocal ) );
	}
	if ( world[1] && t->FindKey( "LPos" ) )
		GMP_SetVector( t, "LPos", GMP_FramePoint( frame, GMP_ParseVector( t->GetString( "LPos" ) ), toLocal ) );
	for ( int i = 2; i <= 3; i++ )
	{
		Q_snprintf( key, sizeof( key ), "WPos%d", i );
		if ( t->FindKey( key ) )
			GMP_SetVector( t, key, GMP_FramePoint( frame, GMP_ParseVector( t->GetString( key ) ), toLocal ) );
	}
}

//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------
// What the duplicator copies. NPCs and items only go into saves.
static bool GMP_DupeCanCopy( CBaseEntity *pEnt, bool save )
{
	if ( !pEnt || pEnt->IsWorld() || pEnt->IsPlayer() || ( pEnt->GetEFlags() & EFL_KILLME ) || !pEnt->GetModelName() )
		return false;
	const char *cls = pEnt->GetClassname();
	if ( GMP_SentKind( cls ) >= 0 || !Q_strnicmp( cls, "prop_physics", 12 ) || !Q_stricmp( cls, "prop_ragdoll" ) ||
		!Q_strnicmp( cls, "prop_dynamic", 12 ) || pEnt->GetServerVehicle() )
		return true;
	if ( save && pEnt->IsNPC() && pEnt->IsAlive() )
		return true;
	return save && ( !Q_strnicmp( cls, "item_", 5 ) || ( !Q_strnicmp( cls, "weapon_", 7 ) && !pEnt->GetOwnerEntity() ) );
}

static void GMP_DupeWriteEntity( CBaseEntity *pEnt, const matrix3x4_t &frame, KeyValues *pOut )
{
	const char *cls = pEnt->GetClassname();
	// GMod's names (the _override variants spawn any model).
	if ( !Q_stricmp( cls, "prop_physics_override" ) || !Q_stricmp( cls, "prop_physics_multiplayer" ) )
		cls = "prop_physics";
	else if ( !Q_stricmp( cls, "prop_dynamic_override" ) )
		cls = "prop_dynamic";
	pOut->SetString( "Class", cls );

	// A SENT's own fields first, so Class / Model / Pos below win.
	if ( GMP_SentKind( pEnt->GetClassname() ) >= 0 )
	{
		KeyValues *t = static_cast<CGMPSent *>( pEnt )->GetTable();
		for ( KeyValues *sub = t ? t->GetFirstSubKey() : NULL; sub; sub = sub->GetNextKey() )
			if ( !sub->GetFirstSubKey() )
				pOut->SetString( sub->GetName(), sub->GetString() );
	}
	pOut->SetString( "Model", STRING( pEnt->GetModelName() ) );
	CBaseAnimating *pAnim = pEnt->GetBaseAnimating();
	pOut->SetInt( "Skin", pAnim ? pAnim->m_nSkin : 0 );
	Vector pos;
	QAngle ang;
	GMP_FrameToLocal( frame, pEnt->GetAbsOrigin(), pEnt->GetAbsAngles(), pos, ang );
	GMP_SetVector( pOut, "Pos", pos );
	GMP_SetAngle( pOut, "Angle", ang );
	pOut->SetInt( "CollisionGroup", pEnt->GetCollisionGroup() );

	IPhysicsObject *list[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	const int count = pEnt->VPhysicsGetObjectList( list, ARRAYSIZE( list ) );
	if ( count > 0 )
	{
		KeyValues *pPhysics = pOut->FindKey( "PhysicsObjects", true );
		for ( int i = 0; i < count; i++ )
		{
			Vector p;
			QAngle a;
			list[i]->GetPosition( &p, &a );
			GMP_FrameToLocal( frame, p, a, pos, ang );
			char key[8];
			Q_snprintf( key, sizeof( key ), "%d", i );
			KeyValues *pObj = pPhysics->FindKey( key, true );
			GMP_SetVector( pObj, "Pos", pos );
			GMP_SetAngle( pObj, "Angle", ang );
			pObj->SetInt( "Frozen", list[i]->IsMotionEnabled() ? 0 : 1 );
		}
	}

	if ( pEnt->GetServerVehicle() )
	{
		const int v = GMP_VehicleByModel( STRING( pEnt->GetModelName() ) );
		if ( v >= 0 )
			pOut->SetString( "VehicleScript", s_GMPVehicles[v].script );
	}
	if ( pEnt->IsNPC() )
	{
		CBaseCombatWeapon *pWeapon = pEnt->MyCombatCharacterPointer() ? pEnt->MyCombatCharacterPointer()->GetActiveWeapon() : NULL;
		if ( pWeapon )
			pOut->SetString( "Equipment", pWeapon->GetClassname() );
		pOut->SetInt( "Health", pEnt->GetHealth() );
	}

	KeyValues *pMods = new KeyValues( "EntityMods" );
	if ( GMP_HasColour( pEnt ) )
		GMP_WriteColourMod( pEnt, pMods->FindKey( "colour", true ) );
	if ( *GMP_GetMaterial( pEnt ) )
		pMods->FindKey( "material", true )->SetString( "MaterialOverride", GMP_GetMaterial( pEnt ) );
	if ( KeyValues *pTrail = GMP_GetTrail( pEnt ) )
	{
		KeyValues *pCopy = pTrail->MakeCopy();
		pCopy->SetName( "trail" );
		pMods->AddSubKey( pCopy );
	}
	if ( pMods->GetFirstSubKey() )
		pOut->AddSubKey( pMods );
	else
		pMods->deleteThis();
}

// GMod's duplicator.Copy table for these entities and the constraints among them.
static KeyValues *GMP_DupeBuild( const CUtlVector<EHANDLE> &ents, const CUtlVector<CGMPConstraint *> &constraints, const matrix3x4_t &frame, bool save )
{
	KeyValues *pDupe = new KeyValues( "Dupe" );
	KeyValues *pEnts = pDupe->FindKey( "Entities", true );
	KeyValues *pCons = pDupe->FindKey( "Constraints", true );
	Vector mins( FLT_MAX, FLT_MAX, FLT_MAX ), maxs( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	int count = 0;
	for ( int i = 0; i < ents.Count(); i++ )
	{
		CBaseEntity *pEnt = ents[i].Get();
		if ( !GMP_DupeCanCopy( pEnt, save ) )
			continue;
		char key[16];
		Q_snprintf( key, sizeof( key ), "%d", pEnt->entindex() );
		GMP_DupeWriteEntity( pEnt, frame, pEnts->FindKey( key, true ) );
		Vector emins, emaxs;
		pEnt->CollisionProp()->WorldSpaceAABB( &emins, &emaxs );
		for ( int c = 0; c < 8; c++ )
		{
			const Vector corner( ( c & 1 ) ? emaxs.x : emins.x, ( c & 2 ) ? emaxs.y : emins.y, ( c & 4 ) ? emaxs.z : emins.z );
			const Vector local = GMP_FramePoint( frame, corner, true );
			mins = mins.Min( local );
			maxs = maxs.Max( local );
		}
		count++;
	}
	if ( !count )
	{
		pDupe->deleteThis();
		return NULL;
	}
	GMP_SetVector( pDupe, "Mins", mins );
	GMP_SetVector( pDupe, "Maxs", maxs );

	int n = 0;
	for ( int i = 0; i < constraints.Count(); i++ )
	{
		CGMPConstraint *pC = constraints[i];
		if ( !pC || ( pC->GetEFlags() & EFL_KILLME ) || !pC->GetTable() )
			continue;
		bool ok = true, world[4] = { false, false, false, false };
		for ( int e = 1; e <= 4 && ok; e++ )
		{
			CBaseEntity *pEnt = pC->GetEnt( e );
			if ( !pC->HasEnt( e ) )
				continue;
			world[e - 1] = pEnt && pEnt->IsWorld();
			ok = world[e - 1] || ( pEnt && ents.Find( EHANDLE( pEnt ) ) >= 0 && GMP_DupeCanCopy( pEnt, save ) );
		}
		if ( !ok )
			continue;
		char key[16];
		Q_snprintf( key, sizeof( key ), "%d", ++n );
		KeyValues *t = pC->GetTable()->MakeCopy();
		t->SetName( key );
		GMP_FrameConstraintKeys( t, world, frame, true );
		KeyValues *pList = t->FindKey( "Entity", true );
		for ( int e = 1; e <= 4; e++ )
		{
			if ( !pC->HasEnt( e ) )
				continue;
			char ekey[8], bone[8];
			Q_snprintf( ekey, sizeof( ekey ), "%d", e );
			Q_snprintf( bone, sizeof( bone ), "Bone%d", e );
			KeyValues *pE = pList->FindKey( ekey, true );
			pE->SetInt( "Index", world[e - 1] ? 0 : pC->GetEnt( e )->entindex() );
			pE->SetInt( "World", world[e - 1] ? 1 : 0 );
			pE->SetInt( "Bone", t->GetInt( bone ) );
		}
		pCons->AddSubKey( t );
	}
	return pDupe;
}

//-----------------------------------------------------------------------------
// Paste
//-----------------------------------------------------------------------------
static bool GMP_DupeValidModel( const char *model )
{
	return model && !Q_strnicmp( model, "models/", 7 ) && !Q_strstr( model, ".." ) && Q_stristr( model, ".mdl" ) && filesystem->FileExists( model, "GAME" );
}

static CBaseEntity *GMP_DupeCreateEntity( CBasePlayer *pPlayer, KeyValues *pData, const matrix3x4_t &frame )
{
	const char *cls = pData->GetString( "Class" );
	const char *model = pData->GetString( "Model" );
	Vector pos;
	QAngle ang;
	GMP_FrameToWorld( frame, GMP_ParseVector( pData->GetString( "Pos" ) ), GMP_ParseAngle( pData->GetString( "Angle" ) ), pos, ang );

	if ( GMP_SentKind( cls ) >= 0 )
	{
		// Only its own fields reach ApplySettings.
		KeyValues *t = new KeyValues( "Entity" );
		for ( KeyValues *sub = pData->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			const char *name = sub->GetName();
			if ( sub->GetFirstSubKey() || !Q_stricmp( name, "Class" ) || !Q_stricmp( name, "Pos" ) || !Q_stricmp( name, "Angle" ) || !Q_stricmp( name, "CollisionGroup" ) )
				continue;
			t->SetString( name, sub->GetString() );
		}
		if ( !GMP_DupeValidModel( t->GetString( "Model" ) ) )
			t->SetString( "Model", "" );
		CBaseEntity *pEnt = GMP_MakeSent( pPlayer, cls, t, pos, ang );
		t->deleteThis();
		return pEnt;
	}
	if ( pData->GetString( "VehicleScript" )[0] || !Q_strnicmp( cls, "prop_vehicle_", 13 ) )
	{
		const int v = GMP_VehicleByModel( model );
		const char *script = pData->GetString( "VehicleScript", v >= 0 ? s_GMPVehicles[v].script : "" );
		if ( Q_strnicmp( script, "scripts/vehicles/", 17 ) || Q_strstr( script, ".." ) || !GMP_DupeValidModel( model ) )
			return NULL;
		return GMP_MakeVehicle( cls, model, script, pos, ang );
	}

	const bool physics = !Q_stricmp( cls, "prop_physics" ) || !Q_strnicmp( cls, "prop_physics_", 13 );
	const bool ragdoll = !Q_stricmp( cls, "prop_ragdoll" );
	const bool dynamic = !Q_strnicmp( cls, "prop_dynamic", 12 );
	const bool npc = !Q_strnicmp( cls, "npc_", 4 );
	const bool item = !Q_strnicmp( cls, "item_", 5 ) || !Q_strnicmp( cls, "weapon_", 7 );
	if ( !physics && !ragdoll && !dynamic && !npc && !item )
		return NULL;
	if ( ( physics || ragdoll || dynamic ) && !GMP_DupeValidModel( model ) )
		return NULL;
	for ( const char *p = cls; *p; p++ )
		if ( !V_isalnum( *p ) && *p != '_' )
			return NULL;

	GMPNPCPrecacheScope precache;
	CBaseEntity *pEnt = CreateEntityByName( physics ? "prop_physics_override" : dynamic ? "prop_dynamic_override" : cls );
	if ( !pEnt )
		return NULL;
	if ( physics || ragdoll || dynamic || ( npc && GMP_DupeValidModel( model ) ) )
		pEnt->KeyValue( "model", model );
	pEnt->KeyValue( "skin", pData->GetInt( "Skin" ) );
	if ( dynamic )
		pEnt->KeyValue( "solid", "2" );
	if ( npc )
	{
		const char *weapon = pData->GetString( "Equipment" );
		if ( !Q_strnicmp( weapon, "weapon_", 7 ) )
			pEnt->KeyValue( "additionalequipment", weapon );
		pEnt->AddSpawnFlags( SF_NPC_FADE_CORPSE | SF_NPC_ALWAYSTHINK );
		if ( pPlayer )
			pEnt->SetName( AllocPooledString( UTIL_VarArgs( "gmp_spawned_npc_%d", pPlayer->GetUserID() ) ) );
	}
	pEnt->SetAbsOrigin( pos );
	pEnt->SetAbsAngles( ang );
	if ( DispatchSpawn( pEnt ) < 0 )
	{
		UTIL_Remove( pEnt );
		return NULL;
	}
	pEnt->Activate();
	if ( npc && pData->GetInt( "Health" ) > 0 )
		pEnt->SetHealth( pData->GetInt( "Health" ) );
	return pEnt;
}

// The saved physics state: per-bone poses (ragdolls), frozen objects; collision group and modifiers.
static void GMP_DupeRestoreEntity( CBaseEntity *pEnt, KeyValues *pData, const matrix3x4_t &frame )
{
	IPhysicsObject *list[VPHYSICS_MAX_OBJECT_LIST_COUNT];
	const int count = pEnt->VPhysicsGetObjectList( list, ARRAYSIZE( list ) );
	KeyValues *pPhysics = pData->FindKey( "PhysicsObjects" );
	for ( int i = 0; pPhysics && i < count; i++ )
	{
		char key[8];
		Q_snprintf( key, sizeof( key ), "%d", i );
		KeyValues *pObj = pPhysics->FindKey( key );
		if ( !pObj )
			continue;
		Vector pos;
		QAngle ang;
		GMP_FrameToWorld( frame, GMP_ParseVector( pObj->GetString( "Pos" ) ), GMP_ParseAngle( pObj->GetString( "Angle" ) ), pos, ang );
		list[i]->SetPosition( pos, ang, true );
		list[i]->SetVelocity( &vec3_origin, &vec3_origin );
		const bool frozen = pObj->GetInt( "Frozen" ) != 0;
		list[i]->EnableMotion( !frozen );
		if ( !frozen )
			list[i]->Wake();
	}
	const int group = pData->GetInt( "CollisionGroup", -1 );
	if ( group == COLLISION_GROUP_NONE || group == COLLISION_GROUP_DEBRIS || group == COLLISION_GROUP_INTERACTIVE_DEBRIS )
		pEnt->SetCollisionGroup( group );

	KeyValues *pMods = pData->FindKey( "EntityMods" );
	if ( !pMods )
		return;
	if ( KeyValues *pColour = pMods->FindKey( "colour" ) )
		GMP_ApplyColourMod( pEnt, pColour );
	if ( KeyValues *pMaterial = pMods->FindKey( "material" ) )
		GMP_SetMaterial( pEnt, pMaterial->GetString( "MaterialOverride" ) );
	if ( KeyValues *pTrail = pMods->FindKey( "trail" ) )
		GMP_SetTrail( pEnt, pTrail );
}

// duplicator.Paste: entities, then the constraints between them. Returns the entities made.
static int GMP_DupePasteTable( CBasePlayer *pPlayer, KeyValues *pDupe, const matrix3x4_t &frame, const char *undo )
{
	KeyValues *pEnts = pDupe->FindKey( "Entities" );
	if ( !pEnts )
		return 0;
	CUtlVector<int> oldIndex;
	CUtlVector<EHANDLE> made;
	GMP_UndoBeginGroup();
	for ( KeyValues *pData = pEnts->GetFirstSubKey(); pData && made.Count() < 512; pData = pData->GetNextKey() )
	{
		CBaseEntity *pEnt = GMP_DupeCreateEntity( pPlayer, pData, frame );
		if ( !pEnt )
		{
			Warning( "GModPort dupe: could not create %s (%s)\n", pData->GetString( "Class" ), pData->GetString( "Model" ) );
			continue;
		}
		GMP_DupeRestoreEntity( pEnt, pData, frame );
		oldIndex.AddToTail( atoi( pData->GetName() ) );
		made.AddToTail( pEnt );
		GMP_AddUndo( pPlayer, pEnt, undo );
	}
	int constraints = 0;
	KeyValues *pCons = pDupe->FindKey( "Constraints" );
	for ( KeyValues *pData = pCons ? pCons->GetFirstSubKey() : NULL; pData; pData = pData->GetNextKey() )
	{
		KeyValues *pList = pData->FindKey( "Entity" );
		CBaseEntity *ents[4] = { NULL, NULL, NULL, NULL };
		bool world[4] = { false, false, false, false }, ok = pList != NULL;
		for ( int e = 0; e < 4 && ok; e++ )
		{
			char key[8];
			Q_snprintf( key, sizeof( key ), "%d", e + 1 );
			KeyValues *pE = pList->FindKey( key );
			if ( !pE )
				continue;
			world[e] = pE->GetInt( "World" ) != 0;
			if ( world[e] )
				ents[e] = GetWorldEntity();
			else
			{
				const int at = oldIndex.Find( pE->GetInt( "Index" ) );
				ents[e] = at >= 0 ? made[at].Get() : NULL;
			}
			ok = ents[e] != NULL;
		}
		if ( !ok )
			continue;
		KeyValues *t = pData->MakeCopy();
		t->SetName( "Constraint" );
		if ( KeyValues *pE = t->FindKey( "Entity" ) )
		{
			t->RemoveSubKey( pE );
			pE->deleteThis();
		}
		GMP_FrameConstraintKeys( t, world, frame, false );
		CGMPConstraint *pC = GMP_CreateConstraint( t, ents, pPlayer );
		t->deleteThis();
		if ( pC )
		{
			GMP_AddUndo( pPlayer, pC, undo );
			constraints++;
		}
	}
	GMP_UndoEndGroup();
	Msg( "GModPort dupe: pasted %d entities, %d constraints\n", made.Count(), constraints );
	return made.Count();
}

//-----------------------------------------------------------------------------
// The duplicator tool's clipboard ("u<userid>")
//-----------------------------------------------------------------------------
static KeyValues *s_pGMPDupes = NULL;

static KeyValues *GMP_DupeClipboard( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !s_pGMPDupes )
		return NULL;
	KeyValues *pDupe = s_pGMPDupes->FindKey( UTIL_VarArgs( "u%d", pPlayer->GetUserID() ) );
	return pDupe && pDupe->FindKey( "Entities" ) ? pDupe : NULL;
}

static void GMP_DupeSetClipboard( CBasePlayer *pPlayer, KeyValues *pDupe )
{
	if ( !s_pGMPDupes )
		s_pGMPDupes = new KeyValues( "GMPDupes" );
	char key[16];
	Q_snprintf( key, sizeof( key ), "u%d", pPlayer->GetUserID() );
	if ( KeyValues *pOld = s_pGMPDupes->FindKey( key ) )
	{
		s_pGMPDupes->RemoveSubKey( pOld );
		pOld->deleteThis();
	}
	pDupe->SetName( key );
	s_pGMPDupes->AddSubKey( pDupe );
}

static int GMP_DupeCount( KeyValues *pDupe, const char *key )
{
	int n = 0;
	KeyValues *pList = pDupe ? pDupe->FindKey( key ) : NULL;
	for ( KeyValues *sub = pList ? pList->GetFirstSubKey() : NULL; sub; sub = sub->GetNextKey() )
		n++;
	return n;
}

// The duplicator's right click: pEnt and everything constrained to it.
static void GMP_DupeCopy( CBasePlayer *pPlayer, CBaseEntity *pEnt )
{
	if ( !pPlayer || !GMP_DupeCanCopy( pEnt, false ) )
		return;
	CUtlVector<EHANDLE> ents;
	CUtlVector<CGMPConstraint *> constraints;
	GMP_GetConstrained( pEnt, ents, constraints );
	// The frame: the bottom centre of everything, turned to the view.
	Vector mins( FLT_MAX, FLT_MAX, FLT_MAX ), maxs( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	for ( int i = 0; i < ents.Count(); i++ )
	{
		if ( !ents[i].Get() )
			continue;
		Vector emins, emaxs;
		ents[i]->CollisionProp()->WorldSpaceAABB( &emins, &emaxs );
		mins = mins.Min( emins );
		maxs = maxs.Max( emaxs );
	}
	matrix3x4_t frame;
	AngleMatrix( QAngle( 0, pPlayer->EyeAngles().y, 0 ), Vector( ( mins.x + maxs.x ) / 2, ( mins.y + maxs.y ) / 2, mins.z ), frame );
	KeyValues *pDupe = GMP_DupeBuild( ents, constraints, frame, false );
	if ( !pDupe )
		return;
	GMP_DupeSetClipboard( pPlayer, pDupe );
	ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Copied %d entities, %d constraints", GMP_DupeCount( pDupe, "Entities" ), GMP_DupeCount( pDupe, "Constraints" ) ) );
	DevMsg( "GModPort: duplicator copied %d entities, %d constraints\n", GMP_DupeCount( pDupe, "Entities" ), GMP_DupeCount( pDupe, "Constraints" ) );
}

// The duplicator's left click: on the surface, turned with the view.
static void GMP_DupePaste( CBasePlayer *pPlayer, const trace_t &tr )
{
	KeyValues *pDupe = GMP_DupeClipboard( pPlayer );
	if ( !pDupe )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "Copy something first (Alt fire), or open a dupe in the Dupes tab" );
		return;
	}
	if ( tr.fraction >= 1.0f )
		return;
	const Vector mins = GMP_ParseVector( pDupe->GetString( "Mins" ) );
	matrix3x4_t frame;
	AngleMatrix( QAngle( 0, pPlayer->EyeAngles().y, 0 ), tr.endpos + Vector( 0, 0, 1 - MIN( mins.z, 0.0f ) ), frame );
	const int count = GMP_DupePasteTable( pPlayer, pDupe, frame, "Duplicator" );
	DevMsg( "GModPort: duplicator pasted %d entities\n", count );
}

// The Dupes tab's icon: the dupe's biggest model, in dupes/<name>.txt next to the file.
static void GMP_DupeWriteInfo( const char *path, KeyValues *pDupe )
{
	const char *model = "";
	float best = -1;
	KeyValues *pEnts = pDupe->FindKey( "Entities" );
	for ( KeyValues *pData = pEnts ? pEnts->GetFirstSubKey() : NULL; pData; pData = pData->GetNextKey() )
	{
		// Spawned models are precached; FindOrLoadModel returns NULL on the server here.
		const char *name = pData->GetString( "Model" );
		if ( !name[0] || name[0] == '*' )
			continue;
		const int index = modelinfo->GetModelIndex( name );
		const model_t *pMdl = index >= 0 ? modelinfo->GetModel( index ) : NULL;
		Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
		if ( pMdl )
			modelinfo->GetModelBounds( pMdl, mins, maxs );
		if ( ( maxs - mins ).LengthSqr() > best )
		{
			best = ( maxs - mins ).LengthSqr();
			model = pData->GetString( "Model" );
		}
	}
	KeyValues *pInfo = new KeyValues( "DupeInfo" );
	pInfo->SetString( "model", model );
	pInfo->SetInt( "entities", GMP_DupeCount( pDupe, "Entities" ) );
	pInfo->SetInt( "constraints", GMP_DupeCount( pDupe, "Constraints" ) );
	pInfo->SetString( "map", STRING( gpGlobals->mapname ) );
	char info[MAX_PATH];
	Q_strncpy( info, path, sizeof( info ) );
	Q_SetExtension( info, ".txt", sizeof( info ) );
	pInfo->SaveToFile( filesystem, info, "MOD" );
	pInfo->deleteThis();
}

// A free <dir>/<prefix>_N.<ext> when no name is given.
static void GMP_PickFileName( const char *dir, const char *prefix, const char *ext, const char *name, char *out, int size )
{
	if ( GMP_ValidFileName( name ) )
	{
		Q_snprintf( out, size, "%s/%s.%s", dir, name, ext );
		return;
	}
	for ( int i = 1; i < 10000; i++ )
	{
		Q_snprintf( out, size, "%s/%s_%d.%s", dir, prefix, i, ext );
		if ( !filesystem->FileExists( out, "MOD" ) )
			return;
	}
}

CON_COMMAND( gm_dupe_save, "GModPort: gm_dupe_save [name] - save the duplicator's copy to dupes/<name>.dupe" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	KeyValues *pDupe = GMP_DupeClipboard( pPlayer );
	if ( !pDupe )
	{
		if ( pPlayer )
			ClientPrint( pPlayer, HUD_PRINTCENTER, "Nothing copied: right click something with the Duplicator" );
		return;
	}
	char path[MAX_PATH];
	GMP_PickFileName( "dupes", "dupe", "dupe", args.ArgC() > 1 ? args.ArgS() : NULL, path, sizeof( path ) );
	KeyValues *pFile = pDupe->MakeCopy();
	pFile->SetName( "Dupe" );
	const bool ok = GMP_WriteGMFile( path, "DUP3", NULL, 0, pFile );
	if ( ok )
		GMP_DupeWriteInfo( path, pFile );
	pFile->deleteThis();
	ClientPrint( pPlayer, HUD_PRINTCENTER, ok ? UTIL_VarArgs( "Saved %s", path ) : "Could not save the dupe" );
	Msg( "gm_dupe_save: %s %s\n", path, ok ? "ok" : "FAILED" );
}

CON_COMMAND( gm_dupe_open, "GModPort: gm_dupe_open <dupes/name.dupe> - put a saved dupe on the duplicator" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer || args.ArgC() < 2 )
		return;
	const char *path = args.ArgS();
	if ( Q_strnicmp( path, "dupes/", 6 ) || Q_strstr( path, ".." ) || !Q_stristr( path, ".dupe" ) )
		return;
	KeyValues *pDupe = GMP_ReadGMFile( path, "MOD", "DUP3", 0, NULL );
	if ( !pDupe || !pDupe->FindKey( "Entities" ) )
	{
		if ( pDupe )
			pDupe->deleteThis();
		ClientPrint( pPlayer, HUD_PRINTCENTER, "Could not read that dupe" );
		return;
	}
	const int count = GMP_DupeCount( pDupe, "Entities" );
	GMP_DupeSetClipboard( pPlayer, pDupe );
	if ( !pPlayer->Weapon_OwnsThisType( "gmod_tool" ) )
		pPlayer->GiveNamedItem( "gmod_tool" );
	engine->ClientCommand( pPlayer->edict(), "gmod_toolmode duplicator;use gmod_tool\n" );
	ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Opened %s (%d entities): left click to paste", path, count ) );
}

//-----------------------------------------------------------------------------
// Saves
//-----------------------------------------------------------------------------
static char s_szGMPPendingLoad[MAX_PATH] = "";
static bool GMP_LoadSave( CBasePlayer *pPlayer, const char *path );

CON_COMMAND( gm_save, "GModPort: gm_save [name] - save what was spawned on this map to saves/<name>.gms" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		pPlayer = UTIL_GetListenServerHost();
	// Everything spawned (the undo lists) and everything constrained to it.
	CUtlVector<EHANDLE> ents;
	CUtlVector<CGMPConstraint *> constraints;
	for ( int i = 0; i < s_GMPUndo.Count(); i++ )
	{
		CBaseEntity *pEnt = s_GMPUndo[i].ent.Get();
		if ( !pEnt || ( pEnt->GetEFlags() & EFL_KILLME ) )
			continue;
		if ( !Q_stricmp( pEnt->GetClassname(), "gmp_constraint" ) )
		{
			for ( int e = 1; e <= 4; e++ )
				GMP_GetConstrained( static_cast<CGMPConstraint *>( pEnt )->GetEnt( e ), ents, constraints );
		}
		else if ( GMP_DupeCanCopy( pEnt, true ) )
			GMP_GetConstrained( pEnt, ents, constraints );
	}
	matrix3x4_t frame;
	SetIdentityMatrix( frame );
	KeyValues *pSave = GMP_DupeBuild( ents, constraints, frame, true );
	if ( !pSave )
		pSave = new KeyValues( "Save" );
	pSave->SetName( "Save" );
	pSave->SetString( "Map", STRING( gpGlobals->mapname ) );
	if ( pPlayer && pPlayer->IsAlive() )
	{
		KeyValues *pPl = pSave->FindKey( "Player", true );
		GMP_SetVector( pPl, "Pos", pPlayer->GetAbsOrigin() );
		GMP_SetAngle( pPl, "Angle", pPlayer->EyeAngles() );
		pPl->SetInt( "Health", pPlayer->GetHealth() );
		pPl->SetInt( "Armor", pPlayer->ArmorValue() );
		if ( pPlayer->GetActiveWeapon() )
			pPl->SetString( "Active", pPlayer->GetActiveWeapon()->GetClassname() );
		KeyValues *pWeapons = pPl->FindKey( "Weapons", true );
		int n = 0;
		for ( int i = 0; i < pPlayer->WeaponCount(); i++ )
		{
			if ( !pPlayer->GetWeapon( i ) )
				continue;
			char key[8];
			Q_snprintf( key, sizeof( key ), "%d", ++n );
			pWeapons->SetString( key, pPlayer->GetWeapon( i )->GetClassname() );
		}
	}
	char name[80] = "";
	if ( args.ArgC() > 1 && GMP_ValidFileName( args.ArgS() ) )
		Q_strncpy( name, args.ArgS(), sizeof( name ) );
	else
	{
		const time_t now = time( NULL );
		char stamp[32];
		strftime( stamp, sizeof( stamp ), "%Y%m%d_%H%M%S", localtime( &now ) );
		Q_snprintf( name, sizeof( name ), "%s_%s", STRING( gpGlobals->mapname ), stamp );
	}
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "saves/%s.gms", name );
	const char *header[2] = { STRING( gpGlobals->mapname ), "WSID:0" };
	const bool ok = GMP_WriteGMFile( path, "GMS3", header, 2, pSave );
	if ( ok )
		GMP_DupeWriteInfo( path, pSave );
	const int count = GMP_DupeCount( pSave, "Entities" );
	pSave->deleteThis();
	if ( pPlayer )
		ClientPrint( pPlayer, HUD_PRINTCENTER, ok ? UTIL_VarArgs( "Saved %s (%d entities)", name, count ) : "Could not save" );
	Msg( "gm_save: %s, %d entities %s\n", path, count, ok ? "ok" : "FAILED" );
}

static bool GMP_LoadSave( CBasePlayer *pPlayer, const char *path )
{
	CUtlString map;
	KeyValues *pSave = GMP_ReadGMFile( path, "MOD", "GMS3", 2, &map );
	if ( !pSave )
		return false;
	// The map first: GMod's saves restore the whole level, this one only what was spawned.
	if ( Q_stricmp( map.Get(), STRING( gpGlobals->mapname ) ) )
	{
		pSave->deleteThis();
		if ( !engine->IsMapValid( map.Get() ) )
		{
			Warning( "gm_load: map %s is not installed\n", map.Get() );
			return false;
		}
		Q_strncpy( s_szGMPPendingLoad, path, sizeof( s_szGMPPendingLoad ) );
		engine->ServerCommand( UTIL_VarArgs( "changelevel %s\n", map.Get() ) );
		return true;
	}
	// Replace what was spawned.
	for ( int i = s_GMPUndo.Count() - 1; i >= 0; i-- )
		if ( CBaseEntity *pEnt = s_GMPUndo[i].ent.Get() )
			UTIL_Remove( pEnt );
	s_GMPUndo.RemoveAll();
	matrix3x4_t frame;
	SetIdentityMatrix( frame );
	const int count = GMP_DupePasteTable( pPlayer, pSave, frame, "Saved" );
	KeyValues *pPl = pSave->FindKey( "Player" );
	if ( pPlayer && pPl && pPlayer->IsAlive() )
	{
		KeyValues *pWeapons = pPl->FindKey( "Weapons" );
		for ( KeyValues *sub = pWeapons ? pWeapons->GetFirstSubKey() : NULL; sub; sub = sub->GetNextKey() )
			if ( !Q_strnicmp( sub->GetString(), "weapon_", 7 ) || !Q_stricmp( sub->GetString(), "gmod_tool" ) )
				if ( !pPlayer->Weapon_OwnsThisType( sub->GetString() ) )
					pPlayer->GiveNamedItem( sub->GetString() );
		const Vector pos = GMP_ParseVector( pPl->GetString( "Pos" ) );
		const QAngle ang = GMP_ParseAngle( pPl->GetString( "Angle" ) );
		pPlayer->Teleport( &pos, &ang, &vec3_origin );
		pPlayer->SnapEyeAngles( ang );
		if ( pPl->GetInt( "Health" ) > 0 )
			pPlayer->SetHealth( pPl->GetInt( "Health" ) );
		pPlayer->SetArmorValue( pPl->GetInt( "Armor" ) );
		const char *active = pPl->GetString( "Active" );
		if ( *active )
			engine->ClientCommand( pPlayer->edict(), "use %s\n", active );
	}
	pSave->deleteThis();
	if ( pPlayer )
		ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Loaded %s (%d entities)", path, count ) );
	Msg( "gm_load: %s, %d entities\n", path, count );
	return true;
}

CON_COMMAND( gm_load, "GModPort: gm_load <name> - load saves/<name>.gms (changes to its map)" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		pPlayer = UTIL_GetListenServerHost();
	if ( args.ArgC() < 2 || !GMP_ValidFileName( args.ArgS() ) )
		return;
	char path[MAX_PATH];
	Q_snprintf( path, sizeof( path ), "saves/%s.gms", args.ArgS() );
	if ( !GMP_LoadSave( pPlayer, path ) && pPlayer )
		ClientPrint( pPlayer, HUD_PRINTCENTER, "Could not load that save" );
}

// A load that had to change the map runs once the player is in.
class CGMPSaveSystem : public CAutoGameSystemPerFrame
{
public:
	CGMPSaveSystem( void ) : CAutoGameSystemPerFrame( "CGMPSaveSystem" ) { m_flLoadAt = 0; }
	void LevelInitPostEntity( void ) { m_flLoadAt = s_szGMPPendingLoad[0] ? gpGlobals->curtime + 1.0f : 0; }
	void LevelShutdownPreEntity( void ) { s_GMPUndo.RemoveAll(); }
	void FrameUpdatePostEntityThink( void )
	{
		if ( !m_flLoadAt || gpGlobals->curtime < m_flLoadAt )
			return;
		CBasePlayer *pPlayer = UTIL_GetListenServerHost();
		if ( !pPlayer || !pPlayer->IsAlive() )
			return;
		m_flLoadAt = 0;
		char path[MAX_PATH];
		Q_strncpy( path, s_szGMPPendingLoad, sizeof( path ) );
		s_szGMPPendingLoad[0] = 0;
		GMP_LoadSave( pPlayer, path );
	}
private:
	float m_flLoadAt;
};
static CGMPSaveSystem s_GMPSaveSystem;
#endif // !CLIENT_DLL
