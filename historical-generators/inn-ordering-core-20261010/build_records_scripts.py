from pathlib import Path
import json,struct,subprocess,sys,shutil,re
sys.stdout.reconfigure(encoding='utf-8');sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent
STAGE=HERE/'staged/本地点餐 - 独立核心重构测试'
NAME='Local - Inn Ordering.esp'
sys.path.insert(0,str(ROOT/'lakeview-windowlight'))
from inspect_plugins import Plugin
p=Plugin(ROOT/'mods/本地点餐 - 独立酒馆点餐'/NAME)
def string(s):b=s.encode('utf-8');return struct.pack('<H',len(b))+b
def z(s):return s.encode('utf-8')+b'\0'
def record(tag,fields,fid,flags=0):
    raw=b''.join(struct.pack('<4sH',t,len(v))+v for t,v in fields)
    return struct.pack('<4sIIIIHH',tag,len(raw),flags,fid,0,44,0)+raw
def group(tag,data,kind=0):return struct.pack('<4sI4siIHH',b'GRUP',24+len(data),tag,kind,0,0,0)+data
def own(i):return 0x05000000|i
def old(i,replace=None):
    r=p.records[(p.name,i)];fields=[(t,(replace or {}).get(t,v)) for t,v in r['subs']]
    return record(r['sig'],fields,r['fid'],r['flags'])
specialty_specs=json.loads((HERE/'config/specialty-foods.json').read_text('utf-8'))
header=[(b'HEDR',struct.pack('<fII',1.7,15+len(specialty_specs),0x80F+len(specialty_specs))),(b'CNAM',z('Local Inn Ordering')),
        (b'SNAM',z('Independent inn ordering core 2.0.0-alpha.3. Independent house specialties and corrected barter.'))]
for m in p.masters:header.extend([(b'MAST',z(m)),(b'DATA',b'\0'*8)])
fragment='LocalInnCleanTopic'
vmad=struct.pack('<HHH',5,2,1)+string(fragment)+b'\0'+struct.pack('<H',0)+b'\x02\x02'+string(fragment)+b'\x01'+string(fragment)+string('Fragment_0')
info=p.records[(p.name,0x808)]
cleaninfo=record(b'INFO',[(t,vmad if t==b'VMAD' else v) for t,v in info['subs']],own(0x80B),info['flags'])
topic=p.records[(p.name,0x807)]
cleanfields=[]
for tag,value in topic['subs']:
    if tag==b'EDID':value=z('LocalInnOrderingCleanTopic')
    elif tag==b'FULL':value=z('Please clear my meal and tableware.')
    elif tag==b'BNAM':value=struct.pack('<I',own(0x809))
    cleanfields.append((tag,value))
cleantopic=record(b'DIAL',cleanfields,own(0x80A))
branch=p.records[(p.name,0x806)]
cleanbranch=record(b'DLBR',[(tag,z('LocalInnOrderingCleanBranch') if tag==b'EDID' else struct.pack('<I',own(0x80A)) if tag==b'SNAM' else value) for tag,value in branch['subs']],own(0x809))
restorefragment='LocalInnRestoreTopic'
restorevmad=struct.pack('<HHH',5,2,1)+string(restorefragment)+b'\0'+struct.pack('<H',0)+b'\x02\x02'+string(restorefragment)+b'\x01'+string(restorefragment)+string('Fragment_0')
restoreinfo=record(b'INFO',[(t,restorevmad if t==b'VMAD' else v) for t,v in info['subs']],own(0x80E),info['flags'])
restoretopic=record(b'DIAL',[(t,z('LocalInnRestoreTopic') if t==b'EDID' else z('Please put the cleared outside food and tableware back.') if t==b'FULL' else struct.pack('<I',own(0x80C)) if t==b'BNAM' else v) for t,v in topic['subs']],own(0x80D))
restorebranch=record(b'DLBR',[(t,z('LocalInnRestoreBranch') if t==b'EDID' else struct.pack('<I',own(0x80D)) if t==b'SNAM' else v) for t,v in branch['subs']],own(0x80C))
sys.path.insert(0,str(ROOT/'load-order-audit'))
from inspect_records import records as source_records
from inspect_plugins import subrecords
source_cache={}
def clone_food(spec):
    name=spec['source_plugin'].lower()
    if name not in source_cache:
        path=ROOT/'StockGame/Data'/spec['source_plugin']
        with path.open('rb') as stream:
            h=stream.read(24);header_data=stream.read(struct.unpack_from('<I',h,4)[0])
        masters=[v.rstrip(b'\0').decode().lower() for t,v in subrecords(header_data) if t==b'MAST']
        objects={int(r['id'].split(':')[1],16):dict(subs=[(t.encode(),v) for t,v in r['fields']],flags=r['flags']) for r in source_records(path,types={'ALCH'})}
        source_cache[name]=(masters+[name],objects)
    owners,objects=source_cache[name]
    item=objects[spec['source_local']]
    target_owners=p.masters+[p.name]
    def mapped(value):
        if not value:return 0
        owner=owners[value>>24];return (target_owners.index(owner)<<24)|(value&0xFFFFFF)
    fields=[]
    for tag,value in item['subs']:
        if tag==b'EDID':value=z('LocalInnSpecialty_'+spec['source_edid'])
        elif tag==b'FULL':value=z(spec['name_en'])
        elif tag in [b'YNAM',b'ZNAM',b'ETYP',b'EFID']:
            value=struct.pack('<I',mapped(struct.unpack('<I',value)[0]))
        elif tag==b'KWDA':value=b''.join(struct.pack('<I',mapped(x[0])) for x in struct.iter_unpack('<I',value))
        elif tag==b'ENIT':
            v=bytearray(value)
            for offset in [8,16]:struct.pack_into('<I',v,offset,mapped(struct.unpack_from('<I',v,offset)[0]))
            value=bytes(v)
        elif tag==b'MODS':
            v=bytearray(value);at=4
            for _ in range(struct.unpack_from('<I',v)[0]):
                size=struct.unpack_from('<I',v,at)[0];at+=4+size
                struct.pack_into('<I',v,at,mapped(struct.unpack_from('<I',v,at)[0]));at+=8
            assert at==len(v);value=bytes(v)
        fields.append((tag,value))
    return record(b'ALCH',fields,own(spec['local']),item['flags']&~0x40000)
