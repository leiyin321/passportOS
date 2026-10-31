#include "hid_protocol.h"
#include <assert.h>
int main(void) {
  unsigned char m[4]={1,127,129,0};
  assert(ps_hid_report_valid(1,m,4));m[0]=8;assert(!ps_hid_report_valid(1,m,4));
  m[0]=0;m[1]=128;assert(!ps_hid_report_valid(1,m,4));
  assert(!ps_hid_report_valid(1,NULL,4));assert(!ps_hid_report_valid(1,m,3));
  unsigned char c[2]={0xe9,0};assert(ps_hid_report_valid(2,c,2));
  c[0]=0;assert(ps_hid_report_valid(2,c,2));c[0]=0x30;assert(!ps_hid_report_valid(2,c,2));
  c[0]=0xe9;c[1]=1;assert(!ps_hid_report_valid(2,c,2));assert(!ps_hid_report_valid(3,c,2));
  /* Parse short HID items and independently verify wire report lengths. */
  unsigned bits[3]={0},id=0,size=0,count=0,depth=0;
  for(size_t off=0;off<ps_hid_report_map_size;) {
    unsigned p=ps_hid_report_map[off++],n=p&3u;if(n==3)n=4;
    assert(p!=254 && off+n<=ps_hid_report_map_size);
    unsigned v=0;for(unsigned i=0;i<n;i++)v|=(unsigned)ps_hid_report_map[off++]<<(i*8);
    unsigned type=(p>>2)&3,tag=p>>4;
    if(type==1 && tag==8){id=v;assert(id>0 && id<3);}
    if(type==1 && tag==7)size=v;
    if(type==1 && tag==9)count=v;
    if(type==0 && tag==8){assert(id);bits[id]+=size*count;}
    if(type==0 && tag==10)depth++;
    if(type==0 && tag==12){assert(depth);depth--;}
    assert(!(type==0 && (tag==9 || tag==11))); /* No output/feature surface. */
  }
  assert(!depth && bits[1]==32 && bits[2]==16);
}
