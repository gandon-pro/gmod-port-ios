// gm_construct single-player horror encounter. No network-server crash command.
#include "igamesystem.h"
#ifdef CLIENT_DLL
#include <cstdlib>
static bool g_GMPWatcherLocalSession = false;
static bool GMPWatcherMap()
{
    char map[MAX_PATH]; Q_FileBase(engine->GetLevelName(),map,sizeof(map));
    return !Q_stricmp(map,"gm_construct");
}
CON_COMMAND_F(gmp_watcher_finish, "Finish the local Construct encounter", FCVAR_CLIENTCMD_CAN_EXECUTE)
{
    if (g_GMPWatcherLocalSession && engine->IsInGame() && gpGlobals->maxClients == 1 &&
        GMPWatcherMap()) std::abort();
}
class CGMPWatcherLight : public CAutoGameSystemPerFrame
{
    float next;
public:
    CGMPWatcherLight() : CAutoGameSystemPerFrame("GMPWatcherLight"), next(0) {}
    void LevelShutdownPreEntity() { next=0; g_GMPWatcherLocalSession=false; }
    void FrameUpdatePostEntityThink()
    {
        if (!engine->IsInGame() || gpGlobals->maxClients != 1 ||
            !GMPWatcherMap()) return;
        if (gpGlobals->curtime<next) return;
        next=gpGlobals->curtime+.5f;
        C_BasePlayer *p=C_BasePlayer::GetLocalPlayer();
        if (!p) return;
        g_GMPWatcherLocalSession=true;
        const Vector light=engine->GetLightForPoint(p->EyePosition(),true);
        engine->ServerCmd((p->IsAlive() && light.x<.06f && light.y<.06f && light.z<.06f)
            ? "gmp_watcher_sample 1\n" : "gmp_watcher_sample 0\n");
    }
};
static CGMPWatcherLight g_GMPWatcherLight;
#else
#include "ai_basenpc.h"
#include "ai_default.h"
static ConVar gmp_construct_watcher("gmp_construct_watcher","1",FCVAR_ARCHIVE,
    "Enable the single-player Construct watcher, including its deliberate crash ending");
