"""Pack a local GMod install's content into the CI-built IPA (personal use only).

The content never goes through CI or GitHub; this runs on the PC that owns the
game. The IPA gets Payload/GModPort.app/gmcontent.gmpk; on first launch
ios_main.mm unpacks it into Documents/gmcontent, which resources/gameinfo.txt
mounts. Reinstalling the same IPA does not unpack again (archive id check).

The home screen icon is GMod's own (the largest PNG in gmod.exe's icon resources),
written as AppIcon*.png plus CFBundleIcons in Info.plist; it stays out of CI as well.

gmpk format (little endian):
  b'GMPK1\\0\\0\\0', u64 total file bytes, u32 file count, 16-byte archive id,
  then one zlib stream of records: u16 path length, UTF-8 path, u64 size, data;
  a record with path length 0 ends the stream.

usage: python tools/embed_content.py <GModPort-unsigned.ipa> <GarrysMod dir> <out.ipa>
"""
import argparse
import io
from pathlib import Path
import plistlib
import shutil
import struct
import sys
import uuid
import zipfile
import zlib

from PIL import Image

APP = 'Payload/GModPort.app/'
PAK = APP + 'gmcontent.gmpk'
# Same set the gameinfo.txt search paths reference. content_hl2/content_cstrike
# VPKs (2.6 GB) are not mounted by this port and are skipped.
SOURCEENGINE_VPKS = ('hl2_textures', 'hl2_sound_vo_english', 'hl2_sound_misc', 'hl2_misc')
# Machine-local state from the desktop install that must not override the phone's.
# GMod's Windows platform/ has no linux_fonts, which the POSIX vgui surface loads by
# path (DejaVu/Liberation, free licenses); take them from a sibling Half-Life 2 install.
LINUX_FONTS = 'platform/resource/linux_fonts'
# GMod's .vcs do not match this engine's shader combo layout (HUD/fonts draw nothing); a pack
# of matching .vcs picked from the GMod/HL2 VPKs by gmod_port/tools/vcs_match.py --pack.
PORTSHADERS = 'portshaders'
SKIP_GARRYSMOD = {'cache', 'download', 'downloads', 'logs', 'screenshots', 'videos', 'gameinfo.txt',
                  'cfg/config.cfg', 'console.log'}
# CFBundleIconFiles base names -> files (iPhone 60pt @2x/@3x, iPad 76pt @2x)
ICON_SIZES = {'AppIcon60x60@2x.png': 120, 'AppIcon60x60@3x.png': 180, 'AppIcon76x76@2x~ipad.png': 152}
PNG_MAGIC = b'\x89PNG\r\n\x1a\n'


def exe_icon(path):
    """Largest PNG icon (RT_ICON) in a PE file's resources, or None."""
    d = path.read_bytes()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec, optsz = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    opt = pe + 24
    datadir = opt + (112 if struct.unpack_from('<H', d, opt)[0] == 0x20b else 96)
    rva = struct.unpack_from('<I', d, datadir + 2 * 8)[0]  # IMAGE_DIRECTORY_ENTRY_RESOURCE
    secs = [struct.unpack_from('<8sIIII', d, opt + optsz + i * 40) for i in range(nsec)]

    def off(r):
        return next(r - va + raw for _, vs, va, rs, raw in secs if va <= r < va + max(vs, rs))

    def entries(o):
        named, ids = struct.unpack_from('<HH', d, o + 12)
        return [struct.unpack_from('<II', d, o + 16 + i * 8) for i in range(named + ids)]

    base = off(rva)
    best = None
    for kind, p in entries(base):
        if kind != 3:  # RT_ICON
            continue
        for _, p2 in entries(base + (p & 0x7fffffff)):
            for _, p3 in entries(base + (p2 & 0x7fffffff)):
                data_rva, size = struct.unpack_from('<II', d, base + p3)
                data = d[off(data_rva):off(data_rva) + size]
                if data.startswith(PNG_MAGIC) and (best is None or len(data) > len(best)):
                    best = data
    return best


def icon_files(png):
    """Opaque square icons (iOS rounds the corners); the transparent corners take the icon's colour."""
    image = Image.open(io.BytesIO(png)).convert('RGBA')
    square = Image.new('RGBA', image.size, image.getpixel((image.width // 2, image.height // 8)))
    square.alpha_composite(image)
    out = {}
    for name, size in ICON_SIZES.items():
        buf = io.BytesIO()
        square.convert('RGB').resize((size, size), Image.LANCZOS).save(buf, 'PNG')
        out[APP + name] = buf.getvalue()
    return out


def with_icon(plist_bytes):
    info = plistlib.loads(plist_bytes)
    info['CFBundleIcons'] = {'CFBundlePrimaryIcon': {'CFBundleIconFiles': ['AppIcon60x60']}}
    info['CFBundleIcons~ipad'] = {'CFBundlePrimaryIcon': {'CFBundleIconFiles': ['AppIcon60x60', 'AppIcon76x76']}}
    fmt = plistlib.FMT_BINARY if plist_bytes.startswith(b'bplist') else plistlib.FMT_XML
    return plistlib.dumps(info, fmt=fmt)


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
    parser.add_argument('--shaders', type=Path, help='vcs_match.py --pack output (default: ../out/portshaders)')
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
    shaders = args.shaders or Path(__file__).resolve().parents[2] / 'out' / PORTSHADERS
    if (shaders / 'shaders/fxc').is_dir():
        # shaders/fxc from vcs_match.py --pack, materials/gmp from make_port_assets.py
        for path in sorted(shaders.rglob('*')):
            if path.is_file():
                files.append((path, f'{PORTSHADERS}/{path.relative_to(shaders).as_posix()}'))
        if not (shaders / 'materials/gmp').is_dir():
            print('warning: no menu art in the port pack; run tools/make_port_assets.py')
    else:
        print(f'warning: no shader pack at {shaders}; HUD and text will not render')
    exe = args.gmod / 'gmod.exe'
    icon = exe_icon(exe) if exe.is_file() else None
    icons = icon_files(icon) if icon else {}
    if not icons:
        print(f'warning: no PNG icon in {exe}; the app keeps the default icon')
    total = sum(p.stat().st_size for p, _ in files)
    print(f'{len(files)} files, {total / 2**30:.2f} GiB uncompressed')
    tmp = args.output.with_suffix('.tmp')
    with zipfile.ZipFile(args.ipa) as src, zipfile.ZipFile(tmp, 'w', allowZip64=True) as dst:
        for info in src.infolist():
            if info.filename == PAK or info.filename in icons:
                continue
            if icons and info.filename == APP + 'Info.plist':
                dst.writestr(info, with_icon(src.read(info)))
                continue
            with src.open(info) as r, dst.open(info, 'w', force_zip64=True) as w:
                shutil.copyfileobj(r, w, 1 << 20)
        for name, data in icons.items():
            entry = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            dst.writestr(entry, data)
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
