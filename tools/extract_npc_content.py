"""Pull the episodic NPC assets the phone's content pack lacks out of a local GarrysMod install.

GMod keeps Episode 1/2 content (hunter, zombine, antlion worker/grub, Magnusson...) in
sourceengine/content_hl2_*.vpk, which embed_content.py does not ship (1.4 GB). This copies only
what those NPCs reference -- models with their .vvd/.vtx/.phy/.ani and $includemodels, the
materials and textures named in the MDLs and VMTs, their sounds, sound scripts and particles --
into a loose folder that embed_content.py packs as gmcontent/npcpack (lowest-priority search path).

Personal use: the output is Valve content and stays on this machine, never in git or CI.

  py -3.13 tools/extract_npc_content.py "E:/SteamLibrary/steamapps/common/GarrysMod" ../out/npcpack
"""
import argparse
import re
import struct
import sys
from pathlib import Path

# Models the NPC catalog / server code use that are only in content_hl2.
SEED_MODELS = [
    'models/hunter.mdl', 'models/weapons/hunter_flechette.mdl',
    'models/zombie/zombie_soldier.mdl', 'models/zombie/zombie_soldier_animations.mdl',
    'models/zombie/fast_torso.mdl',
    'models/antlion_worker.mdl', 'models/antlion_grub.mdl', 'models/antlion_grub_squashed.mdl',
    'models/magnusson.mdl', 'models/eli.mdl', 'models/vortigaunt_doctor.mdl', 'models/vortigaunt_slave.mdl',
    'models/combine_turrets/citizen_turret.mdl',
]
SEED_PREFIXES = [
    'models/gibs/antlion_worker_gibs', 'models/gibs/hunter_gibs', 'models/gibs/hunter_',
    'sound/npc/ministrider/', 'sound/npc/zombine/', 'sound/npc/antlion_grub/', 'sound/npc/hunter/',
    'sound/npc/antlion/antlion_worker', 'sound/npc/antlion/digdown', 'sound/npc/antlion/digup',
    'materials/effects/hunter', 'materials/models/hunter', 'materials/models/ministrider',
    'materials/particle/impacteffect/huntershield',
    'particles/hunter_', 'particles/antlion_worker', 'particles/antlion_gib', 'particles/grub_blood',
    'scripts/npc_sounds_zombine', 'scripts/npc_sounds_hunter', 'scripts/npc_sounds_ministrider',
    'scripts/npc_sounds_antlion_episodic', 'scripts/npc_sounds_antliongrub',
]
MODEL_EXTS = ('.mdl', '.vvd', '.dx90.vtx', '.dx80.vtx', '.sw.vtx', '.vtx', '.phy', '.ani')
SHIPPED_VPKS = ['garrysmod/garrysmod_dir.vpk', 'garrysmod/fallbacks_dir.vpk', 'sourceengine/hl2_misc_dir.vpk',
                'sourceengine/hl2_textures_dir.vpk', 'sourceengine/hl2_sound_misc_dir.vpk',
                'sourceengine/hl2_sound_vo_english_dir.vpk']
SOURCE_VPK = 'sourceengine/content_hl2_dir.vpk'


class Vpk:
    def __init__(self, path):
        self.path = Path(path)
        d = self.path.read_bytes()
        sig, ver, tree = struct.unpack_from('<III', d, 0)
        if sig != 0x55AA1234:
            sys.exit(f'{path}: not a VPK')
        off = 12 if ver == 1 else 28
        self.data_start = off + tree
        self.entries = {}

        def cstr():
            nonlocal off
            e = d.index(b'\0', off)
            r = d[off:e].decode('latin1')
            off = e + 1
            return r
        while ext := cstr():
            while path_ := cstr():
                while name := cstr():
                    crc, pre, archive, eoff, elen, term = struct.unpack_from('<IHHIIH', d, off)
                    off += 18
                    preload = d[off:off + pre]
                    off += pre
                    full = (path_.strip() + '/' if path_.strip() else '') + name + '.' + ext
                    self.entries[full.lower()] = (archive, eoff, elen, preload)

    def read(self, name):
        archive, eoff, elen, preload = self.entries[name]
        if not elen:
            return preload
        if archive == 0x7FFF:
            src = self.path
            eoff += self.data_start
        else:
            src = self.path.with_name(self.path.name.replace('_dir.vpk', f'_{archive:03d}.vpk'))
        with open(src, 'rb') as f:
            f.seek(eoff)
            return preload + f.read(elen)


