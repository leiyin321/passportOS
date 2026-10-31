#include "../os/services/storage/storage_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  ps_storage_layout l;
  assert(ps_storage_layout_make(8388608,5242880,2422048,2490368,2281472,71680,524288,&l));
  uint64_t sum=0;unsigned previous=0;
  for(unsigned i=0;i<PS_STORAGE_PARTS;i++){
    sum+=l.bytes[i];unsigned edge=ps_storage_layout_edge(&l,i+1,216);
    assert(edge>=previous && edge<=216);previous=edge;
  }
  assert(sum==8388608 && previous==216);
  assert(PS_STORAGE_FREE == PS_STORAGE_PARTS-1);
  assert(ps_storage_layout_edge(&l,PS_STORAGE_FREE,216)<216);
  assert(l.bytes[PS_STORAGE_FREE]==2209792);
  assert(l.bytes[PS_STORAGE_OTA]==2820832);
  assert(l.bytes[PS_STORAGE_SYSTEM]==339968);
  assert(ps_storage_layout_edge(&l,1,216)>60); /* OS occupies real Flash proportion. */
  assert(ps_storage_layout_make(8388608,5242880,2422048,2490368,2281472,0,524288,&l));
  assert(l.bytes[PS_STORAGE_PACKAGES]==0);
  assert(ps_storage_layout_make(8388608,5242880,2422048,2490368,2281472,2281472,524288,&l));
  assert(l.bytes[PS_STORAGE_FREE]==0);
  assert(!ps_storage_layout_make(0,0,0,0,0,0,0,&l));
  assert(!ps_storage_layout_make(10,5,6,2,2,0,1,&l));
  assert(!ps_storage_layout_make(10,5,4,2,2,3,1,&l));
  assert(!ps_storage_layout_make(10,5,4,2,3,1,1,&l));
  assert(!ps_storage_layout_make(10,UINT32_MAX,4,UINT32_MAX,2,1,1,&l));
  assert(l.total==0 && ps_storage_layout_edge(&l,6,216)==0);
  puts("Storage whole-Flash accounting/rounding/bounds PASS");
}
