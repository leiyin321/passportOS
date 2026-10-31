/* Freestanding compiler support, linked into each module, never OS exports. */
#include <stddef.h>
#include <stdint.h>
void *memcpy(void *dst,const void *src,size_t n) {
    unsigned char *d=dst; const unsigned char *s=src;
    for (size_t i=0;i<n;i++) d[i]=s[i];
    return dst;
}
void *memset(void *dst,int value,size_t n) {
    unsigned char *d=dst;
    for (size_t i=0;i<n;i++) d[i]=(unsigned char)value;
    return dst;
}
void *memmove(void *dst,const void *src,size_t n) {
    unsigned char *d=dst; const unsigned char *s=src;
    if ((uintptr_t)d<(uintptr_t)s) return memcpy(dst,src,n);
    while (n) { --n; d[n]=s[n]; }
    return dst;
}
int memcmp(const void *left,const void *right,size_t n) {
    const unsigned char *a=left,*b=right;
    for (size_t i=0;i<n;i++) if (a[i]!=b[i]) return (int)a[i]-(int)b[i];
    return 0;
}
