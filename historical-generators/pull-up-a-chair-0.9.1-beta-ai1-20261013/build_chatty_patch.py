"""Build an actor-scoped reservation and a conditional Chatty NPCs alias patch."""
from pathlib import Path
import hashlib, json, struct, sys, zlib
sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
ROOT = BASE.parent
sys.path.insert(0, str(ROOT/'load-order-audit'))
from inspect_records import records
from audit_landscape import subrecords

CORE = BASE/'stage/00_Core/Pull up a chair Ordering.esp'
CORE_BASELINE = ROOT/'pull-up-a-chair-0.9-beta-20261013/stage/00_Core/Pull up a chair Ordering.esp'
SOURCE = ROOT/'mods/【NPC】健谈的 NPC 和追随者-Chatty NPCs/Chatty NPCs.esp'
PATCH = BASE/'stage/45_ChattyNPCs/Pull up a chair - Chatty NPCs Patch.esp'
PAIRS = {
    0x819:(0x802,'JB1ChatEngine','JB1ChattyNPCsEngine'),
    0x822:(0x821,'JB1ChatDo','JB1Chat'),
    0x903:(0x902,'JB1ChattyRudeDO','JB1ChattyRude'),
    0x954:(0x953,'JB1RaceReactionDO','JB1RaceReaction'),
    0x9F1:(0x9F0,'JB1HealthDO','JB1Health'),
    0xA61:(0xA60,'JB1BadHealthDO','JB1BadHealth'),
    0xAC0:(0xABF,'JB1GoodHealthDO','JB1GoodHealth'),
    0xAFC:(0xAFB,'JB1HealthChoiceDO','JB1HealthChoice'),
    0xB1B:(0xB1A,'JB1DiseaseDO','JB1Disease'),
    0xC2A:(0xC29,'JB1MehDoDO','JB1MehDo'),
}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def fields(data): return list(subrecords(data))
def encode(fs): return b''.join(struct.pack('<4sH',t.encode(),len(v))+v for t,v in fs)
def record(tag,flags,fid,fs):
    body=encode(fs)
    return struct.pack('<4sIIIIHH',tag.encode(),len(body),flags,fid,0,44,0)+body
def group(tag,items):
    body=b''.join(items)
    return struct.pack('<4sI4siIHH',b'GRUP',len(body)+24,tag.encode(),0,0,0,0)+body
def header(path):
    raw=path.read_bytes(); n=struct.unpack_from('<I',raw,4)[0]
    fs=fields(raw[24:24+n]); masters=[v.rstrip(b'\0').decode() for t,v in fs if t=='MAST']
    return raw,n,fs,masters

raw,n,fs,core_masters=header(CORE_BASELINE)
core_before=list(records(CORE_BASELINE))
assert not any(r['id'].endswith(':000831') for r in core_before), 'Reservation ID already occupied'
newfs=[]
for t,v in fs:
    if t=='HEDR':
        version,count,nextid=struct.unpack('<fII',v)
        v=struct.pack('<fII',version,count+1,max(nextid,0x832))
    newfs.append((t,v))
faction=[('EDID',b'PullUpAChairActiveServiceFaction\0'),('DATA',struct.pack('<I',1)),('CRVA',b'\0'*16)]
CORE.write_bytes(record('TES4',struct.unpack_from('<I',raw,8)[0],0,newfs)+raw[24+n:]+
                 group('FACT',[record('FACT',0,(len(core_masters)<<24)|0x831,faction)]))
core_after=list(records(CORE))
assert core_after[:-1]==core_before and core_after[-1]['type']=='FACT'

_,_,_,masters=header(SOURCE)
source_records={int(r['id'].split(':')[1],16):r for r in records(SOURCE)}
patch_masters=masters+[SOURCE.name,CORE.name]
faction_ref=((len(patch_masters)-1)<<24)|0x831
# GetInFaction ActiveService == 0, AND, run on alias candidate (Subject).
condition=struct.pack('<B3sfH2sIIIII',0,b'\0'*3,0.0,71,b'\0'*2,faction_ref,0,0,0,0xffffffff)
assert len(condition)==32
patched=[]; report=[]
for sid,(qid,scene_name,quest_name) in PAIRS.items():
    scene=source_records[sid];quest=source_records[qid]
    assert scene['type']=='SCEN' and scene['EDID']==scene_name
    assert quest['type']=='QUST' and quest['EDID']==quest_name
    assert not any(t=='VMAD' for t,v in scene['fields']), 'Unexpected scene script requires review'
    # TES5 xEdit SCEN FNAM bit 1 is Stop Quest on End: terminating this scene
    # also releases its quest aliases. Never issue a broad parent-quest Stop.
    assert next(int.from_bytes(v,'little') for t,v in scene['fields'] if t=='FNAM') & 2
    assert [int.from_bytes(v,'little')&0xffffff for t,v in scene['fields'] if t=='PNAM'][-1]==qid
    output=[];alias=None;added=False;aliases=[]
    for t,v in quest['fields']:
        if t=='ALST': alias=int.from_bytes(v,'little');added=False
        if t=='CTDA' and alias is not None and not added:
            output.append(('CTDA',condition));added=True;aliases.append(alias)
        output.append((t,v))
        if t=='ALED':
            assert added, f'Alias has no reviewed candidate conditions: {qid:x}:{alias}'
            alias=None
    assert aliases
    # Every original field is retained in order. Only candidate exclusions added.
    assert [(t,v) for t,v in output if not (t=='CTDA' and v==condition)]==quest['fields']
    patched.append(record('QUST',quest['flags']&~0x40000,(len(masters)<<24)|qid,output))
    report.append({'scene':f'{sid:06X}','scene_name':scene_name,'quest':f'{qid:06X}',
                   'quest_name':quest_name,'alias_ids':aliases})
tes4=[('HEDR',struct.pack('<fII',1.7,len(patched),0x800)),
      ('CNAM',b'Pull Up a Chair\0'),('SNAM',b'Service-only candidate exclusion for Chatty NPCs.\0')]
for master in patch_masters:tes4.extend([('MAST',master.encode()+b'\0'),('DATA',b'\0'*8)])
PATCH.parent.mkdir(parents=True,exist_ok=True)
PATCH.write_bytes(record('TES4',0x200,0,tes4)+group('QUST',patched))
check=list(records(PATCH));assert len(check)==len(PAIRS)
assert all(r['type']=='QUST' and r['id'].startswith('chatty npcs.esp:') for r in check)
(BASE/'chatty-patch-validation.json').write_text(json.dumps({
    'source':str(SOURCE.relative_to(ROOT)),'source_sha256':sha(SOURCE),
    'patch_sha256':sha(PATCH),'core_sha256':sha(CORE),'masters':patch_masters,
    'quest_overrides':len(check),'candidate_exclusions':sum(len(r['alias_ids']) for r in report),
    'original_quest_fields_preserved':True,'original_core_records_preserved':len(core_before),
    'no_vendor_scripts_or_audio_redistributed':True,'reviewed':report,
    'scene_stop_releases_parent_quest_by_existing_flag':True,
    'record_schema_reference':'https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsTES5.pas',
    'gameplay_tested':False},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'quest_overrides':len(check),'candidate_exclusions':sum(len(r['alias_ids']) for r in report),'patch_sha256':sha(PATCH)}))
