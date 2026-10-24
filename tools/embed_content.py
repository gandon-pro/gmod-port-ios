"""Embed a local GMod install's content into the CI-built IPA (personal use only).

The content never goes through CI or GitHub; this runs on the PC that owns the
game. Result: Payload/GModPort.app/content/{garrysmod,sourceengine,platform}.
On launch ios_main.mm links Documents/gmcontent to that folder, and
resources/gameinfo.txt mounts everything through gmcontent/.

usage: python tools/embed_content.py <GModPort-unsigned.ipa> <GarrysMod dir> <out.ipa>
"""
import argparse
from pathlib import Path
import shutil
import sys
import zipfile

APP = 'Payload/GModPort.app/'
# Same set the gameinfo.txt search paths reference. content_hl2/content_cstrike
# VPKs (2.6 GB) are not mounted by this port and are skipped.
SOURCEENGINE_VPKS = ('hl2_textures', 'hl2_sound_vo_english', 'hl2_sound_misc', 'hl2_misc')
# Machine-local state from the desktop install that must not override the phone's.
SKIP_GARRYSMOD = {'cache', 'download', 'downloads', 'logs', 'screenshots', 'videos', 'gameinfo.txt',
                  'cfg/config.cfg', 'console.log'}


def wanted(rel):
    parts = rel.split('/')
    top = parts[0]
    if top == 'platform':
        return True
    if top == 'garrysmod':
        sub = '/'.join(parts[1:])
        return not any(sub == s or sub.startswith(s + '/') for s in SKIP_GARRYSMOD)
    if top == 'sourceengine':
        if len(parts) > 2:  # loose folders: resource/, scripts/
            return True
        name = parts[1]
        return name.endswith('.vpk') and name.rsplit('_', 1)[0] in SOURCEENGINE_VPKS
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('ipa', type=Path)
    parser.add_argument('gmod', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    for top in ('garrysmod', 'sourceengine', 'platform'):
        if not (args.gmod / top).is_dir():
            sys.exit(f'missing {args.gmod / top}')
    if not (args.gmod / 'garrysmod/garrysmod_dir.vpk').is_file():
        sys.exit('garrysmod_dir.vpk not found: not a GarrysMod install')
    files = []
    for top in ('garrysmod', 'sourceengine', 'platform'):
        for path in sorted((args.gmod / top).rglob('*')):
            rel = path.relative_to(args.gmod).as_posix()
            if path.is_file() and wanted(rel):
                files.append((path, rel))
    total = sum(p.stat().st_size for p, _ in files)
    print(f'{len(files)} files, {total / 2**30:.2f} GiB')
    tmp = args.output.with_suffix('.tmp')
    with zipfile.ZipFile(args.ipa) as src, zipfile.ZipFile(tmp, 'w', allowZip64=True) as dst:
        for info in src.infolist():
            if info.filename.startswith(APP + 'content/'):
                continue
            with src.open(info) as r, dst.open(info, 'w', force_zip64=True) as w:
                shutil.copyfileobj(r, w, 1 << 20)
        done = 0
        for path, rel in files:
            # VPK archives and audio are already dense; storing keeps repacking fast.
            method = zipfile.ZIP_STORED if path.suffix in ('.vpk', '.wav', '.mp3', '.bsp') else zipfile.ZIP_DEFLATED
            dst.write(path, APP + 'content/' + rel, compress_type=method, compresslevel=1)
            done += path.stat().st_size
            print(f'\r{done * 100 // total:3d}%', end='', flush=True)
    print()
    tmp.replace(args.output)
    print(f'wrote {args.output} ({args.output.stat().st_size / 2**30:.2f} GiB)')


if __name__ == '__main__':
    main()
