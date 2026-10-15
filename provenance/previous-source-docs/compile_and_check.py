from pathlib import Path
import hashlib,importlib.util,json,struct,subprocess,sys,zipfile
sys.dont_write_bytecode=True;sys.stdout.reconfigure(encoding='utf-8')
BASE=Path(__file__).resolve().parent;ROOT=BASE.parent
compiler=ROOT/'download-compat-audit-20261004/gotobed-vokrii-patch-20261004/tools/Caprica.exe'
imports=ROOT/'order-menu-feasibility-20261009/imports'
(BASE/'compiled').mkdir(exist_ok=True)
name='PullUpAChairOrderingQuest'
result=subprocess.run([str(compiler),name+'.psc','--game=skyrim','--import',str(BASE/'source')+';'+str(imports),'--flags',str(imports/'TESV_Papyrus_Flags.flg'),'--output',str(BASE/'compiled')],cwd=BASE/'source',capture_output=True,text=True,encoding='utf-8',errors='replace')
(BASE/'compiled/compile.log').write_text(result.stdout+result.stderr,'utf-8');assert result.returncode==0,result.stdout+result.stderr
sys.path.insert(0,str(compiler.parent.parent));from patch_original_pex import Pex
raw=(BASE/'compiled'/(name+'.pex')).read_bytes();p=Pex.__new__(Pex)
try:p.__init__(raw)
except AssertionError:
    assert p.at==len(raw) and struct.unpack_from('<I',raw,p.size_at)[0]==len(raw)-p.size_at-4
    raw=raw[:p.size_at]+struct.pack('>I',len(raw)-p.size_at)+raw[p.size_at+4:]
q=Pex(raw)
with zipfile.ZipFile(ROOT/'eating-module-20261015/dist/Pull-Up-a-Chair-0.9.9-beta-diningmodule1-FOMOD.zip') as z:
    old=Pex(z.read('00_Core/Scripts/'+name+'.pex'))
assert old.variables==q.variables and old.properties==q.properties and old.states==q.states and old.functions.keys()==q.functions.keys()
def signature(fn):
    def operand(v):return (v[0],v[1].lower() if v[0]==1 else v[1])
    return fn['metadata'],fn['params'],fn['locals'],[(o['op'],[operand(v) for v in o['args']],[operand(v) for v in o['extra']]) for o in fn['ops']]
changed=[k for k in old.functions if signature(old.functions[k])!=signature(q.functions[k])]
assert changed==['/localfindtable'],changed
(BASE/'stage/00_Core/Scripts'/(name+'.pex')).write_bytes(raw)
report={'compiled':True,'pex_parsed':True,'changed_functions':changed,'save_fields_unchanged':True,'other_functions_semantically_unchanged':True,'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)}
(BASE/'evidence/script-validation.json').write_text(json.dumps(report,indent=2),'utf-8')
print(json.dumps(report,indent=2))
