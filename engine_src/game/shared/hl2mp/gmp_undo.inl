// GMod-style undo and cleanup (server). Everything a player spawns from the spawn menu goes on
// one list, newest last; gmod_undo removes that player's newest entry that still exists,
// gmod_cleanup all of them. Handles go stale on their own when an entity dies or the map changes.

struct GMPUndo_t
{
	int userid;
	EHANDLE ent;
	char name[64];
};
static CUtlVector<GMPUndo_t> s_GMPUndo;

static void GMP_AddUndo( CBasePlayer *pPlayer, CBaseEntity *pEnt, const char *name )
{
	if ( !pPlayer || !pEnt )
		return;
	for ( int i = s_GMPUndo.Count() - 1; i >= 0; i-- )
		if ( !s_GMPUndo[i].ent.Get() )
			s_GMPUndo.Remove( i );
	GMPUndo_t &entry = s_GMPUndo[s_GMPUndo.AddToTail()];
	entry.userid = pPlayer->GetUserID();
	entry.ent = pEnt;
	Q_strncpy( entry.name, name, sizeof( entry.name ) );
}

CON_COMMAND( gmod_undo, "GModPort: undo the last thing you spawned" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;
	for ( int i = s_GMPUndo.Count() - 1; i >= 0; i-- )
	{
		if ( s_GMPUndo[i].userid != pPlayer->GetUserID() )
			continue;
		CBaseEntity *pEnt = s_GMPUndo[i].ent.Get();
		char name[64];
		Q_strncpy( name, s_GMPUndo[i].name, sizeof( name ) );
		s_GMPUndo.Remove( i );
		if ( !pEnt )
			continue;
		UTIL_Remove( pEnt );
		ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "Undone %s", name ) );
		Msg( "gmod_undo: %s\n", name );
		return;
	}
	ClientPrint( pPlayer, HUD_PRINTCENTER, "Nothing to undo" );
}

CON_COMMAND( gmod_cleanup, "GModPort: remove everything you spawned" )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;
	int removed = 0;
	for ( int i = s_GMPUndo.Count() - 1; i >= 0; i-- )
	{
		if ( s_GMPUndo[i].userid != pPlayer->GetUserID() )
			continue;
		if ( CBaseEntity *pEnt = s_GMPUndo[i].ent.Get() )
		{
			UTIL_Remove( pEnt );
			removed++;
		}
		s_GMPUndo.Remove( i );
	}
	ClientPrint( pPlayer, HUD_PRINTCENTER, "Cleaned up everything" );
	Msg( "gmod_cleanup: removed %d\n", removed );
}
