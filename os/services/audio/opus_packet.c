#include "opus_packet.h"
#include <string.h>
size_t ps_opus_join_20ms(const uint8_t p[3][PS_OPUS_PART_MAX],const uint16_t n[3],
                        uint8_t *out,size_t cap) {
    if (!p||!n||!out||!n[0]||n[0]>PS_OPUS_PART_MAX) return 0;
    uint8_t toc=p[0][0];
    /* CELT-only 20ms, mono, single frame; fixed by the voice encoder profile. */
    if (!(toc&0x80) || (toc&7) || ((toc>>3)&3)!=3) return 0;
    size_t payload=n[0]-1,bytes=2+payload*3;
    if (cap<bytes) return 0;
    for(unsigned i=1;i<3;i++) if(n[i]!=n[0] || p[i][0]!=toc) return 0;
    out[0]=toc|3;out[1]=3; /* CBR, no padding, 3 frames. */
    for(unsigned i=0;i<3;i++) memcpy(out+2+i*payload,p[i]+1,payload);
    return bytes;
}
