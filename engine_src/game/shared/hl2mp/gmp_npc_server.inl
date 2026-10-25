// Native sandbox spawning. Behavior, navigation, schedules and attacks remain
// in the actual HL2/episodic NPC classes, not in a scripted approximation.
#include "gmp_npc_catalog.h"
#include "ai_basenpc.h"
#include "ai_squad.h"

static ConVar sbox_maxnpcs("sbox_maxnpcs", "20", FCVAR_GAMEDLL, "NPC limit per player; -1 for unlimited");

class GMPNPCPrecacheScope
{
    bool previous;
public:
    GMPNPCPrecacheScope() : previous(CBaseEntity::IsPrecacheAllowed()) { CBaseEntity::SetAllowPrecache(true); }
    ~GMPNPCPrecacheScope() { CBaseEntity::SetAllowPrecache(previous); }
};

CON_COMMAND(gmp_npc, "GModPort: gmp_npc <spawnmenu id> [weapon|none]")
{
    CBasePlayer *player=UTIL_GetCommandClient();
    if (!player || !player->IsAlive() || args.ArgC()<2) return;
    const GMPNPCDef *def=NULL;
    for (int i=0;i<ARRAYSIZE(s_GMPNPCs);++i)
        if (!Q_stricmp(args[1],s_GMPNPCs[i].id)) { def=&s_GMPNPCs[i]; break; }
    if (!def) { ClientPrint(player,HUD_PRINTCONSOLE,"Unknown NPC spawnmenu id.\n"); return; }
    if (!filesystem->FileExists(def->iconModel,"GAME"))
    {
        ClientPrint(player,HUD_PRINTCONSOLE,"NPC model is not installed; see the NPC asset manifest.\n");
        Warning("gmp_npc: missing %s for %s\n",def->iconModel,def->id);
        return;
    }
    char ownerTag[64];
    Q_snprintf(ownerTag,sizeof(ownerTag),"gmp_spawned_npc_%d",player->GetUserID());
    int count=0;
    for (CBaseEntity *e=NULL;(e=gEntList.FindEntityByName(e,ownerTag))!=NULL;)
        if (e->GetHealth()>0) ++count;
    if (sbox_maxnpcs.GetInt()>=0 && count>=sbox_maxnpcs.GetInt())
    { ClientPrint(player,HUD_PRINTCONSOLE,"NPC limit reached (sbox_maxnpcs).\n"); return; }

    const char *weapon=def->weapons[0];
    if (args.ArgC()>2)
    {
        weapon=args[2];
        bool valid=!Q_stricmp(weapon,"none");
        for (int i=0;i<ARRAYSIZE(def->weapons) && def->weapons[i];++i)
            if (!Q_stricmp(weapon,def->weapons[i])) valid=true;
        if (!valid) { ClientPrint(player,HUD_PRINTCONSOLE,"Weapon not supported by this NPC.\n"); return; }
    }
    Vector direction;
    player->EyeVectors(&direction);
    trace_t tr;
    Vector start=player->EyePosition();
    UTIL_TraceLine(start,start+direction*4096,MASK_SOLID,player,COLLISION_GROUP_NONE,&tr);
    if (tr.startsolid || tr.allsolid || tr.fraction==1 || tr.surface.flags&SURF_SKY) return;
    if ((def->onFloor || def->onCeiling) &&
        !((def->onFloor && tr.plane.normal.z>=.95f) || (def->onCeiling && tr.plane.normal.z<=-.95f)))
    { ClientPrint(player,HUD_PRINTCONSOLE,"This NPC needs a floor/ceiling surface.\n"); return; }

    GMPNPCPrecacheScope precache;
    CBaseEntity *entity=CreateEntityByName(def->entityClass);
    if (!entity) { Warning("gmp_npc: unregistered native class %s\n",def->entityClass); return; }
    entity->SetAbsOrigin(tr.endpos+tr.plane.normal*def->offset);
    entity->SetAbsAngles(QAngle(0,player->EyeAngles().y+180+def->rotateYaw,0));
    entity->AddSpawnFlags(def->totalFlags>=0 ? def->totalFlags : (SF_NPC_FADE_CORPSE|SF_NPC_ALWAYSTHINK|def->spawnFlags));
    entity->SetName(AllocPooledString(ownerTag));
    if (*def->model) entity->KeyValue("model",def->model);
    for (int i=0;i<ARRAYSIZE(def->keys) && def->keys[i].key;++i)
    {
        const GMPNPCKey &kv=def->keys[i];
        if (!Q_stricmp(kv.key,"SquadName"))
        {
            char squadName[128];
            Q_strncpy(squadName,kv.value,sizeof(squadName));
            for (int n=0;;++n)
            {
                CAI_Squad *squad=g_AI_SquadManager.FindSquad(AllocPooledString(squadName));
                if (!squad || squad->NumMembers()<16) break;
                Q_snprintf(squadName,sizeof(squadName),"%s%d",kv.value,n);
            }
            entity->KeyValue(kv.key,squadName);
        }
        else entity->KeyValue(kv.key,kv.value);
    }
    if (weapon && *weapon && Q_stricmp(weapon,"none")) entity->KeyValue("additionalequipment",weapon);
    if (def->skin>=0) entity->KeyValue("skin",def->skin);
    if (DispatchSpawn(entity)<0) { UTIL_Remove(entity); return; }
    entity->Activate();
    CBaseAnimating *anim=entity->GetBaseAnimating();
    if (anim && *def->model) anim->SetModel(def->model);
    if (anim && def->skin>=0) anim->m_nSkin=def->skin;
    if (!def->onFloor && !def->onCeiling && !def->noDrop) UTIL_DropToFloor(entity,MASK_NPCSOLID);
    if (def->health>0) { entity->SetHealth(def->health); entity->SetMaxHealth(def->health); }
    Msg("gmp_npc: spawned %s (%s)\n",def->id,def->entityClass);
}

CON_COMMAND(gmp_npc_cleanup, "GModPort: remove NPCs spawned by this player")
{
    CBasePlayer *player=UTIL_GetCommandClient();
    if (!player) return;
    char tag[64];
    Q_snprintf(tag,sizeof(tag),"gmp_spawned_npc_%d",player->GetUserID());
    for (CBaseEntity *e=NULL;(e=gEntList.FindEntityByName(e,tag))!=NULL;) UTIL_Remove(e);
}
