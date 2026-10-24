"""Pack a local GMod install's content into the CI-built IPA (personal use only).

The content never goes through CI or GitHub; this runs on the PC that owns the
game. The IPA gets Payload/GModPort.app/gmcontent.gmpk; on first launch
ios_main.mm unpacks it into Documents/gmcontent, which resources/gameinfo.txt
mounts. Reinstalling the same IPA does not unpack again (archive id check).

gmpk format (little endian):
  b'GMPK1\\0\\0\\0', u64 total file bytes, u32 file count, 16-byte archive id,
  then one zlib stream of records: u16 path length, UTF-8 path, u64 size, data;
  a record with path length 0 ends the stream.

usage: python tools/embed_content.py <GModPort-unsigned.ipa> <GarrysMod dir> <out.ipa>
"""
import argparse
from pathlib import Path
import shutil
import struct
import sys
import uuid
import zipfile
import zlib

APP = 'Payload/GModPort.app/'
PAK = APP + 'gmcontent.gmpk'
# Same set the gameinfo.txt search paths reference. content_hl2/content_cstrike
# VPKs (2.6 GB) are not mounted by this port and are skipped.
SOURCEENGINE_VPKS = ('hl2_textures', 'hl2_sound_vo_english', 'hl2_sound_misc', 'hl2_misc')
# Machine-local state from the desktop install that must not override the phone's.
# GMod's Windows platform/ has no linux_fonts, which the POSIX vgui surface loads by
# path (DejaVu/Liberation, free licenses); take them from a sibling Half-Life 2 install.
LINUX_FONTS = 'platform/resource/linux_fonts'
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
    parser.add_argument('--fonts', type=Path, help='linux_fonts dir (default: ../Half-Life 2/' + LINUX_FONTS + ')')
    parser.add_argument('--level', type=int, default=6, help='zlib level (1 fastest, 9 smallest)')
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
    fonts = args.fonts or args.gmod.parent / 'Half-Life 2' / LINUX_FONTS
    have = {rel for _, rel in files}
    if fonts.is_dir():
        for path in sorted(fonts.glob('*.ttf')):
            rel = f'{LINUX_FONTS}/{path.name}'
            if rel not in have and not path.name.startswith('noto'):
                files.append((path, rel))
    else:
        print(f'warning: no linux_fonts at {fonts}; vgui text will not render')
    total = sum(p.stat().st_size for p, _ in files)
    print(f'{len(files)} files, {total / 2**30:.2f} GiB uncompressed')
    tmp = args.output.with_suffix('.tmp')
    with zipfile.ZipFile(args.ipa) as src, zipfile.ZipFile(tmp, 'w', allowZip64=True) as dst:
        for info in src.infolist():
            if info.filename == PAK:
                continue
            with src.open(info) as r, dst.open(info, 'w', force_zip64=True) as w:
                shutil.copyfileobj(r, w, 1 << 20)
        entry = zipfile.ZipInfo(PAK, date_time=(2026, 1, 1, 0, 0, 0))
        entry.compress_type = zipfile.ZIP_STORED  # payload is already deflated
        entry.external_attr = 0o644 << 16
        with dst.open(entry, 'w', force_zip64=True) as w:
            w.write(b'GMPK1\0\0\0' + struct.pack('<QI', total, len(files)) + uuid.uuid4().bytes)
            z = zlib.compressobj(args.level)
            done = 0
            for path, rel in files:
                name = rel.encode()
                w.write(z.compress(struct.pack('<H', len(name)) + name + struct.pack('<Q', path.stat().st_size)))
                with open(path, 'rb') as f:
                    while chunk := f.read(4 << 20):
                        w.write(z.compress(chunk))
                        done += len(chunk)
                        print(f'\r{done * 100 // total:3d}%', end='', flush=True)
            w.write(z.compress(struct.pack('<H', 0)) + z.flush())
    print()
    tmp.replace(args.output)
    print(f'wrote {args.output} ({args.output.stat().st_size / 2**30:.2f} GiB)')


if __name__ == '__main__':
    main()
