"""Collect original installed GMod sky inputs for a measured renderer port.

This exports assets/binaries for analysis; it does not install a replacement sky.
Uses the project's existing VPK reader. Python 3.10+, standard library only.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import zipfile

from extract_npc_content import Vpk


def sky_input(name):
    name = name.lower()
    path = PurePosixPath(name)
    return ((name.startswith('shaders/fxc/') and 'sky' in path.name and path.suffix == '.vcs')
            or (name.startswith('materials/skybox/') and
                (path.name.startswith('painted') or path.stem == 'starfield') and path.suffix in ('.vmt', '.vtf'))
            or name in ('gamemodes/base/entities/entities/env_skypaint.lua', 'lua/matproxy/sky_paint.lua'))


def pe_machine(data):
    if len(data) < 64 or data[:2] != b'MZ':
        return None
    offset = struct.unpack_from('<I', data, 60)[0]
    if offset + 6 > len(data) or data[offset:offset+4] != b'PE\0\0':
        return None
    machine = struct.unpack_from('<H', data, offset+4)[0]
    return {'machine': hex(machine), 'architecture': {0x14c: 'x86', 0x8664: 'x64', 0xaa64: 'arm64'}.get(machine, 'unknown')}


def entities(data):
    if len(data) < 24 or data[:4] != b'VBSP':
        raise ValueError('gm_construct.bsp: invalid Source BSP header')
    offset, size = struct.unpack_from('<ii', data, 8)
    if offset < 0 or size < 0 or size > 8*1024*1024 or offset > len(data) or size > len(data)-offset:
        raise ValueError('gm_construct.bsp: invalid entity lump bounds')
    result = data[offset:offset+size]
    if result.startswith(b'LZMA'):
        raise ValueError('Compressed entity lump; preserve original BSP for separate decoding')
    return result


def collect(root, output):
    root, output = Path(root).resolve(), Path(output).resolve()
    game = root / 'garrysmod'
    if not game.is_dir():
        raise ValueError('Expected the GMod install/content root containing garrysmod/')
    if output.exists():
        raise ValueError('Output already exists; choose another filename')
    payload = {}
    origins = {}
    warnings = []

    def add(path, data, origin):
        # Store canonical relative paths, never absolute machine paths.
        parts = PurePosixPath(path).parts
        if not parts or any(p in ('..', '') for p in parts) or PurePosixPath(path).is_absolute() or '\\' in path:
            raise ValueError('Unsafe archive path')
        if path not in payload:
            payload[path], origins[path] = data, origin

    for folder in ('shaders/fxc', 'materials/skybox'):
        directory = game / folder
        if directory.is_dir():
            for file in sorted(directory.rglob('*')):
                if file.is_file() and not file.is_symlink():
                    name = file.relative_to(game).as_posix()
                    if sky_input(name):
                        add('garrysmod/'+name, file.read_bytes(), 'loose')
    for name in ('gamemodes/base/entities/entities/env_skypaint.lua', 'lua/matproxy/sky_paint.lua'):
        file = game / name
        if file.is_file() and not file.is_symlink():
            add('garrysmod/'+name, file.read_bytes(), 'loose')

    # Include shader libraries with their real filenames/architecture; do not guess
    # which library registers g_sky before inspecting the user's exact build.
    for base in (root/'bin', game/'bin'):
        if base.is_dir():
            for file in sorted(base.rglob('*')):
                if file.is_file() and not file.is_symlink() and 'shader' in file.name.lower() and file.suffix.lower() in ('.dll', '.so', '.dylib'):
                    add(file.relative_to(root).as_posix(), file.read_bytes(), 'loose')

    # GMod search order: loose assets override packed copies.
    for file in sorted(game.glob('*_dir.vpk')):
        with file.open('rb') as stream:
            header = stream.read(8)
        if len(header) != 8 or struct.unpack('<II', header)[0] != 0x55AA1234 or struct.unpack('<II', header)[1] not in (1, 2):
            raise ValueError('Unsupported VPK directory: '+file.name)
        archive = Vpk(file)
        for name in sorted(archive.entries):
            if sky_input(name) and 'garrysmod/'+name not in payload:
                data = archive.read(name)
                _, _, length, preload = archive.entries[name]
                if len(data) != length + len(preload):
                    raise ValueError('Truncated VPK asset: '+name)
                add('garrysmod/'+name, data, file.relative_to(root).as_posix())

    bsp = game/'maps/gm_construct.bsp'
    if bsp.is_file() and not bsp.is_symlink():
        data = bsp.read_bytes()
        try:
            add('map_entities/gm_construct.ent', entities(data), 'garrysmod/maps/gm_construct.bsp')
        except ValueError as error:
            warnings.append(str(error))
        map_hash = hashlib.sha256(data).hexdigest()
    else:
        map_hash = None
        warnings.append('No loose gm_construct.bsp; map-specific sky parameters were not collected')

    manifest = {'schema': 1, 'purpose': 'original-sky-analysis-inputs',
                'runtime_compatibility_verified': False, 'files': [],
                'gm_construct_sha256': map_hash, 'warnings': warnings}
    steam = root.parent.parent/'appmanifest_4000.acf'
    if steam.is_file():
        match = re.search(r'"buildid"\s+"(\d+)"', steam.read_text(errors='replace'), re.I)
        if match:
            manifest['steam_build_id'] = match.group(1)
    missing = []
    if not any(p.endswith('.vcs') for p in payload):
        missing.append('original sky .vcs bytecode')
    if not any('shader' in PurePosixPath(p).name.lower() and PurePosixPath(p).suffix.lower() in ('.dll', '.so', '.dylib') for p in payload):
        missing.append('matching shader registration library')
    manifest['missing_inputs'] = missing
    for path, data in sorted(payload.items()):
        entry = {'path': path, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'origin': origins[path]}
        machine = pe_machine(data)
        if machine:
            entry['pe'] = machine
        manifest['files'].append(entry)
    output.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        archive = zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED)
        created = True
        with archive:
            for path, data in sorted(payload.items()):
                archive.writestr(path, data)
            archive.writestr('manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
    except BaseException:
        # Only remove a partial file created by this invocation.
        if created and output.exists():
            output.unlink()
        raise
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gmod', type=Path, help='Installed GarrysMod/content root containing garrysmod/')
    parser.add_argument('output', type=Path, help='New output ZIP filename')
    args = parser.parse_args()
    try:
        manifest = collect(args.gmod, args.output)
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, str(error)+'\n')
    print(f"Collected {len(manifest['files'])} files into {args.output}")
    for item in manifest['missing_inputs']:
        print('Missing: '+item)
    for item in manifest['warnings']:
        print('Warning: '+item)
    print('Analysis bundle only; original sky rendering is not yet ported or verified.')


if __name__ == '__main__':
    main()
