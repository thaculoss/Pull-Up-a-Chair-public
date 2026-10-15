"""Skyrim PEX reader, extracted from the local structural validation tool."""
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