def mdl_refs(data):
    """Material search dirs, texture names and $includemodels of a studio header (v44-49)."""
    if data[:4] != b'IDST':
        return [], [], []

    def cstr(o):
        return data[o:data.index(b'\0', o)].decode('latin1')
    # studiohdr_t: id, version, checksum, name[64], length, 6 vectors (72 bytes), flags, ...
    o = 4 + 4 + 4 + 64 + 4 + 72 + 4
    (numbones, boneindex, numbonecontrollers, bonecontrollerindex, numhitboxsets, hitboxsetindex,
     numlocalanim, localanimindex, numlocalseq, localseqindex, activitylistversion, eventsindexed,
     numtextures, textureindex, numcdtextures, cdtextureindex) = struct.unpack_from('<16i', data, o)
    o += 16 * 4
    o += 4 * 4      # numskinref, numskinfamilies, skinindex, numbodyparts
    o += 4 * 1      # bodypartindex
    o += 4 * 2      # numlocalattachments, localattachmentindex
    o += 4 * 3      # numlocalnodes, localnodeindex, localnodenameindex
    o += 4 * 2      # numflexdesc, flexdescindex
    o += 4 * 2      # numflexcontrollers, flexcontrollerindex
    o += 4 * 2      # numflexrules, flexruleindex
    o += 4 * 2      # numikchains, ikchainindex
    o += 4 * 2      # nummouths, mouthindex
    o += 4 * 2      # numlocalposeparameters, localposeparamindex
    o += 4          # surfacepropindex
    o += 4 * 2      # keyvalueindex, keyvaluesize
    o += 4 * 2      # numlocalikautoplaylocks, localikautoplaylockindex
    o += 4 * 2      # mass, contents
    numincludemodels, includemodelindex = struct.unpack_from('<2i', data, o)
    textures = []
    for i in range(numtextures):
        t = textureindex + i * 64   # mstudiotexture_t: sznameindex, flags, used, unused, material, clientmaterial, unused[10]
        textures.append(cstr(t + struct.unpack_from('<i', data, t)[0]))
    cddirs = [cstr(struct.unpack_from('<i', data, cdtextureindex + i * 4)[0]) for i in range(numcdtextures)]
    includes = []
    for i in range(numincludemodels):
        t = includemodelindex + i * 8   # mstudiomodelgroup_t: szlabelindex, sznameindex
        includes.append(cstr(t + struct.unpack_from('<i', data, t + 4)[0]))
    return cddirs, textures, includes


def norm(p):
    p = p.replace('\\', '/').strip().lower()
    while '//' in p:
        p = p.replace('//', '/')
    return p.lstrip('/')


def extract(gmod, out):
    src = Vpk(gmod / SOURCE_VPK)
    shipped = set()
    for v in SHIPPED_VPKS:
        if (gmod / v).is_file():
            shipped |= set(Vpk(gmod / v).entries)

    def available(name):
        return name in src.entries and name not in shipped

    take = set()
    queue = [m for m in SEED_MODELS]
    for p in SEED_PREFIXES:
        for name in src.entries:
            if name.startswith(p) and name not in shipped:
                if name.endswith('.mdl'):
                    queue.append(name)
                else:
                    take.add(name)
    seen = set()
    vmts = []
    while queue:
        m = norm(queue.pop())
        if m in seen:
            continue
        seen.add(m)
        if m not in src.entries:
            if m not in shipped:
                print(f'warning: {m} is in no VPK')
            continue
        base = m[:-4]
        for ext in MODEL_EXTS:
            if available(base + ext):
                take.add(base + ext)
        cddirs, textures, includes = mdl_refs(src.read(m))
        queue += includes
        for t in textures:
            for d in cddirs or ['']:
                vmt = norm(f'materials/{d}/{t}.vmt')
                if vmt in src.entries or vmt in shipped:
                    vmts.append(vmt)
                    break
    for name in list(take):
        if name.endswith('.vmt'):
            vmts.append(name)
    done = set()
    while vmts:
        vmt = vmts.pop()
        if vmt in done:
            continue
        done.add(vmt)
        if not available(vmt):
            continue
        take.add(vmt)
        text = src.read(vmt).decode('latin1', 'replace')
        for key, val in re.findall(r'"?(\$\w+|include)"?\s+"?([^"\r\n]+?)"?\s*(?:\r?\n|$)', text, re.I):
            val = norm(val)
            if key.lower() == 'include':
                vmts.append(val)
            elif re.match(r'^[\w./-]+$', val) and not re.match(r'^[\d.\s-]+$', val):
                vtf = 'materials/' + (val[:-4] if val.endswith('.vtf') else val) + '.vtf'
                if available(vtf):
                    take.add(vtf)
    total = 0
    for name in sorted(take):
        data = src.read(name)
        dst = out / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(data)
        total += len(data)
    print(f'npc content: {len(take)} files, {total / 2**20:.1f} MB -> {out}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('gmod', type=Path)
    ap.add_argument('out', type=Path)
    args = ap.parse_args()
    extract(args.gmod, args.out)


if __name__ == '__main__':
    main()
