"""Read-only helpers for TES5 record and placed-reference inspection."""
from pathlib import Path
import mmap, struct, zlib
from audit_landscape import subrecords

def records(path, types=None, ids=None, cell_ids=None):
    path = Path(path)
    masters = []
    def canon(fid):
        i = fid >> 24
        owner = masters[i] if i < len(masters) else path.name
        return f'{owner.lower()}:{fid & 0xffffff:06X}'
    with path.open('rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as b:
        def walk(p,end,cell=None,world=None):
            while p+24<=end:
                typ=b[p:p+4].decode('ascii'); n=struct.unpack_from('<I',b,p+4)[0]
                if typ=='GRUP':
                    gt=struct.unpack_from('<i',b,p+12)[0]
                    sc,sw=cell,world
                    if gt==1: sw=canon(struct.unpack_from('<I',b,p+8)[0])
                    if gt in (6,8,9,10): sc=canon(struct.unpack_from('<I',b,p+8)[0])
                    yield from walk(p+24,p+n,sc,sw)
                    p+=n; continue
                flags,fid=struct.unpack_from('<II',b,p+8)
                a,p=p+24,p+24+n
                if typ!='TES4' and (types is not None and typ not in types): continue
                if typ!='TES4' and cell_ids is not None and cell not in cell_ids: continue
                key=canon(fid) if typ!='TES4' else None
                if typ!='TES4' and ids is not None and key not in ids: continue
                data=b[a:p]
                if flags & 0x40000: data=zlib.decompress(data[4:])
                fs=list(subrecords(data))
                if typ=='TES4':
                    masters.extend(v.rstrip(b'\0').decode('utf-8',errors='replace') for t,v in fs if t=='MAST')
                    continue
                out={'id':key,'type':typ,'flags':flags,'cell':cell,'world':world,'fields':fs}
                for t,v in fs:
                    if t in ('EDID','MODL'):
                        out[t]=v.rstrip(b'\0').decode('utf-8',errors='replace')
                    elif t=='NAME' and len(v)==4:
                        out['base']=canon(struct.unpack_from('<I',v)[0])
                    elif t=='DATA' and typ in ('REFR','ACHR') and len(v)==24:
                        out['position_rotation']=list(struct.unpack('<6f',v))
                    elif t=='XSCL': out['scale']=struct.unpack('<f',v)[0]
                    elif t=='XESP': out['enable_parent']=[canon(struct.unpack_from('<I',v)[0]),v[4:].hex()]
                    elif t=='XTEL': out['teleport_door']=canon(struct.unpack_from('<I',v)[0])
                yield out
        yield from walk(0,len(b))

def compact(record):
    return {k:v for k,v in record.items() if k!='fields'}
