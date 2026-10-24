"""List a Mach-O arm64 binary's static initializers in run order with their symbols.

usage: python tools/initorder.py <binary> [substring ...]
"""
import bisect
import struct
import sys


def main():
    data = open(sys.argv[1], 'rb').read()
    magic, _, _, _, ncmds, _, _, _ = struct.unpack_from('<8I', data, 0)
    assert magic == 0xFEEDFACF, 'not a thin 64-bit Mach-O'
    off = 32
    sections = []
    text_vm = None
    symtab = None
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<2I', data, off)
        if cmd == 0x19:  # LC_SEGMENT_64
            segname = data[off + 8:off + 24].rstrip(b'\0').decode()
            vmaddr, = struct.unpack_from('<Q', data, off + 24)
            if segname == '__TEXT':
                text_vm = vmaddr
            nsects, = struct.unpack_from('<I', data, off + 64)
            for i in range(nsects):
                s = off + 72 + i * 80
                sect = data[s:s + 16].rstrip(b'\0').decode()
                addr, sz, foff = struct.unpack_from('<QQI', data, s + 32)
                sections.append((segname, sect, addr, sz, foff))
        elif cmd == 0x2:  # LC_SYMTAB
            symtab = struct.unpack_from('<4I', data, off + 8)
        off += size
    syms = []
    symoff, nsyms, stroff, _ = symtab
    for i in range(nsyms):
        strx, ntype, sect, _, value = struct.unpack_from('<IBBhQ', data, symoff + i * 16)
        if sect and not ntype & 0xE0:
            end = data.index(b'\0', stroff + strx)
            syms.append((value, data[stroff + strx:end].decode(errors='replace')))
    syms.sort()
    addrs = [a for a, _ in syms]
    inits = []
    for seg, sect, addr, sz, foff in sections:
        if sect == '__init_offsets':
            inits += [text_vm + struct.unpack_from('<I', data, foff + k)[0] for k in range(0, sz, 4)]
        elif sect == '__mod_init_func':
            # chained-fixup rebase: low 36 bits are the target offset from the image base
            inits += [text_vm + (struct.unpack_from('<Q', data, foff + k)[0] & ((1 << 36) - 1)) for k in range(0, sz, 8)]
    keys = sys.argv[2:]
    for n, a in enumerate(inits):
        i = bisect.bisect_right(addrs, a) - 1
        name = syms[i][1] if i >= 0 else '?'
        if not keys or any(k in name for k in keys):
            print(n, hex(a), name)
    print('total', len(inits))


if __name__ == '__main__':
    main()
