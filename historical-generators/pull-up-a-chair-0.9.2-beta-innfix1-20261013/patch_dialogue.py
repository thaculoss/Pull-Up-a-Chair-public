from pathlib import Path
import hashlib,json,struct,sys,zlib
sys.dont_write_bytecode=True
BASE=Path(__file__).resolve().parent;ROOT=BASE.parent
sys.path.insert(0,str(ROOT/'load-order-audit'))
from inspect_records import records
from audit_landscape import subrecords
SOURCE=ROOT/'pull-up-a-chair-0.9.1-beta-ai1-20261013/stage/00_Core/Pull up a chair Ordering.esp'
TARGET=BASE/'stage/00_Core/Pull up a chair Ordering.esp'
raw=SOURCE.read_bytes();changed=[]
def encode(fs):return b''.join(struct.pack('<4sH',t.encode(),len(v))+v for t,v in fs)
def walk(start,end):
 out=bytearray();pos=start
 while pos<end:
  tag,size=struct.unpack_from('<4sI',raw,pos)
  h=bytearray(raw[pos:pos+24])
  if tag==b'GRUP':
   body=walk(pos+24,pos+size);struct.pack_into('<I',h,4,len(body)+24);out+=h+body;pos+=size;continue
  flags,fid=struct.unpack_from('<II',h,8);body=raw[pos+24:pos+24+size]
  if tag==b'INFO' and fid&0xFFFFFF in [0x808,0x80B,0x80E]:
   data=zlib.decompress(body[4:]) if flags&0x40000 else body
   fs=list(subrecords(data));old=[v for t,v in fs if t=='CTDA'];assert len(old)==4
   assert [struct.unpack_from('<H',v,8)[0] for v in old]==[71,71,255,159]
   converted=[];count=0;last=max(i for i,(t,v) in enumerate(fs) if t=='CTDA')
   for i,(t,v) in enumerate(fs):
    if t=='CTDA' and struct.unpack_from('<H',v,8)[0]==255:
     v=bytearray(v);struct.pack_into('<H',v,8,562);struct.pack_into('<I',v,12,0x1CB87);v=bytes(v);count+=1
    converted.append((t,v))
    if i==last:converted.append(('CTDA',struct.pack('<B3sfH2sIIIII',0,b'\0'*3,1.0,32,b'\0'*2,0x14,0,0,0,0xFFFFFFFF)))
   assert count==1;body=encode(converted)
   if flags&0x40000:body=struct.pack('<I',len(body))+zlib.compress(body)
   struct.pack_into('<I',h,4,len(body));changed.append(f'{fid&0xFFFFFF:06X}')
  out+=h+body;pos+=24+size
 assert pos==end
 return bytes(out)
TARGET.write_bytes(walk(0,len(raw)))
a=list(records(SOURCE));b=list(records(TARGET));assert len(a)==len(b)==52
for old,new in zip(a,b):
 assert old['id']==new['id'] and old['type']==new['type'] and old['flags']==new['flags']
 if old['id'].split(':')[-1] not in changed:assert old['fields']==new['fields']
 else:
  assert [(t,v) for t,v in old['fields'] if t!='CTDA']==[(t,v) for t,v in new['fields'] if t!='CTDA']
  cs=[v for t,v in new['fields'] if t=='CTDA'];assert [struct.unpack_from('<H',v,8)[0] for v in cs]==[71,71,562,159,32]
assert set(changed)=={'000808','00080B','00080E'}
report={'changed_info_ids':changed,'records':len(b),'original_non_condition_fields_retained':True,'old_conditions':['owner OR server','offers services now','player seated'],'new_conditions':['owner OR server','location keyword LocTypeInn','player seated','same cell as player'],'new_masters':[],'all_ai_packages_unchanged':True,'sha256':hashlib.sha256(TARGET.read_bytes()).hexdigest()}
(BASE/'dialogue-patch-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps(report,ensure_ascii=False))
