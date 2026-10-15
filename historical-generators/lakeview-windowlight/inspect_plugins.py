"""Read-only Skyrim plugin inspection for the local Lakeview lighting patch."""
from pathlib import Path
import struct, zlib, json
ROOT = Path(__file__).resolve().parents[1]
CELL = ('hearthfires.esm', 0x2FEC)

def subrecords(data):
    pos = 0
    extended = None
    while pos < len(data):
        assert pos + 6 <= len(data)
        tag, size = struct.unpack_from('<4sH', data, pos)
        pos += 6
        if tag == b'XXXX':
            assert size == 4
            extended = struct.unpack_from('<I', data, pos)[0]
            pos += 4
            continue
        if extended is not None:
            size, extended = extended, None
        assert pos + size <= len(data)
        yield tag, data[pos:pos+size]
        pos += size

class Plugin:
    def __init__(self, path):
        self.path = path
        self.name = path.name.lower()
        self.masters = []
        self.records = {}
        data = path.read_bytes()
        def walk(start, end, context=()):
            pos = start
            while pos < end:
                assert pos + 24 <= end
                sig, size = struct.unpack_from('<4sI', data, pos)
                if sig == b'GRUP':
                    typ = struct.unpack_from('<i', data, pos+12)[0]
                    label = data[pos+8:pos+12]
                    walk(pos+24, pos+size, context+((typ, label),))
                    pos += size
                    continue
                flags, fid = struct.unpack_from('<II', data, pos+8)
                raw = data[pos+24:pos+24+size]
                if flags & 0x40000:
                    expected = struct.unpack_from('<I', raw)[0]
                    raw = zlib.decompress(raw[4:])
                    assert len(raw) == expected
                subs = list(subrecords(raw))
                if sig == b'TES4':
                    self.masters = [v.rstrip(b'\0').decode().lower() for k,v in subs if k == b'MAST']
                self.records[self.key(fid)] = dict(sig=sig, flags=flags, fid=fid, subs=subs, context=context, header=data[pos:pos+24])
                pos += 24+size
            assert pos == end
        walk(0,len(data))

    def key(self, fid):
        if not fid: return ('',0)
        names=self.masters+[self.name]
        return (names[fid>>24] if fid>>24<len(names) else f'UNRESOLVED_{fid>>24:02X}', fid&0xFFFFFF)

    def lake(self):
        return {k:r for k,r in self.records.items() if (r['sig']==b'CELL' and k==CELL) or any(t in (6,8,9,10) and self.key(struct.unpack('<I',v)[0])==CELL for t,v in r['context'])}

def describe(plugin, record, lookup):
    s=dict(record['subs'])
    out={'type':record['sig'].decode(), 'flags':hex(record['flags']), 'edid':s.get(b'EDID',b'').rstrip(b'\0').decode(errors='replace')}
    if b'NAME' in s:
        key=plugin.key(struct.unpack('<I',s[b'NAME'])[0]); out['base']=key
        if key in lookup:
            bp,br=lookup[key]; bs=dict(br['subs'])
            out['base_type']=br['sig'].decode();out['base_edid']=bs.get(b'EDID',b'').rstrip(b'\0').decode(errors='replace');out['model']=bs.get(b'MODL',b'').rstrip(b'\0').decode(errors='replace')
    if record['sig']==b'REFR' and b'DATA'in s:out['position_rotation']=struct.unpack('<6f',s[b'DATA'])
    if b'XSCL'in s:out['scale']=struct.unpack('<f',s[b'XSCL'])[0]
    if b'XESP'in s:out['enable_parent']=(plugin.key(struct.unpack_from('<I',s[b'XESP'])[0]),s[b'XESP'][4:].hex())
    if b'XEMI'in s:out['emittance']=plugin.key(struct.unpack('<I',s[b'XEMI'])[0])
    return out

if __name__=='__main__':
    paths=[ROOT/'StockGame/Data'/n for n in ('Skyrim.esm','Update.esm','HearthFires.esm')]
    for folder in ('终极窗影（1.3版本）Window Shadows Ultimate','照明大修-Lux','【炉火】炉火大修 Hearthfire Improvements - FOMOD'):
        paths+=list((ROOT/'mods'/folder).glob('*.esp'))
    plugins=[Plugin(p) for p in paths]
    lookup={k:(p,r) for p in plugins for k,r in p.records.items()}
    report={}
    for p in plugins:
        entries={f'{k[0]}:{k[1]:06X}':describe(p,r,lookup) for k,r in p.lake().items()}
        report[p.name]=entries
        selected=[(k,v) for k,v in entries.items() if v.get('base_type')=='LIGH' or any(w in (v.get('model','')+' '+v.get('base_edid','')).lower() for w in ('window','fxamb','byohhouseint'))]
        print(p.name, 'lake records',len(entries))
        if p.name=='lux.esp':
            for k,v in selected:print(k,json.dumps(v))
    (ROOT/'lakeview-windowlight/inspection.json').write_text(json.dumps(report,indent=2),encoding='utf8')
