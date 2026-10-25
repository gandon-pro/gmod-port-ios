"""Connect stock HL2/episodic NPC AI to the HL2MP game rules, not replacement AI."""
from pathlib import Path
import re

NPC_SERVER_SOURCES=['hl2/npc_fastzombie.cpp','hl2/npc_zombine.cpp',
    'hl2/npc_antliongrub.cpp','hl2/grenade_spit.cpp',
    'episodic/npc_hunter.cpp','episodic/npc_magnusson.cpp','episodic/weapon_striderbuster.cpp',
    'hl2/weapon_alyxgun.cpp']

def function_body(text, signature):
    start=text.index('{',text.index(signature))
    # C++ strings/comments must not contribute braces to the extraction.
    tokens=re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',text[start:])
    level=0
    for token in tokens:
        if token.group()=='{':level+=1
        elif token.group()=='}':
            level-=1
            if level==0:return text[start:start+token.end()]
    raise RuntimeError('Unbalanced function: '+signature)

def apply(root):
    changed=[]
    def write(path,text):
        file=root/path
        if file.exists() and file.read_text(encoding='utf-8')==text:return
        file.parent.mkdir(parents=True,exist_ok=True)
        file.write_text(text,encoding='utf-8',newline='\n')
        changed.append(path)
    def replace(path,before,after):
        text=(root/path).read_text(encoding='utf-8')
        if after in text:return
        if text.count(before)!=1:raise RuntimeError('Unexpected NPC context: '+path+' / '+before[:60])
        write(path,text.replace(before,after))

    original=(root/'game/shared/hl2/hl2_gamerules.cpp').read_text(encoding='utf-8')
    relationships=function_body(original,'void CHalfLife2::InitDefaultAIRelationships')
    names=function_body(original,'const char* CHalfLife2::AIClassText')
    # NPC bullets otherwise retain HL2MP's zero NPC damage. Keep player damage
    # and carrying limits intact, and use the original skill convars dynamically.
    bindings=[]
    for match in re.finditer(r'def\.AddAmmoType\("([^"]+)"[^;]+;',original):
        args=re.findall(r'"([^"]*)"',match.group())
        if len(args)>=3 and args[2].startswith('sk_'):
            bindings.append((args[0],args[2]))
    setup='\n\t// NPC skill damage bindings from the original HL2 ammo definitions.\n'
    for ammo,cvar in dict(bindings).items():
        setup+=f'\tif ( Ammo_t *a = GetAmmoDef()->GetAmmoOfIndex(GetAmmoDef()->Index("{ammo}")) )\n'
        setup+=f'\t\tif ( const ConVar *v = cvar->FindVar("{cvar}") ) {{ a->pNPCDmg = USE_CVAR; a->pNPCDmgCVar = v; }}\n'
    relationships=relationships[:-1]+setup+'}\n'
    write('game/shared/hl2mp/gmp_npc_relationships.inl',
          '// Generated from pinned HL2 game rules; retains its complete relationship table.\n'
          '#ifndef CLIENT_DLL\nvoid CHL2MPRules::InitDefaultAIRelationships( void )\n'+relationships+
          '\nconst char* CHL2MPRules::AIClassText(int classType)\n'+names+'\n#endif\n')
    replace('game/shared/hl2mp/hl2mp_gamerules.h',
            '\tDECLARE_CLASS( CHL2MPRules, CTeamplayRules );',
            '\tDECLARE_CLASS( CHL2MPRules, CTeamplayRules );\n#ifndef CLIENT_DLL\n'
            '\tvirtual void InitDefaultAIRelationships( void );\n\tvirtual const char* AIClassText(int classType);\n#endif')
    path='game/shared/hl2mp/hl2mp_gamerules.cpp'
    text=(root/path).read_text(encoding='utf-8')
    if '#include "gmp_npc_relationships.inl"' not in text:
        write(path,text+'\n#include "gmp_npc_relationships.inl"\n')
    # Missing aircraft / strider / Alyx ammo types come from the same source.
    text=(root/path).read_text(encoding='utf-8')
    existing=set(re.findall(r'def\.AddAmmoType\("([^"]+)"',text))
    additions=[]
    for match in re.finditer(r'^\s*def\.AddAmmoType\("([^"]+)"[^;]+;',original,re.M):
        name=match.group(1)
        if name in existing:continue
        additions.append(match.group().strip())
        existing.add(name)
    if additions:
        replace(path,'\treturn &def;',
                '\t// Original HL2 NPC-specific ammunition; add each type once.\n'
                '\tstatic bool npcAmmo = false;\n\tif ( !npcAmmo ) { npcAmmo = true;\n'+
                '\n'.join('\t\t'+a for a in additions)+'\n\t}\n\treturn &def;')
    # Enable the native worker and guardian portions without changing the whole
    # game to single-player episodic mode. Shared class layouts use the same flag.
    for path in ['game/server/hl2/npc_antlion.cpp','game/server/hl2/npc_antlion.h',
                 'game/server/hl2/npc_antlionguard.cpp','game/client/hl2/c_npc_antlionguard.cpp']:
        text=(root/path).read_text(encoding='utf-8')
        text=re.sub(r'^([ \t]*)#(?:ifdef|if)\s+HL2_EPISODIC\b([^\n]*)',
                    r'\1#if defined(HL2_EPISODIC) || defined(GMP_EPISODIC_NPCS)\2',text,flags=re.M)
        write(path,text)
    replace('game/server/hl2/npc_antlion.cpp',
            'LINK_ENTITY_TO_CLASS( npc_antlion, CNPC_Antlion );',
            'LINK_ENTITY_TO_CLASS( npc_antlion, CNPC_Antlion );\nLINK_ENTITY_TO_CLASS( npc_antlion_worker, CNPC_Antlion );')
    replace('game/server/hl2/npc_antlion.cpp', 'void CNPC_Antlion::Spawn( void )\n{',
            'void CNPC_Antlion::Spawn( void )\n{\n\tif ( ClassMatches("npc_antlion_worker") ) AddSpawnFlags(SF_ANTLION_WORKER);')
    # The sandbox has no single-player Alyx darkness mission state.
    replace('game/server/hl2/npc_zombine.cpp',
            'HL2GameRules()->IsAlyxInDarknessMode() && pPlayer->FlashlightIsOn() == false',
            '!g_pGameRules->IsMultiplayer() && HL2GameRules()->IsAlyxInDarknessMode() && pPlayer->FlashlightIsOn() == false')
    # GMod's published spawn flags for friendly hacks and Breen's enemy variant.
    replace('game/server/hl2/npc_manhack.cpp','\tm_bHackedByAlyx = false;',
            '\tm_bHackedByAlyx = HasSpawnFlags(2097152); // SF_MANHACK_HACKED from GMod')
    replace('game/server/hl2/npc_rollermine.cpp','void CNPC_RollerMine::Spawn( void )\n{',
            'void CNPC_RollerMine::Spawn( void )\n{\n\tif ( HasSpawnFlags(262144) ) m_bHackedByAlyx = true;')
    replace('game/server/hl2/npc_breen.cpp','\treturn\tCLASS_NONE;',
            '\treturn HasSpawnFlags(131072) ? CLASS_COMBINE : CLASS_NONE;')
    citizen_header='game/server/hl2/npc_citizen17.h'
    text=(root/citizen_header).read_text(encoding='utf-8')
    if '\tbool m_bGMPHostile;' in text:
        write(citizen_header,text.replace('\tbool m_bGMPHostile;','\tbool m_bGMPHostile = false;'))
    replace(citizen_header,'class CNPC_Citizen : public CNPC_PlayerCompanion\n{',
            'class CNPC_Citizen : public CNPC_PlayerCompanion\n{\n\tbool m_bGMPHostile = false;')
    replace('game/server/hl2/npc_citizen17.cpp','BEGIN_DATADESC( CNPC_Citizen )',
            'BEGIN_DATADESC( CNPC_Citizen )\n\tDEFINE_KEYFIELD( m_bGMPHostile, FIELD_BOOLEAN, "Hostile" ),')
    replace('game/server/hl2/npc_citizen17.cpp','Class_T\tCNPC_Citizen::Classify()\n{',
            'Class_T\tCNPC_Citizen::Classify()\n{\n\tif ( m_bGMPHostile ) return CLASS_COMBINE;')
    return changed

if __name__=='__main__':
    import sys
    print(apply(Path(sys.argv[1])))
