"""Restore pinned HL2 NPC weapon events while retaining HL2MP player weapons."""
from pathlib import Path
import re
from patch_npcs import function_body


def apply(root):
    changed = []
    def read(path): return (root / path).read_text(encoding='utf-8')
    def write(path, text):
        if (root / path).exists() and read(path) == text: return
        (root / path).write_text(text, encoding='utf-8', newline='\n')
        changed.append(path)
    def method(source, cls, name):
        match = re.search(r'^[\w \t*&]+\b' + cls + r'::' + name + r'\([^;{]*\)', source, re.M)
        if not match: raise RuntimeError('Missing original method: ' + cls + '::' + name)
        return match.group().strip() + '\n' + function_body(source, match.group())
    def replace_body(path, signature, body):
        text = read(path)
        old = function_body(text, signature)
        pos = text.index(old, text.index(signature))
        write(path, text[:pos] + body + text[pos + len(old):])

    path = 'game/shared/hl2mp/weapon_hl2mpbase.h'
    text = read(path)
    forward = '#ifndef CLIENT_DLL\nclass CHL2MP_Player;\n#endif\n#include "hl2mp_player_shared.h"'
    if forward not in text:
        write(path, text.replace('#include "hl2mp_player_shared.h"', forward, 1))
    # The original SP-only cast rejected MP weapons during NPC weapon searches.
    path = 'game/server/hl2/npc_citizen17.cpp'
    text = read(path)
    write(path, text.replace('CBaseHLCombatWeapon *pWeapon = dynamic_cast<CBaseHLCombatWeapon *>(Weapon_FindUsable( WEAPON_SEARCH_DELTA ));',
                             'CBaseCombatWeapon *pWeapon = Weapon_FindUsable( WEAPON_SEARCH_DELTA );'))

    methods = {
        'smg1': ('CWeaponSMG1', ['FireNPCPrimaryAttack', 'Operator_ForceNPCFire',
                                 'Operator_HandleAnimEvent', 'WeaponRangeAttack2Condition']),
        'ar2': ('CWeaponAR2', ['FireNPCPrimaryAttack', 'FireNPCSecondaryAttack',
                              'Operator_ForceNPCFire', 'Operator_HandleAnimEvent']),
        'pistol': ('CWeaponPistol', ['Operator_HandleAnimEvent']),
        'shotgun': ('CWeaponShotgun', ['FireNPCPrimaryAttack', 'Operator_ForceNPCFire',
                                      'Operator_HandleAnimEvent']),
        'rpg': ('CWeaponRPG', ['Operator_HandleAnimEvent']),
        'stunstick': ('CWeaponStunStick', []),  # Its MP event handler already exists.
    }
    for weapon, (cls, names) in methods.items():
        path = f'game/shared/hl2mp/weapon_{weapon}.cpp'
        original = read(f'game/server/hl2/weapon_{weapon}.cpp')
        marker = '// GModPort: original HL2 NPC weapon events.\n'
        if names and marker not in read(path):
            bodies = [method(original, cls, name) for name in names]
            if weapon == 'rpg':
                # MP's replicated handle is CBaseEntity, SP's is CMissile.
                bodies = [b.replace('m_hMissile->', 'static_cast<CMissile *>(m_hMissile.Get())->') for b in bodies]
            includes = '#include "ai_basenpc.h"\n#include "ai_utils.h"\n#include "soundent.h"\n'
            if weapon == 'ar2': includes += '#include "npc_combine.h"\n'
            if weapon == 'smg1':
                includes += 'extern ConVar sk_plr_dmg_smg1_grenade;\n'
                includes += re.search(r'^#define\s+COMBINE_MIN_GRENADE_CLEAR_DIST[^\n]*', original, re.M).group() + '\n'
            write(path, read(path) + '\n#ifndef CLIENT_DLL\n' + marker + includes + '\n\n'.join(bodies) + '\n#endif\n')
            declpath = f'game/shared/hl2mp/weapon_{weapon}.h' if weapon in ('ar2', 'rpg') else path
            declarations = '\n#ifndef CLIENT_DLL\npublic:\n\tint CapabilitiesGet(void) { return bits_CAP_WEAPON_RANGE_ATTACK1; }\n'
            for name in names:
                declaration = method(original, cls, name).split('\n{', 1)[0].replace(cls + '::', '')
                declarations += '\t' + declaration + ';\n'
            declarations += '#endif\n'
            text = read(declpath)
            start = text.index('{', text.index('class ' + cls + ' :')) + 1
            write(declpath, text[:start] + declarations + text[start:])

        # Include the native NPC animation mappings, keeping MP player activities.
        text = read(path)
        table = re.search(r'acttable_t\s+' + cls + r'::m_acttable\[\]\s*=\s*\{[\s\S]*?\n\};', text)
        native = re.search(r'acttable_t\s+' + cls + r'::m_acttable\[\]\s*=\s*\{[\s\S]*?\n\};', original)
        if not table or not native: raise RuntimeError('Missing activity table: ' + weapon)
        row = r'\{\s*(ACT_\w+)\s*,\s*(ACT_\w+)\s*,\s*(true|false)\s*\}'
        uncomment = lambda value: re.sub(r'//[^\n]*|/\*[\s\S]*?\*/', '', value)
        mp_rows = re.findall(row, uncomment(table.group()))
        npc_rows = re.findall(row, uncomment(native.group()))
        merged = {r[0]: r for r in mp_rows}
        merged.update({r[0]: r for r in npc_rows})
        new = f'acttable_t {cls}::m_acttable[] =\n{{\n'
        new += ''.join('\t{ %s, %s, %s },\n' % r for r in merged.values()) + '};'
        write(path, text[:table.start()] + new + text[table.end():])

    # NPC accuracy differs from the MP player's cone (notably Alyx's shotgun).
    for weapon in ('pistol', 'shotgun'):
        path = f'game/shared/hl2mp/weapon_{weapon}.cpp'
        text = read(path)
        marker = '// GModPort: native NPC bullet spread.\n'
        if marker not in text:
            old = function_body(text, 'GetBulletSpread( void )')
            original = read(f'game/server/hl2/weapon_{weapon}.cpp')
            native = function_body(original, 'GetBulletSpread( void )')
            if weapon == 'pistol':
                prefix = native[1:native.index('static Vector cone;')]
                body = '{\n' + marker + '#ifndef CLIENT_DLL\n' + prefix + '#endif\n' + old[1:]
            else:
                body = '{\n' + marker + '#ifndef CLIENT_DLL\n' + native[1:-1] + '\n#else\n' + old[1:-1] + '\n#endif\n}'
            replace_body(path, 'GetBulletSpread( void )', body)

    # Ally SMGs retain their native longer NPC range.
    path = 'game/shared/hl2mp/weapon_smg1.cpp'
    body = function_body(read('game/server/hl2/weapon_smg1.cpp'), 'void CWeaponSMG1::Equip')
    replace_body(path, 'void CWeaponSMG1::Equip', body.replace('{', '{\n#ifndef CLIENT_DLL', 1)
                 .replace('\tBaseClass::Equip', '#endif\n\tBaseClass::Equip'))

    # Citizens guide RPGs using a separate target vector; MP left these as stubs.
    path = 'game/shared/hl2mp/weapon_rpg.cpp'
    original = read('game/server/hl2/weapon_rpg.cpp')
    for name in ('UpdateNPCLaserPosition', 'SetNPCLaserPosition', 'GetNPCLaserPosition'):
        body = function_body(original, 'CWeaponRPG::' + name)
        body = body[:1] + '\n#ifndef CLIENT_DLL\n' + body[1:-1] + '\n#else\n' + (
            '\treturn vec3_origin;\n' if name == 'GetNPCLaserPosition' else '') + '#endif\n}'
        replace_body(path, 'CWeaponRPG::' + name, body)
    header = 'game/shared/hl2mp/weapon_rpg.h'
    text = read(header)
    if 'static const int EXPLOSION_RADIUS' not in text:
        text = text.replace('\tCMissile();', '\tstatic const int EXPLOSION_RADIUS = 200;\n\tCMissile();', 1)
    if 'm_vecNPCLaserDot;' not in text:
        text = text.replace('CHandle<CLaserDot>\tm_hLaserDot;', 'CHandle<CLaserDot>\tm_hLaserDot;\n\tVector m_vecNPCLaserDot;')
    if 'struct CustomDetonator_t' not in text:
        text = text.replace('protected:\n\tvirtual void DoExplosion();',
            'static void AddCustomDetonator(CBaseEntity *pEntity, float radius, float height = -1);\n'
            '\tstatic void RemoveCustomDetonator(CBaseEntity *pEntity);\n'
            'protected:\n\tstruct CustomDetonator_t { EHANDLE hEntity; float radiusSq; float halfHeight; };\n'
            '\tstatic CUtlVector<CustomDetonator_t> gm_CustomDetonators;\n\tvirtual void DoExplosion();', 1)
    write(header, text)
    text = read(path)
    if 'CMissile::gm_CustomDetonators;' not in text:
        text += '\n#ifndef CLIENT_DLL\nCUtlVector<CMissile::CustomDetonator_t> CMissile::gm_CustomDetonators;\n'
        for name in ('AddCustomDetonator', 'RemoveCustomDetonator'):
            text += method(original, 'CMissile', name) + '\n'
        text += '#endif\n'
    if 'rpg_missle_use_custom_detonators' not in text:
        native_if = 'if ( rpg_missle_use_custom_detonators.GetBool() )'
        block = native_if + '\n' + function_body(original, native_if)
        text = text.replace('//If we have a dot target', block + '\n\t//If we have a dot target', 1)
        text = text.replace('const char *g_pLaserDotThink',
            'static ConVar rpg_missle_use_custom_detonators("rpg_missle_use_custom_detonators", "1");\nconst char *g_pLaserDotThink', 1)
    # NPCs have weapon clips rather than player reserve ammo.
    text = text.replace('if ( pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )',
                        'if ( pOwner->IsPlayer() && pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )')
    write(path, text)

    # Use one actual StunStick class definition in weapon and metropolice TUs.
    path = 'game/shared/hl2mp/weapon_stunstick.cpp'
    text = read(path)
    header = 'game/shared/hl2mp/gmp_weapon_stunstick.h'
    if '#include "gmp_weapon_stunstick.h"' not in text:
        start = text.index('class CWeaponStunStick :')
        end = text.index('\n};', start) + len('\n};')
        declaration = text[start:end]
        declaration = declaration.replace('DECLARE_ACTTABLE();',
            'DECLARE_ACTTABLE();\n\tint CapabilitiesGet(void) { return bits_CAP_WEAPON_MELEE_ATTACK1; }')
        write(header, '#pragma once\n#include "weapon_hl2mpbasebasebludgeon.h"\n'
              '#define STUNSTICK_RANGE 75.0f\n#define STUNSTICK_REFIRE 0.8f\n' + declaration + '\n')
        text = text[:start] + '#include "gmp_weapon_stunstick.h"' + text[end:]
        text = re.sub(r'^#define\s+STUNSTICK_(RANGE|REFIRE)\s+[^\n]+\n', '', text, flags=re.M)
        write(path, text)
    signature = 'float CWeaponStunStick::GetDamageForActivity'
    replace_body(path, signature, '{\n#ifndef CLIENT_DLL\n'
        '\tif (GetOwner() && !GetOwner()->IsPlayer()) {\n'
        '\t\tconst ConVar *damage = cvar->FindVar("sk_npc_dmg_stunstick");\n'
        '\t\treturn damage ? damage->GetFloat() : 0.0f;\n\t}\n#endif\n\treturn 40.0f;\n}')
    text = read(path)
    if 'ConVar sk_npc_dmg_stunstick(' not in text:
        text += '\n#ifndef CLIENT_DLL\nConVar sk_npc_dmg_stunstick("sk_npc_dmg_stunstick", "0");\n#endif\n'
        write(path, text)

    # AI files resolve headers beside themselves first. Forward those headers
    # to the actual MP layouts instead of compiling incompatible SP definitions.
    for name, target in [('rpg', 'weapon_rpg.h'), ('ar2', 'weapon_ar2.h'),
                         ('stunstick', 'gmp_weapon_stunstick.h')]:
        path = f'game/server/hl2/weapon_{name}.h'
        text = read(path)
        marker = '// GModPort: match the linked HL2MP weapon layout.\n'
        if not text.startswith(marker):
            write(path, marker + '#ifdef HL2MP\n#include "../../shared/hl2mp/' + target + '"\n#else\n' + text + '\n#endif\n')
    return changed


if __name__ == '__main__':
    import sys
    print(apply(Path(sys.argv[1])))
