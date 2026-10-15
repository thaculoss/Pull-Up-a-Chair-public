"""Scoped Skyrim PEX edit, preserving original instructions, jumps and state.

Format references: Orvid/Champollion Pex/FileReader.cpp and Instruction.cpp.
Replace the two Cidhna bed calls, their argument preparation and early returns
with one NOP per original instruction. No jump or debug index changes required.
"""
from pathlib import Path
import struct, hashlib, json, sys, re, subprocess
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parent
COUNTS=[0,3,3,3,3,3,3,3,3,3,2,2,2,2,2,3,3,3,3,3,1,2,2,3,2,3,1,3,3,3,2,2,3,3,4,4]

class Pex:
    def __init__(self,raw):
        self.raw=raw;self.at=0;self.functions={}
        assert self.take(8)==bytes.fromhex('FA57C0DE03020001')
        self.take(8)
        self.header=[self.string() for _ in range(3)]
        self.strings=[self.string() for _ in range(self.num('H'))]
        if self.num('B'):
            self.take(8)
            for _ in range(self.num('H')):
                self.take(7);self.take(2*self.num('H'))
        for _ in range(self.num('H')):self.take(3)
        assert self.num('H')==1
        self.object_start=self.at
        self.name=self.ref();self.size_at=self.at;self.object_size=self.num('I')
        self.parent=self.ref();self.doc=self.ref();self.flags=self.num('I');self.auto=self.ref()
        self.variables=[]
        for _ in range(self.num('H')):self.variables.append((self.ref(),self.ref(),self.num('I'),self.val()))
        self.properties=[]
        for _ in range(self.num('H')):
            row=[self.ref(),self.ref(),self.ref(),self.num('I'),self.num('B')]
            assert row[4]&4,'Expected auto properties only'
            row.append(self.ref());self.properties.append(row)
        self.states=[]
        for _ in range(self.num('H')):
            state=self.ref();self.states.append(state)
            for _ in range(self.num('H')):
                start=self.at;name=self.ref()
                metadata=[self.ref(),self.ref(),self.num('I'),self.num('B')]
                params=[(self.ref(),self.ref()) for _ in range(self.num('H'))]
                locals_=[(self.ref(),self.ref()) for _ in range(self.num('H'))]
                ops=[]
                for _ in range(self.num('H')):
                    a=self.at;op=self.num('B');assert op<len(COUNTS)
                    args=[self.val() for _ in range(COUNTS[op])];extra=[]
                    if op in (23,24,25):
                        t,n=self.val();assert t==3 and n>=0
                        extra=[self.val() for _ in range(n)]
                    ops.append({'op':op,'args':args,'extra':extra,'start':a,'end':self.at})
                self.functions[state+'/'+name.lower()]={'start':start,'end':self.at,'metadata':metadata,'params':params,'locals':locals_,'ops':ops}
        assert self.at==len(raw),(self.at,len(raw))
        # CK object size includes the size field itself but excludes the name.
        assert self.object_size==len(raw)-self.size_at,(self.object_size,len(raw)-self.size_at)
    def take(self,n):
        b=self.raw[self.at:self.at+n];assert len(b)==n;self.at+=n;return b
    def num(self,fmt):return struct.unpack('>'+fmt,self.take(struct.calcsize('>'+fmt)))[0]
    def string(self):return self.take(self.num('H')).decode('utf8',errors='surrogateescape')
    def ref(self):return self.strings[self.num('H')]
    def val(self):
        t=self.num('B');assert 0<=t<=5
        v=None if t==0 else self.ref() if t in (1,2) else self.num({3:'i',4:'f',5:'B'}[t])
        return (t,v)

def sha(b):return hashlib.sha256(b).hexdigest()
def op_signature(op):return (op['op'],op['args'],op['extra'])

