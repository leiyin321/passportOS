#include "mp3_frames.h"
#include <string.h>
unsigned ps_mp3_frame_size(const uint8_t h[4]) {
  unsigned ver=(h[1]>>3)&3,layer=(h[1]>>1)&3,rate=(h[2]>>2)&3,br=h[2]>>4;
  if(h[0]!=255 || (h[1]&224)!=224 || ver==1 || layer!=1 || rate==3 || !br || br==15)return 0;
  static const unsigned rates[]={44100,48000,32000};
  static const unsigned m1[]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
  static const unsigned m2[]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
  unsigned hz=rates[rate]/(ver==3?1:ver==2?2:4);
  return (ver==3?144000*m1[br]:72000*m2[br])/hz+((h[2]>>1)&1);
}
bool ps_mp3_feed(ps_mp3_frames *s,const void *input,size_t n,ps_mp3_frame_fn sink,void *ctx) {
  if(!s || (!input&&n) || !sink)return false;
  const uint8_t *p=input;
  while(n) {
    if(s->skip){unsigned k=n<s->skip?(unsigned)n:s->skip;s->skip-=k;p+=k;n-=k;continue;}
    if(s->used==sizeof(s->data))return false;
    s->data[s->used++]=*p++;n--;
    if(s->used>=3 && !memcmp(s->data,"ID3",3)) {
      if(s->used<10)continue;
      unsigned bytes=0;
      for(unsigned i=6;i<10;i++){if(s->data[i]&128)return false;bytes=(bytes<<7)|s->data[i];}
      if(bytes>1048576)return false;
      s->skip=bytes+((s->data[5]&16)?10:0);s->used=0;continue;
    }
    if(s->used<4)continue;
    unsigned frame=ps_mp3_frame_size(s->data);
    if(!frame){if(++s->discarded>65536)return false;memmove(s->data,s->data+1,--s->used);continue;}
    if(frame>sizeof(s->data))return false;
    if(s->used<frame)continue;
    if(!sink(s->data,frame,ctx))return false;
    s->used=0;s->discarded=0;
  }
  return true;
}
