# Extract RT_BITMAP / RT_RCDATA / WAVE resources from a PE32 into .bmp/.bin files keyed by resource name.
import struct, sys, os
data = open(sys.argv[1], 'rb').read()
out = sys.argv[2]; os.makedirs(out, exist_ok=True)
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
optsz = struct.unpack_from('<H', data, pe + 20)[0]
opt = pe + 24
rsrc_rva = struct.unpack_from('<I', data, opt + 96 + 2 * 8)[0]
secs = []
for i in range(nsec):
    s = opt + optsz + i * 40
    va, vs, raw = struct.unpack_from('<I', data, s + 12)[0], struct.unpack_from('<I', data, s + 8)[0], struct.unpack_from('<I', data, s + 20)[0]
    secs.append((va, max(vs, struct.unpack_from('<I', data, s + 16)[0]), raw))
def off(rva):
    for va, sz, raw in secs:
        if va <= rva < va + sz: return rva - va + raw
    raise ValueError(hex(rva))
base = off(rsrc_rva)
def name(e):
    if e & 0x80000000:
        p = base + (e & 0x7fffffff); n = struct.unpack_from('<H', data, p)[0]
        return data[p + 2:p + 2 + n * 2].decode('utf-16le')
    return str(e)
def entries(p):
    nn, ni = struct.unpack_from('<HH', data, p + 12)
    for i in range(nn + ni):
        yield struct.unpack_from('<II', data, p + 16 + i * 8)
TYPES = {'2': 'bmp', '10': 'rcdata'}
for tn, tp in entries(base):
    t = name(tn)
    for rn, rp in entries(base + (tp & 0x7fffffff)):
        r = name(rn)
        for ln, lp in entries(base + (rp & 0x7fffffff)):
            rva, size = struct.unpack_from('<II', data, base + lp)
            blob = data[off(rva):off(rva) + size]
            if t == '2':
                hdr = struct.unpack_from('<I', blob, 0)[0]
                bpp = struct.unpack_from('<H', blob, 14)[0]
                clr = struct.unpack_from('<I', blob, 32)[0] or (1 << bpp if bpp <= 8 else 0)
                bits = 14 + hdr + clr * 4
                blob = b'BM' + struct.pack('<IHHI', 14 + len(blob), 0, 0, bits) + blob
                fn = f'{r}.bmp'
            else:
                fn = f'{t}_{r}.bin'
            open(os.path.join(out, fn), 'wb').write(blob)
            print(t, r, size)
