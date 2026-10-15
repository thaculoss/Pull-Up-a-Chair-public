from pathlib import Path
import json,struct,subprocess,sys,shutil
sys.stdout.reconfigure(encoding='utf8');sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent;NAME='Local - Inn Service.esp'
STAGE=HERE/'staged/本地点餐 - 酒馆与服务'
sys.path.insert(0,str(ROOT/'load-order-audit'))
from inspect_records import records
sys.path.insert(0,str(ROOT/'lakeview-windowlight'))
from inspect_plugins import Plugin
def record(tag,fields,fid,flags=0):
    raw=b''.join(struct.pack('<4sH',t,len(v))+v for t,v in fields)
    return struct.pack('<4sIIIIHH',tag,len(raw),flags,fid,0,44,0)+raw
def group(tag,body):return struct.pack('<4sI4siIHH',b'GRUP',24+len(body),tag,0,0,0,0)+body
def z(text):return text.encode('utf8')+b'\0'
def string(text):b=text.encode('utf8');return struct.pack('<H',len(b))+b
def own(i):return 0x01000000|i
base=next(records(ROOT/'StockGame/Data/Skyrim.esm',types={'PACK'},ids={'skyrim.esm:0F608F'}))
fields=[];kind='';floatIndex=0
for tag,value in base['fields']:
    if tag in ['CTDA','CIS1','CIS2','QNAM']:continue
    if tag=='EDID':value=z('LocalInnServiceApproachPlayer')
    elif tag=='PSDT':value=bytes.fromhex('ffff00ffff00000000000000')
    elif tag=='ANAM':kind=value.rstrip(b'\0').decode()
    elif tag=='CNAM' and kind=='Float':
        value=struct.pack('<f',[120,200][floatIndex]);floatIndex+=1
    fields.append((tag.encode(),value))
# Restrict the temporary package to a native-managed active call.
condition=struct.pack('<B3xfH2xIIIII',0,1.0,74,own(0x802),0,0,0,0xFFFFFFFF)
fields.insert(3,(b'CTDA',condition))
fields.insert(4,(b'QNAM',struct.pack('<I',own(0x800))))
assert floatIndex==2
package=record(b'PACK',fields,own(0x801))
vmad=struct.pack('<HHH',5,2,1)+string('LocalInnServiceControl')+b'\0'+struct.pack('<H',0)
# An optional, loaded-only, transient actor alias starts empty. Matching conditions
# deliberately conflict; ForceRefIntoAlias assigns only the selected employee.
def alias_condition(value):return struct.pack('<B3xfH2xIIIII',0,float(value),72,7,0,0,0,0xFFFFFFFF)
questFields=[(b'EDID',z('LocalInnServiceQuest')),(b'VMAD',vmad),(b'FULL',z('Inn Service')),
    (b'DNAM',struct.pack('<HBBII',0x11,70,0xFF,0,0)),(b'NEXT',b''),(b'ANAM',struct.pack('<I',1)),
    (b'ALST',struct.pack('<I',0)),(b'ALID',z('InvitedEmployee')),(b'FNAM',struct.pack('<I',0xC002A)),
    (b'CTDA',alias_condition(1)),(b'CTDA',alias_condition(0)),(b'ALPC',struct.pack('<I',own(0x801))),(b'ALED',b'')]
quest=record(b'QUST',questFields,own(0x800))
flag=record(b'GLOB',[(b'EDID',z('LocalInnServiceActive')),(b'FNAM',b'f'),(b'FLTV',struct.pack('<f',0))],own(0x802))
header=[(b'HEDR',struct.pack('<fII',1.7,3,0x803)),(b'CNAM',z('Local Inn Service')),
    (b'SNAM',z('Independent inn staff and shared stock. No order records or NPC overrides.')),
    (b'MAST',z('Skyrim.esm')),(b'DATA',bytes(8))]
STAGE.mkdir(parents=True,exist_ok=True)
(STAGE/NAME).write_bytes(record(b'TES4',header,0,0x200)+group(b'GLOB',flag)+group(b'QUST',quest)+group(b'PACK',package))
(STAGE/'SEQ').mkdir(exist_ok=True);(STAGE/'SEQ/Local - Inn Service.seq').write_bytes(struct.pack('<I',own(0x800)))
p=Plugin(STAGE/NAME)
assert p.masters==['skyrim.esm'] and len(p.records)==4
assert all(key[0]==p.name for key,r in p.records.items() if r['sig']!=b'TES4')
assert not any(r['sig'] in [b'NPC_',b'FACT',b'CONT',b'DIAL',b'INFO'] for r in p.records.values())
compiler=ROOT/'download-compat-audit-20261004/gotobed-vokrii-patch-20261004/tools/Caprica.exe'
imports=ROOT/'order-menu-feasibility-20261009/imports'
sys.path.insert(0,str(compiler.parent.parent));from patch_original_pex import Pex
(HERE/'compiled').mkdir(exist_ok=True)
for name in ['LocalInnService','LocalInnServiceControl']:
    run=subprocess.run([str(compiler),name+'.psc','--game=skyrim','--import',str(HERE/'source')+';'+str(imports),
        '--flags',str(imports/'TESV_Papyrus_Flags.flg'),'--output',str(HERE/'compiled')],cwd=HERE/'source',capture_output=True,text=True,encoding='utf8',errors='replace')
    (HERE/'reports'/f'{name}-compile.log').write_text(run.stdout+run.stderr,'utf8');assert run.returncode==0,run.stdout+run.stderr
    data=(HERE/'compiled'/f'{name}.pex').read_bytes();pex=Pex.__new__(Pex)
    try:pex.__init__(data)
    except AssertionError:
        assert pex.at==len(data) and struct.unpack_from('<I',data,pex.size_at)[0]==len(data)-pex.size_at-4
        data=data[:pex.size_at]+struct.pack('>I',len(data)-pex.size_at)+data[pex.size_at+4:];Pex(data)
    (STAGE/'Scripts').mkdir(exist_ok=True);(STAGE/'Source/Scripts').mkdir(parents=True,exist_ok=True)
    (STAGE/'Scripts'/f'{name}.pex').write_bytes(data);shutil.copy2(HERE/'source'/f'{name}.psc',STAGE/'Source/Scripts'/f'{name}.psc')
(HERE/'reports/records.json').write_text(json.dumps({'masters':p.masters,'own_records':3,'alias_package':hex(0x801),
    'alias_transient':True,'npc_overrides':0,'inventory_overrides':0,'order_plugin_dependency':False,'source_package':base['id']},indent=2),'utf8')
print('Service ESP: three own records, no NPC/faction/container/order overrides; two scripts compiled.')
