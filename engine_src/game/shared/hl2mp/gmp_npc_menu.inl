#include "gmp_npc_catalog.h"
static CUtlVector<GMPSpawnItem> g_GMPNPCs;
static void GMP_LoadNPCMenu()
{
    if (g_GMPNPCs.Count()) return;
    const char *category="";
    for (int i=0;i<ARRAYSIZE(s_GMPNPCs);++i)
    {
        const GMPNPCDef &def=s_GMPNPCs[i];
        if (Q_stricmp(category,def.category))
        {
            GMPSpawnItem heading;
            heading.text=def.category;
            heading.skin=0;
            g_GMPNPCs.AddToTail(heading);
            category=def.category;
        }
        GMPSpawnItem item;
        item.model=def.iconModel;
        item.text=def.label;
        char command[128];
        Q_snprintf(command,sizeof(command),"gmp_npc %s",def.id);
        item.command=command;
        item.skin=MAX(0,def.skin);
        g_GMPNPCs.AddToTail(item);
    }
}
