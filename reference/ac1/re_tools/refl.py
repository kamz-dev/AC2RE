"""Reflection helpers: find class descriptors by hash, dump properties."""
import struct
from pe import *
from vt import find_all_u32
_data=[s for s in SECS if s[0]=='.data'][0]
def class_by_hash(h):
    out=[]
    for r in find_all_u32(h):
        if sec(r)!='.data': continue
        nm=cstr(u32(r-8)) if u32(r-8) else None
        if nm: out.append((r-8,nm))
    return out
TYPES={0x00:'bool',0x02:'u16',0x03:'u8?',0x05:'?5',0x06:'?6',0x07:'u32/int',0x0a:'float',0x0d:'?d',0x12:'?12',0x13:'struct',0x14:'ptr',0x16:'inline-class',0x1d:'array',0x1c:'accessor'}
def props(desc):
    """desc = address of class descriptor (name ptr). property list is {ptr,count} at desc-0x20."""
    lst,cnt=u32(desc-0x20),u32(desc-0x1c)
    out=[]
    if not lst or not cnt or cnt>200: return out
    for i in range(cnt):
        a=lst+i*32
        w=[u32(a+j*4) for j in range(8)]
        ty=(w[3]>>16)&0xff; el=(w[3]>>24)&0xff
        off=(w[4]>>16)>>2
        th=w[2]
        tn=class_by_hash(th) if th else []
        out.append(dict(addr=a,flags=w[0],name_hash=w[1],type_hash=th,type_name=(tn[0][1] if tn else None),ty=ty,el=el,off=off,w=w))
    return out
def show(desc):
    print(f'{cstr(u32(desc))} desc {desc:#x} parent {u32(desc+4):#x} hash {u32(desc+8):#x} size {u32(desc+12):#x}')
    for p in props(desc):
        print(f"  +{p['off']:#05x} ty={p['ty']:#04x}({TYPES.get(p['ty'],'?')}) el={p['el']:#04x} namehash={p['name_hash']:08x} type={p['type_hash']:08x} {p['type_name'] or ''}  raw={' '.join('%08x'%x for x in p['w'])}")
