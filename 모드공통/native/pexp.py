import struct,sys,json
def exports(path):
    d=open(path,'rb').read()
    pe=struct.unpack_from('<I',d,0x3c)[0]; magic=struct.unpack_from('<H',d,pe+24)[0]
    ddir=pe+24+(112 if magic==0x20b else 96)
    exp_rva,exp_sz=struct.unpack_from('<II',d,ddir)
    nsec=struct.unpack_from('<H',d,pe+6)[0]; soh=struct.unpack_from('<H',d,pe+20)[0]; sec=pe+24+soh
    secs=[struct.unpack_from('<8sIIII',d,sec+40*i) for i in range(nsec)]
    def off(rva):
        for n,vs,va,rs,ro in secs:
            if va<=rva<va+max(vs,rs): return rva-va+ro
    e=off(exp_rva)
    _,_,_,_,name_rva,base,nfun,nnames,af,an,ao=struct.unpack_from('<IIHHIIIIIII',d,e)
    dllname=d[off(name_rva):d.index(b'\0',off(name_rva))].decode()
    names={}
    for i in range(nnames):
        nr=struct.unpack_from('<I',d,off(an)+4*i)[0]; o=struct.unpack_from('<H',d,off(ao)+2*i)[0]
        names[o]=d[off(nr):d.index(b'\0',off(nr))].decode()
    res=[]
    for i in range(nfun):
        fr=struct.unpack_from('<I',d,off(af)+4*i)[0]
        if fr==0: continue
        fwd=d[off(fr):d.index(b'\0',off(fr))].decode() if exp_rva<=fr<exp_rva+exp_sz else None
        res.append((base+i,names.get(i),fwd))
    return dllname,res
