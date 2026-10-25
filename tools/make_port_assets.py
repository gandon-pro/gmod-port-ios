"""Convert GMod's own menu art into Source textures for the native GModPort menus.

GMod draws its main menu and spawn menu in HTML/Lua from PNGs, which vgui cannot load.
This writes VTF 7.2 RGBA8888 copies next to the shader pack (out/portshaders), which
embed_content.py packs into gmcontent/portshaders. The art is GMod content: run it locally
against your own install; the output never goes to the repository or CI.

usage: python tools/make_port_assets.py <GarrysMod dir> [out/portshaders]
"""
import io
import struct
import sys
from pathlib import Path

from PIL import Image

from extract_npc_content import Vpk

# TEXTUREFLAGS_CLAMPS | CLAMPT | NOMIP | NOLOD | EIGHTBITALPHA
FLAGS = 0x4 | 0x8 | 0x100 | 0x200 | 0x2000
IMAGE_FORMAT_RGBA8888 = 0
IMAGE_FORMAT_UNKNOWN = -1
# name -> (source relative to the install, or 'vpk:' + a path inside garrysmod_dir.vpk; power-of-two
# size it is stretched to; the vgui panels draw it back at the source aspect)
ASSETS = {
    'logo': ('garrysmod/gamemodes/sandbox/logo.png', (512, 256)),
    'playermodel': ('vpk:materials/icon64/playermodel.png', (64, 64)),   # C menu desktop icon
}
VMT = '"UnlitGeneric"\n{\n\t"$basetexture" "gmp/%s"\n\t"$translucent" 1\n\t"$vertexcolor" 1\n\t"$vertexalpha" 1\n\t"$ignorez" 1\n\t"$nolod" 1\n}\n'


def vtf(image):
    w, h = image.size
    header = struct.pack('<4s3I2HI2H4x3f4xfiBiBBH', b'VTF\0', 7, 2, 80, w, h, FLAGS, 1, 0,
                         0.5, 0.5, 0.5, 1.0, IMAGE_FORMAT_RGBA8888, 1, IMAGE_FORMAT_UNKNOWN, 0, 0, 1)
    return header.ljust(80, b'\0') + image.tobytes()


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    gmod = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).resolve().parents[2] / 'out' / 'portshaders'
    target = out / 'materials' / 'gmp'
    target.mkdir(parents=True, exist_ok=True)
    vpk = None
    for name, (source, size) in ASSETS.items():
        if source.startswith('vpk:'):
            vpk = vpk or Vpk(gmod / 'garrysmod' / 'garrysmod_dir.vpk')
            data = io.BytesIO(vpk.read(source[4:]))
        else:
            data = gmod / source
        image = Image.open(data).convert('RGBA').resize(size, Image.LANCZOS)
        (target / f'{name}.vtf').write_bytes(vtf(image))
        (target / f'{name}.vmt').write_text(VMT % name)
        print(f'{source} -> {target / name}.vtf {size[0]}x{size[1]}')


if __name__ == '__main__':
    main()
