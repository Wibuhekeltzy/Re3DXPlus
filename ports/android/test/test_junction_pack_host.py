"""Validate a generated private sidecar, without copying original assets into tests."""
import hashlib, struct, sys, zlib
from pathlib import Path

def main():
    root=Path(sys.argv[1]); table=(root/'junction-table.bin').read_bytes()
    assert len(table)==384
    assert hashlib.sha256(table).hexdigest()=='5841afd5a55d625c209ce7ed8484b3afd66683ff11fa670e6bb35a3a73b6a15f'
    b=(root/'junction-pages.wcp').read_bytes(); magic,ver,ns,na=struct.unpack_from('<4sIII',b)
    # Golden hash for the verified local Python/Pillow/zlib build; compression
    # implementations may legitimately produce different bytes on other hosts.
    if len(sys.argv)>2:
        assert hashlib.sha256(b).hexdigest()==sys.argv[2]
    assert magic==b'WCP1' and ver==1 and na==4
    pos=16; ids={}
    for _ in range(ns):
        sid,w,h,n=struct.unpack_from('<iHHI',b,pos); pos+=12
        assert sid not in ids and w>0 and h>0 and n>0 and pos+n<=len(b)
        raw=zlib.decompress(b[pos:pos+n]);pos+=n;assert len(raw)==w*h*4
        ids[sid]=(w,h,hashlib.sha256(raw).hexdigest())
    for i,wh in {246:(288,36),248:(148,48),249:(148,48),250:(148,48),251:(148,48),252:(108,24)}.items():
        assert ids[102000+i][:2]==wh
    for sid,wh in {0x569:(256,256),0x7d5:(512,512),0x7d6:(512,512)}.items():
        assert ids[sid][:2]==wh
    names=set()
    for _ in range(na):
        name,nf,nr=struct.unpack_from('<48sII',b,pos);pos+=56; name=name.split(bytes([0]))[0].decode('ascii');names.add(name)
        frames=[struct.unpack_from('<II',b,pos+i*8) for i in range(nf)];pos+=nf*8
        assert all(a+c<=nr for a,c in frames)
        for i in range(nr):
            sid=struct.unpack_from('<i',b,pos+i*46)[0]; assert sid in ids
        pos+=nr*46
    assert pos==len(b) and names=={'cons_g_bunki_flash','cons_g_bunki_mes_next','cons_g_bunki_mes_select','cons_g_bunki_select'}
    print('private resource structure/pixels: OK',len(ids),'sha256',hashlib.sha256(b).hexdigest())
if __name__=='__main__': main()