raw=record(b'TES4',header,0,0x200)
raw+=group(b'GLOB',old(0x804))
raw+=group(b'QUST',old(0x805,{b'FULL':z('Inn Dining')}))
raw+=group(b'DIAL',old(0x807,{b'FULL':z('I would like to order a meal.')})+group(struct.pack('<I',own(0x807)),old(0x808),7)+cleantopic+group(struct.pack('<I',own(0x80A)),cleaninfo,7)+restoretopic+group(struct.pack('<I',own(0x80D)),restoreinfo,7))
raw+=group(b'FLST',b''.join(old(i) for i in range(0x800,0x804)))
raw+=group(b'DLBR',old(0x806)+cleanbranch+restorebranch)
raw+=group(b'ALCH',b''.join(clone_food(spec) for spec in specialty_specs))
(STAGE/NAME).write_bytes(raw)
new=Plugin(STAGE/NAME)
assert new.masters==p.masters and len(new.records)==16+len(specialty_specs)
for i in range(0x800,0x804):assert new.records[(new.name,i)]['subs']==p.records[(p.name,i)]['subs']

# Keep the private service package when rebuilding the stable plugin records.
subprocess.run([sys.executable,str(HERE/'build_service_package.py'),
                '--input',str(STAGE/NAME),'--output',str(STAGE/NAME)],check=True)

source=HERE/'source/LocalInnOrderingQuest.psc'
text=source.read_text('utf-8')
text=re.sub(r'return LocalFoodGallery.ProfileValue\(index, 0\)(?: as Int)*', 'return LocalFoodGallery.ProfileValue(index, 0) as Int',text)
text=text.replace('''if i == 3
                    kind = 1
                endif''','''if i == 3
                    kind = 1
                elseif i > 0
                    kind = 2
                endif''')
text=text.replace('''if i == 3
                        kind = 1
                    endif''','''if i == 3
                        kind = 1
                    elseif i > 0
                        kind = 2
                    endif''')
source.write_text(text,'utf-8')
compiler=ROOT/'download-compat-audit-20261004/gotobed-vokrii-patch-20261004/tools/Caprica.exe'
imports=ROOT/'order-menu-feasibility-20261009/imports'
sys.path.insert(0,str(compiler.parent.parent))
from patch_original_pex import Pex
for name in ['LocalFoodGallery','LocalInnOrderingQuest','LocalInnOrderingTopic','LocalInnCleanTopic','LocalInnRestoreTopic']:
    result=subprocess.run([str(compiler),name+'.psc','--game=skyrim','--import',str(HERE/'source')+';'+str(imports),
                           '--flags',str(imports/'TESV_Papyrus_Flags.flg'),'--output',str(HERE/'compiled')],cwd=HERE/'source',capture_output=True,text=True,encoding='utf-8',errors='replace')
    (HERE/'reports'/f'{name}-compile.log').write_text(result.stdout+result.stderr,'utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    data=(HERE/'compiled'/f'{name}.pex').read_bytes();pex=Pex.__new__(Pex)
    try:pex.__init__(data)
    except AssertionError:
        assert pex.at==len(data) and struct.unpack_from('<I',data,pex.size_at)[0]==len(data)-pex.size_at-4
        data=data[:pex.size_at]+struct.pack('>I',len(data)-pex.size_at)+data[pex.size_at+4:];Pex(data)
    (STAGE/'Scripts').mkdir(exist_ok=True)
    (STAGE/'Scripts'/f'{name}.pex').write_bytes(data)
    (STAGE/'Source/Scripts').mkdir(parents=True,exist_ok=True)
    shutil.copy2(HERE/'source'/f'{name}.psc',STAGE/'Source/Scripts'/f'{name}.psc')
print('ESP verified: stable form IDs, official masters, cleanup and restore records; five Papyrus scripts compiled.')
