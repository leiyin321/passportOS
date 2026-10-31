#include "storage_layout.h"
#include <string.h>
bool ps_storage_layout_make(uint32_t flash, uint32_t slots, uint32_t image,
                            uint32_t package_partition, uint32_t package_total,
                            uint32_t package_used, uint32_t data_partition,
                            ps_storage_layout *out) {
  if (!out) return false;
  memset(out,0,sizeof(*out));
  if (!flash || !image || image>slots || package_used>package_total || package_total>package_partition ||
      (uint64_t)slots+package_partition+data_partition>flash) return false;
  out->total=flash;
  out->bytes[PS_STORAGE_OS]=image;
  out->bytes[PS_STORAGE_OTA]=slots-image;
  out->bytes[PS_STORAGE_PACKAGES]=package_used;
  out->bytes[PS_STORAGE_FREE]=package_total-package_used;
  out->bytes[PS_STORAGE_DATA]=data_partition;
  out->bytes[PS_STORAGE_SYSTEM]=flash-slots-package_total-data_partition;
  return true;
}
unsigned ps_storage_layout_edge(const ps_storage_layout *layout,unsigned count,unsigned width) {
  if(!layout || !layout->total)return 0;
  if(count>PS_STORAGE_PARTS)count=PS_STORAGE_PARTS;
  uint64_t sum=0;for(unsigned i=0;i<count;i++)sum+=layout->bytes[i];
  return (unsigned)(sum*width/layout->total);
}
