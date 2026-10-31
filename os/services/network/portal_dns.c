#include "portal_dns.h"
#include <string.h>
size_t ps_portal_dns_reply(const uint8_t*q,size_t n,uint8_t*out,size_t cap){
 if(!q||!out||n<17||n>512||(q[2]&0xfe)||q[4]||q[5]!=1)return 0;
 size_t at=12,total=0;
 while(at<n && q[at]){unsigned len=q[at++];if(len>63||at+len>=n||total+len+1>254)return 0;at+=len;total+=len+1;}
 if(at>=n||at+5>n)return 0;
 at++;unsigned type=(unsigned)q[at]<<8|q[at+1],cls=(unsigned)q[at+2]<<8|q[at+3];at+=4;
 if(cls!=1)return 0;
 size_t bytes=at+(type==1?16:0);if(bytes>cap)return 0;
 uint8_t flags=q[2]&1;memmove(out,q,at);
 out[2]=0x84|flags;out[3]=0;out[6]=0;out[7]=type==1;out[8]=out[9]=out[10]=out[11]=0;
 if(type==1){const uint8_t answer[16]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};memcpy(out+at,answer,16);}
 return bytes;
}
