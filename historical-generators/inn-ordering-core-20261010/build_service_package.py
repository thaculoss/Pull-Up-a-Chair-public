"""Build private approach/hold packages while preserving existing plugin records."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
sys.stdout.reconfigure(encoding='utf-8');sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent
sys.path.insert(0,str(ROOT/'load-order-audit'));from inspect_records import records
sys.path.insert(0,str(ROOT/'lakeview-windowlight'));from inspect_plugins import Plugin
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--input',type=Path);parser.add_argument('--output',type=Path);args=parser.parse_args()
original=args.input or ROOT/'mods/本地点餐 - 独立核心重构测试/Local - Inn Ordering.esp'
old=Plugin(original);assert (old.name,0x830) not in old.records
vanilla={int(r['id'].split(':')[1],16):r for r in records(ROOT/'StockGame/Data/Skyrim.esm',types={'PACK'},ids={'skyrim.esm:106C9F','skyrim.esm:106ACF'})}
def fields(source,edid,travel):
    result=[]
    for tag,value in source['fields']:
        if tag in ['QNAM','CTDA','CIS1','CIS2','VMAD']:continue
        if tag=='EDID':value=edid.encode()+b'\0'
        elif tag=='PKDT':
            value=bytearray(value);struct.pack_into('<I',value,0,0x2005);value[6]=0;struct.pack_into('<H',value,8,0);value=bytes(value)
        elif travel and tag=='PLDT':value=struct.pack('<III',0,0x14,180)
        if tag in ['INAM','PDTO']:assert not int.from_bytes(value,'little'),('Unexpected source script/topic',tag)
        result.append((tag.encode(),value))
    return result
def record(local,items):
    body=b''.join(struct.pack('<4sH',tag,len(value))+value for tag,value in items)
    return struct.pack('<4sIIIIHH',b'PACK',len(body),0,(len(old.masters)<<24)|local,0,44,0)+body
approach=fields(vanilla[0x106C9F],'LocalInnServicePriorityTravel',True)
hold=fields(vanilla[0x106ACF],'LocalInnServiceWaitForOrder',False)
raw=bytearray(original.read_bytes());has_travel=(old.name,0x82F) in old.records
if has_travel:
    changed=False
    def walk(at,end):
        global changed
        while at+24<=end:
            tag=raw[at:at+4];size=struct.unpack_from('<I',raw,at+4)[0]
            if tag==b'GRUP':walk(at+24,at+size);at+=size;continue
            fid=struct.unpack_from('<I',raw,at+12)[0]
            if tag==b'PACK' and fid==((len(old.masters)<<24)|0x82F):
                p=at+24
                while p<at+24+size:
                    sub,n=struct.unpack_from('<4sH',raw,p);p+=6
                    if sub==b'PLDT':
                        assert n==12 and raw[p:p+8]==struct.pack('<II',0,0x14)
                        struct.pack_into('<I',raw,p+8,180);changed=True
                    p+=n
            at+=24+size
    walk(0,len(raw));assert changed
new_records=(b'' if has_travel else record(0x82F,approach))+record(0x830,hold)
raw+=struct.pack('<4sI4siIHH',b'GRUP',24+len(new_records),b'PACK',0,0,0,0)+new_records
end=24+struct.unpack_from('<I',raw,4)[0];at=24
while at<end:
    tag,size=struct.unpack_from('<4sH',raw,at);at+=6
    if tag==b'HEDR':
        count,nextid=struct.unpack_from('<II',raw,at+4);struct.pack_into('<II',raw,at+4,count+(1 if has_travel else 2),max(nextid,0x831))
    at+=size
target=args.output or HERE/'compiled/Local - Inn Ordering.esp';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
new=Plugin(target);assert new.masters==old.masters and len(new.records)==len(old.records)+(1 if has_travel else 2)
preserved=0
for key,r in old.records.items():
    if r['sig']==b'TES4':continue
    if key==(old.name,0x82F):
        expected=[(t,struct.pack('<III',0,0x14,180) if t==b'PLDT' else v) for t,v in r['subs']]
        assert new.records[key]['subs']==expected and new.records[key]['flags']==r['flags']
    else:assert new.records[key]['subs']==r['subs'] and new.records[key]['flags']==r['flags'];preserved+=1
for local,template in [(0x82F,0x16FAA),(0x830,0x654E2)]:
    pack=new.records[(new.name,local)];data=dict(pack['subs']);assert pack['sig']==b'PACK'
    assert int.from_bytes(data[b'PKDT'][:4],'little')==0x2005 and int.from_bytes(data[b'PKDT'][8:10],'little')==0
    assert int.from_bytes(data[b'PKCU'][4:8],'little')==template and b'QNAM' not in data and b'CTDA' not in data
report={'approach':'00082F','arrival_radius':180,'hold':'000830','hold_template':'Skyrim.esm:0654E2 DoNothing',
        'hold_source':'Skyrim.esm:106ACF MQ303OdahviingDoNothing','old_records_preserved_except_travel_radius':preserved,
        'original_food_dialogue_quest_formlist_records_preserved':True,'masters_unchanged':True,'records':len(new.records),
        'no_random_conversation_idle_hello_or_world_interaction':True,'sha256':hashlib.sha256(raw).hexdigest(),'gameplay_tested':False}
(HERE/'reports/service-package.json').write_text(json.dumps(report,indent=2),'utf-8');print(json.dumps(report,indent=2))