if __name__=='__main__':
    source=ROOT/'vokrii/scripts/mineorescript.pex';raw=source.read_bytes();p=Pex(raw)
    edits=[];changed={}
    for name in ['/onactivate','/onhit']:
        ops=p.functions[name]['ops']
        matches=[i for i,o in enumerate(ops) if o['op']==23 and o['args'][0][1].lower()=='activate' and o['args'][1][1].lower()=='::cidhnamineplayerbedref_var']
        assert len(matches)==1
        j=matches[0]
        assert ops[j-2]['op']==25 and ops[j-2]['args'][0][1].lower()=='game' and ops[j-2]['args'][1][1].lower()=='getplayer'
        assert ops[j-1]['op']==14 and ops[j-1]['args'][1]==ops[j-2]['args'][2]
        assert ops[j]['extra']==[ops[j-1]['args'][0],(5,0)]
        assert ops[j+1]['op']==23 and ops[j+1]['args'][0][1].lower()=='setstage' and ops[j+1]['args'][1][1].lower()=='::dialoguecidhnamine_var' and ops[j+1]['extra']==[(3,45)]
        assert ops[j+2]['op']==26 and ops[j+2]['args']==[(0,None)]
        indices=[j-2,j-1,j,j+2];changed[name]=indices
        for i in indices:edits.append((ops[i]['start'],ops[i]['end'],b'\0'))
    assert len(edits)==8
    delta=sum(b-a-len(replacement) for a,b,replacement in edits)
    edits.append((p.size_at,p.size_at+4,struct.pack('>I',p.object_size-delta)))
    new=raw
    for a,b,replacement in sorted(edits,reverse=True):new=new[:a]+replacement+new[b:]
    q=Pex(new)
    assert p.strings==q.strings and p.variables==q.variables and p.properties==q.properties and p.states==q.states
    assert p.functions.keys()==q.functions.keys()
    unchanged=[]
    for name,f in p.functions.items():
        g=q.functions[name]
        assert (f['metadata'],f['params'],f['locals'])==(g['metadata'],g['params'],g['locals'])
        assert len(f['ops'])==len(g['ops'])
        for i,(a,b) in enumerate(zip(f['ops'],g['ops'])):
            assert op_signature(b)==((0,[],[]) if i in changed.get(name,[]) else op_signature(a)),(name,i)
        if name not in changed:
            assert raw[f['start']:f['end']]==new[g['start']:g['end']]
            unchanged.append(name)
    # Every byte outside the eight instructions and object-size field is retained.
    old_kept=[];new_kept=[];old_at=0;new_at=0
    for a,b,replacement in sorted(edits):
        n=a-old_at;old_kept.append(raw[old_at:a]);new_kept.append(new[new_at:new_at+n]);old_at=b;new_at+=n+len(replacement)
    old_kept.append(raw[old_at:]);new_kept.append(new[new_at:]);assert old_kept==new_kept
    out=ROOT/'validated/scripts/MineOreScript.pex';out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(new)
    manifest={'method':'Replace eight original instructions with NOPs; keep all other bytes except object size','source_sha256':sha(raw),'output_sha256':sha(new),'original_bytes':len(raw),'patched_bytes':len(new),'replaced_instructions':changed,'byte_identical_functions':unchanged,'variable_count':len(p.variables),'property_count':len(p.properties),'function_count':len(p.functions),'all_other_bytes_identical':True,'runtime_test':'pending'}
    (ROOT/'bytecode-verification.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
    print(json.dumps(manifest,indent=2))
    result=subprocess.run([str(ROOT/'tools/Champollion.exe'),str(out),'--psc',str(ROOT/'validated/decompiled'),'--asm',str(ROOT/'validated/asm'),'--no-debug-line'],capture_output=True,text=True,timeout=20)
    print(result.stdout,result.stderr);assert result.returncode==0
    decompiled=(ROOT/'validated/decompiled/MineOreScript.psc').read_text('utf8')
    expected=(ROOT/'patched-source/MineOreScript.psc').read_text('utf8')
    def normalize(s):return [l.strip().lower() for l in s.splitlines() if l.strip() and not l.strip().startswith(';')]
    assert normalize(decompiled)==normalize(expected),'Unexpected decompiled result'
    print('VALIDATION PASSED: patched original PEX decompiles to the exact intended merged source.')
