"""Compile the exported Facepunch NPC metadata into a native spawn catalog."""
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def generate():
    rows=[json.loads(s) for s in (ROOT/'resources/npcs/catalog.jsonl').read_text(encoding='utf-8-sig').splitlines() if s.strip()]
    models=json.loads((ROOT/'resources/npcs/default_models.json').read_text())
    assert len({r['id'] for r in rows})==len(rows)
    categories={'#spawnmenu.category.humans_resistance':'Humans / Resistance',
                '#spawnmenu.category.zombies_aliens':'Zombies / Aliens',
                '#spawnmenu.category.animals':'Animals', '#spawnmenu.category.combine':'Combine'}
    q=lambda s:json.dumps(str(s),ensure_ascii=True)
    lines=['#pragma once', '// Generated from Facepunch base_npcs.lua; do not edit entries by hand.',
           'struct GMPNPCKey { const char *key, *value; };',
           'struct GMPNPCDef { const char *id, *entityClass, *label, *category, *model, *iconModel; '
           'int spawnFlags, totalFlags, skin, health; float offset, rotateYaw; '
           'bool onFloor, onCeiling, noDrop; GMPNPCKey keys[8]; const char *weapons[8]; };',
           'static const GMPNPCDef s_GMPNPCs[] = {']
    assets=[]
    for r in rows:
        assert re.fullmatch(r'[A-Za-z0-9_]+',r['id']) and re.fullmatch(r'[A-Za-z0-9_]+',r['Class'])
        assert r['Class'] in models
        keys=r.get('KeyValues',{})
        weapons=r.get('Weapons',[])
        assert len(keys)<=8 and len(weapons)<=8
        label=r['id'].removeprefix('npc_').replace('_',' ').title()
        label={'Rebel':'Rebel','Medic':'Medic','Refugee':'Refugee','VortigauntSlave':'Vortigaunt Slave',
               'VortigauntUriah':'Uriah','ShotgunSoldier':'Shotgun Soldier','CombinePrison':'Prison Guard',
               'PrisonShotgunner':'Prison Shotgunner','CombineElite':'Combine Elite'}.get(r['id'],label)
        icon=r.get('Model',models[r['Class']])
        if r['Class']=='npc_citizen' and not r.get('Model'):
            group={1:'group01',2:'group02',3:'group03'}.get(keys.get('citizentype'), 'group01')
            if r['id']=='Medic':group='group03m'
            icon=f'models/humans/{group}/male_07.mdl'
        if r['id']=='npc_turret_floor_resistance':icon='models/combine_turrets/citizen_turret.mdl'
        values=[q(r['id']),q(r['Class']),q(label),q(categories[r['Category']]),q(r.get('Model','')),q(icon),
                str(r.get('SpawnFlags',0)),str(r.get('TotalSpawnFlags',-1)),str(r.get('Skin',-1)),str(r.get('Health',0)),
                str(r.get('Offset',32)),str(r.get('Rotate',{}).get('yaw',0)),
                *[str(bool(r.get(k,False))).lower() for k in ('OnFloor','OnCeiling','NoDrop')],
                '{'+','.join('{'+q(k)+','+q(v)+'}' for k,v in sorted(keys.items()))+'}',
                '{'+','.join(q(w) for w in weapons)+'}']
        lines.append(' {'+','.join(values)+'},')
        assets.append({'id':r['id'],'entity_class':r['Class'],'preview_model':icon,
                       'model_override':r.get('Model'),'weapons':weapons})
    lines.append('};')
    output=ROOT/'engine_src/game/shared/hl2mp/gmp_npc_catalog.h'
    output.write_text('\n'.join(lines)+'\n')
    (ROOT/'docs/npc_assets_for_claude.json').write_text(json.dumps({'npc_variants':len(rows),
        'native_classes':len(models), 'content_only':True,
        'required_groups':['models with .vvd/.vtx/.phy and referenced animation MDLs',
                           'referenced materials and textures', 'NPC weapon models and scripts',
                           'scripts/npc_sounds*.txt and their sound/ files',
                           'particles/ files for antlion workers, hunters and strider busters',
                           'cfg/skill_manifest.cfg, skill.cfg, skill1.cfg, skill2.cfg, skill3.cfg',
                           'maps/graphs/*.ain when supplied with the map'],
        'entries':assets},indent=2)+'\n')
    print(f'Generated {len(rows)} NPC variants / {len(models)} native classes')

if __name__=='__main__':generate()
