"""Read-only MO2 plugin audit; output stays in this report directory."""
from pathlib import Path
import collections, hashlib, json, mmap, struct, sys, zlib

ROOT = Path(r'C:\iniRePather-Skyrim')
OUT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).parent
PROFILE = ROOT/'profiles'/'整合新版本测试'

def digest(b):
    return hashlib.sha256(b).hexdigest()

def subrecords(data):
    p, extended = 0, None
    while p+6 <= len(data):
        tag = data[p:p+4].decode('ascii')
        n = struct.unpack_from('<H', data, p+4)[0]
        p += 6
        if tag == 'XXXX':
            extended = struct.unpack_from('<I', data, p)[0]
            p += n
            continue
        if extended is not None:
            n, extended = extended, None
        if p+n > len(data):
            raise ValueError('Truncated subrecord')
        yield tag, data[p:p+n]
        p += n

def parse(path):
    masters, lands, cells, ltex = [], {}, {}, {}
    counts = collections.Counter()
    def canon(fid):
        idx = fid >> 24
        if idx > len(masters):
            raise ValueError(f'Invalid master index {idx} in {path.name}')
        owner = masters[idx] if idx < len(masters) else path.name
        return f'{owner.lower()}:{fid & 0xffffff:06X}'
    with path.open('rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as b:
        def walk(p, end, cell=None, world=None):
            while p+24 <= end:
                kind = b[p:p+4]
                n = struct.unpack_from('<I', b, p+4)[0]
                if kind == b'GRUP':
                    if n < 24 or p+n > end:
                        raise ValueError('Invalid group boundary')
                    gt = struct.unpack_from('<i', b, p+12)[0]
                    subcell, subworld = cell, world
                    if gt == 1:
                        subworld = canon(struct.unpack_from('<I', b, p+8)[0])
                    if gt in (6,8,9,10):
                        subcell = canon(struct.unpack_from('<I', b, p+8)[0])
                    walk(p+24, p+n, subcell, subworld)
                    p += n
                    continue
                flags, fid = struct.unpack_from('<II', b, p+8)
                a, p = p+24, p+24+n
                if p > end:
                    raise ValueError('Invalid record boundary')
                counts[kind.decode('ascii')] += 1
                if kind not in (b'TES4',b'LAND',b'CELL',b'LTEX'):
                    continue
                data = b[a:p]
                if flags & 0x40000:
                    expected = struct.unpack_from('<I', data)[0]
                    data = zlib.decompress(data[4:])
                    if len(data) != expected:
                        raise ValueError('Decompression length mismatch')
                fs = list(subrecords(data))
                if kind == b'TES4':
                    masters.extend(v.rstrip(b'\0').decode('utf-8',errors='replace') for t,v in fs if t=='MAST')
                    continue
                key = canon(fid)
                if kind == b'CELL':
                    xy = next((list(struct.unpack_from('<ii',v)) for t,v in fs if t=='XCLC'),None)
                    edid = next((v.rstrip(b'\0').decode('utf-8',errors='replace') for t,v in fs if t=='EDID'),'')
                    if xy is not None:
                        cells[key] = {'xy':xy,'edid':edid,'world':world}
                else:
                    normalized = collections.defaultdict(list)
                    for t,v in fs:
                        if kind==b'LAND' and t in ('BTXT','ATXT'):
                            value = [canon(struct.unpack_from('<I',v)[0]),v[4:].hex()]
                        elif kind==b'LAND' and t=='VTEX':
                            value = [canon(x[0]) for x in struct.iter_unpack('<I',v)]
                        elif kind==b'LTEX' and t in ('TNAM','GNAM','MNAM') and len(v)==4:
                            value = canon(struct.unpack_from('<I',v)[0])
                        else:
                            value = digest(v)
                        normalized[t].append(value)
                    record = {'cell':cell,'world':world,'flags':flags & 0x20,'fields':dict(normalized)}
                    (lands if kind==b'LAND' else ltex)[key] = record
        walk(0,len(b))
    return masters,dict(counts),lands,cells,ltex

def changed(a,b):
    return sorted(t for t in set(a)|set(b) if a.get(t)!=b.get(t))

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    snapshots = {n:(PROFILE/n).read_bytes() for n in ('modlist.txt','plugins.txt','loadorder.txt')}
    for name,data in snapshots.items():
        (OUT/(name+'.snapshot')).write_bytes(data)
    texts = {n:v.decode('utf-8-sig').splitlines() for n,v in snapshots.items()}
    mods = [s[1:] for s in texts['modlist.txt'] if s.startswith('+')]
    providers = {}
    for folder in [ROOT/'overwrite']+[ROOT/'mods'/m for m in mods]+[ROOT/'StockGame'/'Data']:
        if folder.exists():
            for p in folder.iterdir():
                if p.is_file() and p.suffix.lower() in ('.esp','.esm','.esl'):
                    providers.setdefault(p.name.lower(),p)
    official = ['Skyrim.esm','Update.esm','Dawnguard.esm','HearthFires.esm','Dragonborn.esm']
    ccc = ROOT/'StockGame'/'Skyrim.ccc'
    if ccc.exists():
        official += [s for s in ccc.read_text(encoding='utf-8-sig').splitlines() if s and not s.startswith('#')]
    lines = {s[1:].lower():i for i,s in enumerate(texts['plugins.txt'],1) if s.startswith('*')}
    active = set(lines)|{s.lower() for s in official}
    order = [s for s in texts['loadorder.txt'] if s.lower() in active]
    if active-{s.lower() for s in order}:
        raise ValueError('Active plugins absent from loadorder')
    info,chains,cells,ltchains = {},collections.defaultdict(list),{},collections.defaultdict(list)
    missing = []
    for i,name in enumerate(order):
        path = providers.get(name.lower())
        if path is None:
            missing.append(name)
            continue
        masters,counts,lands,c,lt = parse(path)
        info[name] = {'index':i,'line':lines.get(name.lower()),'path':str(path),'masters':masters,'counts':counts}
        for key,v in lands.items():
            chains[key].append({'plugin':name,**v})
        for key,v in lt.items():
            ltchains[key].append({'plugin':name,**v})
        cells.update(c)
        if i%300==0:
            print(f'Parsed {i}/{len(order)}',flush=True)
    index = {n.lower():i for i,n in enumerate(order)}
    deps = []
    for name,meta in info.items():
        for master in meta['masters']:
            if master.lower() not in index:
                deps.append({'plugin':name,'master':master,'issue':'inactive_or_missing_master'})
            elif index[master.lower()]>index[name.lower()]:
                deps.append({'plugin':name,'master':master,'issue':'master_loads_later'})
    def nr(name):
        return name.lower()=='northern roads.esp' or any(m.lower()=='northern roads.esp' for m in info[name]['masters'])
    pairs = collections.defaultdict(list)
    for key,chain in chains.items():
        final = chain[-1]
        for rec in chain[:-1]:
            if not nr(rec['plugin']):
                continue
            diff = changed(rec['fields'],final['fields'])
            if diff:
                pairs[(rec['plugin'],final['plugin'])].append({'land':key,'cell':final['cell'],
                    'xy':cells.get(final['cell'],{}).get('xy'),'world':final['world'],'changed':diff})
    pairs = [{'earlier':a,'winner':b,'winner_knows_nr':nr(b),'count':len(rs),
              'height_changes':sum('VHGT' in r['changed'] for r in rs),'records':rs}
              for (a,b),rs in pairs.items()]
    pairs.sort(key=lambda p:(-p['count'],p['earlier'],p['winner']))
    late = [{'plugin':n,'line':m['line'],'land_count':m['counts']['LAND'],'nr_aware':nr(n)}
            for n,m in info.items() if m['index']>index.get('northern roads.esp',10**9) and m['counts'].get('LAND')]
    output = {'profile':str(PROFILE),'input_sha256':{n:digest(v) for n,v in snapshots.items()},
        'plugin_count':len(order),'missing_plugins':missing,'dependency_issues':deps,'plugins':info,
        'cells':cells,'land_chains':dict(chains),'ltex_chains':dict(ltchains),'nr_override_pairs':pairs,
        'land_plugins_after_nr':late,'inputs_unchanged':all((PROFILE/n).read_bytes()==v for n,v in snapshots.items())}
    (OUT/'audit.json').write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf-8')
    summary = {k:output[k] for k in ('plugin_count','missing_plugins','dependency_issues','inputs_unchanged')}
    summary['land_records'] = len(chains)
    summary['nr_overridden_by_unrelated'] = [{k:v for k,v in p.items() if k!='records'} for p in pairs if not p['winner_knows_nr']]
    summary['land_plugins_after_nr'] = late
    (OUT/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(summary,ensure_ascii=False,indent=2))

if __name__=='__main__':
    main()