class CGMPConstructWatcher : public CAutoGameSystemPerFrame
{
public:
    EHANDLE npc, owner;
    float darkSince, lastSample, spawned, finishAt, next, relocateAt;
    bool dark, chasing, finished, armed;
    CGMPConstructWatcher() : CAutoGameSystemPerFrame("GMPConstructWatcher") { Reset(); }
    void Reset() { npc=NULL; owner=NULL; darkSince=-1; lastSample=-1; spawned=finishAt=next=relocateAt=0; dark=chasing=finished=armed=false; }
    void LevelInitPostEntity() { Reset(); }
    void LevelShutdownPreEntity() { Reset(); }
    bool Allowed() { return gmp_construct_watcher.GetBool() && gpGlobals->maxClients==1 &&
        !Q_stricmp(STRING(gpGlobals->mapname),"gm_construct"); }
    void Sample(CBasePlayer *p,bool value)
    {
        if (!Allowed() || p!=UTIL_GetListenServerHost()) return;
        trace_t roof;
        UTIL_TraceLine(p->EyePosition(),p->EyePosition()+Vector(0,0,2048),MASK_SOLID,p,COLLISION_GROUP_NONE,&roof);
        dark=value && roof.DidHit() && !(roof.surface.flags&SURF_SKY);
        lastSample=gpGlobals->curtime;
        if (!dark) darkSince=-1;
        else if (darkSince<0) darkSince=lastSample;
    }
    bool Visible(CBasePlayer *p,const Vector &pos)
    {
        Vector forward; p->EyeVectors(&forward);
        Vector delta=pos+Vector(0,0,60)-p->EyePosition();
        VectorNormalize(delta);
        if (DotProduct(forward,delta)<.75f) return false;
        trace_t sight;
        CTraceFilterSkipTwoEntities filter(p,npc.Get(),COLLISION_GROUP_NONE);
        UTIL_TraceLine(p->EyePosition(),pos+Vector(0,0,60),MASK_SOLID,&filter,&sight);
        return !sight.startsolid && sight.fraction>.99f;
    }
    bool FindRoof(CBasePlayer *p,Vector &pos)
    {
        for (int i=0;i<24;++i)
        {
            Vector direction; AngleVectors(QAngle(0,p->EyeAngles().y+RandomFloat(-38,38),0),&direction);
            Vector start=p->EyePosition()+direction*RandomFloat(1200,3200);
            trace_t floor, hull, sky, underside;
            UTIL_TraceLine(start+Vector(0,0,4096),start+Vector(0,0,128),MASK_SOLID_BRUSHONLY,p,COLLISION_GROUP_NONE,&floor);
            if (!floor.DidHit() || floor.startsolid || floor.plane.normal.z<.95f ||
                (floor.surface.flags&SURF_SKY)) continue;
            pos=floor.endpos+Vector(0,0,2);
            if (pos.z<p->GetAbsOrigin().z+128 || (npc && (pos-npc->GetAbsOrigin()).Length()<512)) continue;
            // Exterior top with a room underneath: reject terrain and solid ledges.
            UTIL_TraceLine(pos+Vector(0,0,74),pos+Vector(0,0,8192),MASK_SOLID_BRUSHONLY,p,COLLISION_GROUP_NONE,&sky);
            if (sky.startsolid || (sky.DidHit() && !(sky.surface.flags&SURF_SKY))) continue;
            UTIL_TraceLine(pos-Vector(0,0,192),pos-Vector(0,0,3),MASK_SOLID_BRUSHONLY,p,COLLISION_GROUP_NONE,&underside);
            if (underside.startsolid || !underside.DidHit() || underside.plane.normal.z>-.95f) continue;
            UTIL_TraceHull(pos,pos,Vector(-16,-16,0),Vector(16,16,72),MASK_NPCSOLID,p,COLLISION_GROUP_NONE,&hull);
            if (hull.startsolid || hull.allsolid || !Visible(p,pos)) continue;
            return true;
        }
        return false;
    }
    bool Spawn(CBasePlayer *p)
    {
        Vector pos;
        if (FindRoof(p,pos))
        {
            GMPNPCPrecacheScope scope;
            CBaseEntity::PrecacheModel("models/Humans/Group01/male_07.mdl");
            CBaseEntity::PrecacheSound("ambient/voices/citizen_beaten3.wav");
            CAI_BaseNPC *n=dynamic_cast<CAI_BaseNPC*>(CreateEntityByName("npc_citizen"));
            if (!n) return false;
            n->KeyValue("model","models/Humans/Group01/male_07.mdl");
            n->SetAbsOrigin(pos); n->SetAbsAngles(QAngle(0,p->EyeAngles().y+180,0));
            n->AddSpawnFlags(SF_NPC_ALWAYSTHINK);
            if (DispatchSpawn(n)<0) { UTIL_Remove(n); return false; }
            n->Activate(); n->SetRenderColor(0,0,0); n->m_takedamage=DAMAGE_NO;
            n->SetMoveType(MOVETYPE_NONE); n->SetSchedule(SCHED_IDLE_STAND);
            npc=n; owner=p; spawned=gpGlobals->curtime; relocateAt=spawned+RandomFloat(20,40);
            return true;
        }
        return false;
    }
    void FrameUpdatePostEntityThink()
    {
        if (finished) return;
        CBasePlayer *p=UTIL_GetListenServerHost();
        if (!Allowed() || !p || !p->IsAlive())
        { if (npc) UTIL_Remove(npc); Reset(); return; }
        const float now=gpGlobals->curtime;
        if (now<next) return;
        next=now+.25f;
        if (!npc)
        {
            if (spawned>0) { finished=true; return; }
            if (!armed && (lastSample<0 || now-lastSample>1.5f)) { darkSince=-1; return; }
            if (dark && darkSince>=0 && now-darkSince>60) armed=true;
            if (armed) { next=now+2; Spawn(p); }
            return;
        }
        CAI_BaseNPC *n=npc->MyNPCPointer();
        if (!n || owner!=p) { UTIL_Remove(npc); Reset(); return; }
        if (!chasing && now-spawned>=300)
        {
            chasing=true; n->SetMoveType(MOVETYPE_STEP);
            n->EmitSound("ambient/voices/citizen_beaten3.wav");
            n->AddEntityRelationship(p,D_HT,99); n->SetEnemy(p);
            n->SetSchedule(SCHED_CHASE_ENEMY);
        }
        if (!chasing)
        {
            if (now>=relocateAt && !Visible(p,n->GetAbsOrigin()))
            {
                Vector pos;
                if (FindRoof(p,pos)) n->SetAbsOrigin(pos);
                relocateAt=now+RandomFloat(20,40);
            }
            QAngle face; VectorAngles(p->EyePosition()-n->EyePosition(),face);
            n->SetAbsAngles(QAngle(0,face.y,0));
        }
        if (chasing)
        {
            n->UpdateEnemyMemory(p,p->GetAbsOrigin());
            if (!n->IsCurSchedule(SCHED_CHASE_ENEMY)) n->SetSchedule(SCHED_CHASE_ENEMY);
            if (!finishAt && (n->GetAbsOrigin()-p->GetAbsOrigin()).Length()<80) finishAt=now+1;
            if (finishAt && now>=finishAt)
            { finished=true; engine->ClientCommand(p->edict(),"gmp_watcher_finish\n"); }
        }
    }
};
static CGMPConstructWatcher g_GMPConstructWatcher;
CON_COMMAND(gmp_watcher_sample,"Local lighting sample for the Construct watcher")
{
    CBasePlayer *p=UTIL_GetCommandClient();
    if (p && args.ArgC()==2) g_GMPConstructWatcher.Sample(p,!Q_strcmp(args[1],"1"));
}
#endif
