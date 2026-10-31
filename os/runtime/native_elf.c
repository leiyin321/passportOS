#include "native_elf.h"
#include <string.h>
static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1]<<8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p+2)<<16; }
static bool bounds(size_t n, uint32_t offset, uint32_t size) { return offset<=n && size<=n-offset; }
static bool loaded(const uint8_t *b, uint32_t address, uint32_t length, unsigned flags) {
    uint32_t off=u32(b+28); unsigned count=u16(b+44);
    for (unsigned i=0;i<count;i++) {
        const uint8_t *p=b+off+32*i;
        if (u32(p)!=1 || (u32(p+24)&flags)!=flags) continue;
        uint32_t start=u32(p+8), size=u32(p+((flags&1)?16:20));
        if (address>=start && address-start<=size && length<=size-(address-start)) return true;
    }
    return false;
}
bool ps_native_elf_executable(const void *bytes, size_t size, uint32_t address) {
    const uint8_t *b=bytes;
    if (!b || size<52 || !bounds(size,u32(b+28),(uint32_t)u16(b+44)*32)) return false;
    return !(address&1u) && loaded(b,address,2,1);
}
bool ps_native_elf_validate(const void *bytes, size_t size, size_t capacity, uint32_t *span) {
    const uint8_t *b=bytes;
    if (!b || size<52 || memcmp(b,"\177ELF\1\1\1",7) ||
        u16(b+16)!=3 || u16(b+18)!=243 || u32(b+20)!=1 ||
        (u32(b+36)&~1u) || u16(b+40)!=52 || u16(b+42)!=32 || u16(b+46)!=40) return false;
    unsigned phn=u16(b+44), shn=u16(b+48), names=u16(b+50);
    uint32_t pho=u32(b+28), sho=u32(b+32), end=0; bool first=true;
    if (!phn || phn>8 || !shn || shn>64 || names>=shn ||
        !bounds(size,pho,phn*32) || !bounds(size,sho,shn*40)) return false;
    for (unsigned i=0;i<phn;i++) {
        const uint8_t *p=b+pho+32*i; uint32_t type=u32(p);
        if (type==7) return false; /* TLS unsupported */
        if (type!=1) continue;
        uint32_t start=u32(p+8), files=u32(p+16), mem=u32(p+20), align=u32(p+28);
        if (files>mem || !bounds(size,u32(p+4),files) || start>capacity || mem>capacity-start ||
            (first && start) || (!first && start<end) || (align && (align&(align-1)))) return false;
        end=start+mem; first=false;
    }
    if (first || !end || !ps_native_elf_executable(b,size,u32(b+24))) return false;
    const uint8_t *ns=b+sho+40*names;
    if (u32(ns+4)!=3 || !bounds(size,u32(ns+16),u32(ns+20))) return false;
    for (unsigned i=0;i<shn;i++) {
        const uint8_t *s=b+sho+40*i;
        uint32_t type=u32(s+4), off=u32(s+16), len=u32(s+20), link=u32(s+24), align=u32(s+32);
        if ((u32(s+8)&0x400u) || type==9 || type==14 || type==15 || type==16 ||
            link>=shn || (align && (align&(align-1))) ||
            (type!=8 && !bounds(size,off,len))) return false;
        uint32_t name=u32(s);
        if (name>=u32(ns+20) || !memchr(b+u32(ns+16)+name,0,u32(ns+20)-name)) return false;
        if (type==2 || type==11) {
            const uint8_t *strings=b+sho+40*link;
            if (u32(s+36)!=16 || len%16 || u32(strings+4)!=3 || !bounds(size,u32(strings+16),u32(strings+20))) return false;
            for (uint32_t j=0;j<len;j+=16) {
                const uint8_t *sym=b+off+j; uint32_t name=u32(sym);
                if (name>=u32(strings+20) || !memchr(b+u32(strings+16)+name,0,u32(strings+20)-name) ||
                    (u16(sym+14)==0 && name)) return false; /* No unresolved imports */
            }
        }
        if (type==4) {
            const uint8_t *syms=b+sho+40*link;
            if (u32(s+36)!=12 || len%12 || (u32(syms+4)!=2 && u32(syms+4)!=11) || u32(syms+20)<16) return false;
            for (uint32_t j=0;j<len;j+=12) {
                const uint8_t *r=b+off+j; uint32_t where=u32(r), info=u32(r+4), addend=u32(r+8);
                if (info!=3 || (where&3) || !loaded(b,where,4,2) || !loaded(b,addend,1,0)) return false;
            }
        }
    }
    if (span) *span=end;
    return true;
}
