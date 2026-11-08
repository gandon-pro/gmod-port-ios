"""Synthetic input tests for the PC asset collector; no original shader fixture."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import zipfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from collect_original_sky import collect, entities


def packed_vpk(path):
    entries = [('vcs', 'shaders/fxc', 'synthetic_sky_ps20b', b'packed synthetic bytecode'),
               ('vmt', 'materials/skybox', 'painted_test', b'g_sky { synthetic 1 }')]
    tree = b''
    for ext, folder, name, data in entries:
        tree += ext.encode()+b'\0'+folder.encode()+b'\0'+name.encode()+b'\0'
        tree += struct.pack('<IHHIIH', 0, len(data), 0x7fff, 0, 0, 0xffff)+data+b'\0\0'
    tree += b'\0'
    path.write_bytes(struct.pack('<III', 0x55aa1234, 1, len(tree))+tree)


with tempfile.TemporaryDirectory() as temp:
    temp=Path(temp)
    root=temp/'steamapps/common/GarrysMod';game=root/'garrysmod'
    game.mkdir(parents=True)
    packed_vpk(game/'garrysmod_dir.vpk')
    loose=game/'shaders/fxc/synthetic_sky_ps20b.vcs'
    loose.parent.mkdir(parents=True);loose.write_bytes(b'loose synthetic bytecode')
    binary=root/'bin/win64/synthetic_shader.dll';binary.parent.mkdir(parents=True)
    pe=bytearray(80);pe[:2]=b'MZ';struct.pack_into('<I',pe,60,64);pe[64:68]=b'PE\0\0';struct.pack_into('<H',pe,68,0x8664)
    binary.write_bytes(pe)
    entity_data=b'{\n"classname" "env_skypaint"\n"topcolor" "0.2 0.5 1.0"\n}\0'
    bsp=bytearray(1036);bsp[:4]=b'VBSP';struct.pack_into('<i',bsp,4,20);struct.pack_into('<ii',bsp,8,len(bsp),len(entity_data));bsp+=entity_data
    maps=game/'maps';maps.mkdir();(maps/'gm_construct.bsp').write_bytes(bsp)
    (temp/'steamapps/appmanifest_4000.acf').write_text('"buildid" "123456"\n"LastOwner" "not_for_export"')
    config=game/'cfg/config.cfg';config.parent.mkdir();config.write_text('private config excluded')
    output=temp/'sky.zip';manifest=collect(root,output)
    assert manifest['steam_build_id']=='123456'
    assert manifest['missing_inputs']==[]
    assert manifest['runtime_compatibility_verified'] is False
    with zipfile.ZipFile(output) as z:
        assert z.read('garrysmod/shaders/fxc/synthetic_sky_ps20b.vcs')==loose.read_bytes()
        assert z.read('garrysmod/materials/skybox/painted_test.vmt').startswith(b'g_sky')
        assert z.read('map_entities/gm_construct.ent')==entity_data
        assert not any('config.cfg' in n for n in z.namelist())
        text=z.read('manifest.json').decode();assert 'not_for_export' not in text and str(temp) not in text
        for entry in json.loads(text)['files']:
            assert hashlib.sha256(z.read(entry['path'])).hexdigest()==entry['sha256']
        entry=next(x for x in manifest['files'] if x['path'].endswith('.dll'));assert entry['pe']['architecture']=='x64'
    old=output.read_bytes()
    try:collect(root,output)
    except ValueError:pass
    else:raise AssertionError('existing output overwritten')
    assert output.read_bytes()==old
    empty=temp/'empty';(empty/'garrysmod').mkdir(parents=True)
    partial=collect(empty,temp/'partial.zip');assert len(partial['missing_inputs'])==2
    corrupt=bytearray(bsp);struct.pack_into('<i',corrupt,8,len(bsp)+1)
    try:entities(corrupt)
    except ValueError:pass
    else:raise AssertionError('invalid entity bounds accepted')
print('Sky collector passed: loose/VPK priority, exact bytes/hashes, PE machine, BSP entities, build ID, missing inputs and overwrite protection')
