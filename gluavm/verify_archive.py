"""Verify every Mach-O object is arm64/iOS, not merely arm64/macOS."""
import json
from pathlib import Path
import struct
import sys

path=Path(sys.argv[1])
data=path.read_bytes()
assert data.startswith(b'!<arch>\n')
position=8
objects=0
platforms=set()
while position<len(data):
    header=data[position:position+60]
    assert len(header)==60 and header[58:60]==b'`\n'
    size=int(header[48:58].strip())
    name=header[:16].decode('ascii').strip()
    payload=data[position+60:position+60+size]
    assert len(payload)==size
    if name.startswith('#1/'):
        length=int(name[3:])
        name=payload[:length].rstrip(b'\0').decode('utf-8')
        payload=payload[length:]
    if payload[:4]==b'\xcf\xfa\xed\xfe':
        magic,cpu,subtype,filetype,ncmds,sizeofcmds,flags,reserved=struct.unpack_from('<8I',payload)
        assert cpu==0x0100000c,(name,hex(cpu))
        assert filetype==1,(name,filetype)
        command=32
        platform=None
        for _ in range(ncmds):
            cmd,cmdsize=struct.unpack_from('<2I',payload,command)
            assert cmdsize>=8 and command+cmdsize<=len(payload)
            if cmd==0x32:platform=struct.unpack_from('<I',payload,command+8)[0]
            elif cmd==0x25:platform=2  # LC_VERSION_MIN_IPHONEOS
            command+=cmdsize
        assert platform==2,(name,platform)
        platforms.add(platform)
        objects+=1
    elif not name.startswith(('__.SYMDEF','/')):
        raise AssertionError('Unexpected non-Mach-O archive member: '+name)
    position+=60+size+(size%2)
assert objects>0
print(json.dumps({'archive':str(path),'objects':objects,'architecture':'arm64','platform':'iOS','passed':True}))
