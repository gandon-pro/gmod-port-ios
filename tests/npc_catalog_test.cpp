#include "gmp_npc_catalog.h"
#include <cassert>
#include <cstring>
static const GMPNPCDef& find(const char* id) {
    for (const auto& n:s_GMPNPCs) if (!std::strcmp(n.id,id)) return n;
    assert(false); return s_GMPNPCs[0];
}
int main() {
    assert(sizeof(s_GMPNPCs)/sizeof(s_GMPNPCs[0])==61);
    for (const auto& n:s_GMPNPCs) {
        assert(*n.entityClass && *n.iconModel && n.offset>=0);
        assert(!std::strncmp(n.entityClass,"npc_",4));
        int count=0;
        for (const auto& other:s_GMPNPCs) if (!std::strcmp(other.id,n.id)) ++count;
        assert(count==1);
    }
    assert(find("Medic").spawnFlags==(131072|8));
    assert(find("npc_turret_floor").totalFlags==0);
    assert(find("npc_turret_floor_resistance").totalFlags==512);
    assert(find("npc_barnacle").onCeiling && !find("npc_barnacle").onFloor);
    assert(find("npc_manhack_hacked").spawnFlags==2097152);
    assert(find("npc_rollermine_hacked").spawnFlags==262144);
    assert(find("ShotgunSoldier").skin==1);
    assert(!std::strcmp(find("npc_antlion_worker").entityClass,"npc_antlion_worker"));
}
