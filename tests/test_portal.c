#include "portal_flow.h"
#include "portal_dns.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 ps_portal_flow f={0};assert(!ps_portal_done(&f,9999));ps_portal_submit(&f,0);
 ps_portal_observe(&f,false,100);assert(f.state==PS_PORTAL_CONNECTING);
 ps_portal_observe(&f,true,200);ps_portal_observe(&f,true,1699);assert(f.state==PS_PORTAL_CONNECTING);
 ps_portal_observe(&f,false,1700);ps_portal_observe(&f,true,1800);ps_portal_observe(&f,true,3300);assert(f.state==PS_PORTAL_CONNECTED);
 assert(!ps_portal_done(&f,5799));assert(ps_portal_done(&f,5800));ps_portal_observe(&f,false,5801);assert(!ps_portal_done(&f,9000));
 ps_portal_fail(&f);ps_portal_observe(&f,true,9900);assert(f.state==PS_PORTAL_FAILED);
 ps_portal_submit(&f,UINT32_MAX-500);ps_portal_observe(&f,true,UINT32_MAX-500);ps_portal_observe(&f,true,999);assert(f.state==PS_PORTAL_CONNECTED);assert(ps_portal_done(&f,3499));
 ps_portal_submit(&f,100);ps_portal_observe(&f,false,40100);assert(f.state==PS_PORTAL_FAILED);
 ps_portal_submit(&f,100);f.timeout_ms=300000;ps_portal_observe(&f,false,60000);assert(f.state==PS_PORTAL_CONNECTING);ps_portal_observe(&f,true,60100);ps_portal_observe(&f,true,61600);assert(f.state==PS_PORTAL_CONNECTED);
 ps_portal_submit(&f,20);f.timeout_ms=300000;ps_portal_observe(&f,false,300019);assert(f.state==PS_PORTAL_CONNECTING);ps_portal_observe(&f,false,300020);assert(f.state==PS_PORTAL_FAILED);
 bool once=false;assert(ps_portal_advance(0,&once,false)==0);assert(ps_portal_advance(0,&once,true)==1);assert(ps_portal_advance(0,&once,true)==0);once=false;assert(ps_portal_advance(2,&once,true)==2&&once);
 uint8_t q[512]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,7,'c','a','p','t','i','v','e',5,'a','p','p','l','e',3,'c','o','m',0,0,1,0,1},out[512];
 size_t n=35,z=ps_portal_dns_reply(q,n,out,sizeof(out));assert(z==51&&out[2]==0x85&&out[7]==1&&!memcmp(out+47,(uint8_t[]){192,168,4,1},4));
 assert(!ps_portal_dns_reply(q,n,out,50));q[32]=28;assert(ps_portal_dns_reply(q,n,out,sizeof(out))==n&&out[7]==0);q[32]=1;
 uint8_t copy[512];memcpy(copy,q,n);assert(ps_portal_dns_reply(copy,n,copy,sizeof(copy))==51);
 q[12]=0xc0;assert(!ps_portal_dns_reply(q,n,out,sizeof(out)));q[12]=7;q[2]=0x81;assert(!ps_portal_dns_reply(q,n,out,sizeof(out)));q[2]=1;
 for(size_t i=0;i<n;i++)assert(!ps_portal_dns_reply(q,i,out,sizeof(out)));
 for(unsigned len=0;len<=512;len++){memset(copy,0xff,sizeof(copy));(void)ps_portal_dns_reply(copy,len,out,sizeof(out));}
 puts("Captive portal PASS exact-network stability, disconnect/error/resubmit, completion delay/wrap, one-shot phone transition, DNS A/AAAA/in-place/bounds/malformed");
}
