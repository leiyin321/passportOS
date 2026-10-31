#include "portal_flow.h"
void ps_portal_submit(ps_portal_flow*f,uint32_t now){*f=(ps_portal_flow){.state=PS_PORTAL_CONNECTING,.timeout_ms=40000,.submitted_ms=now};}
void ps_portal_fail(ps_portal_flow*f){*f=(ps_portal_flow){.state=PS_PORTAL_FAILED};}
void ps_portal_observe(ps_portal_flow*f,bool exact,uint32_t now){
 if(f->state!=PS_PORTAL_CONNECTING&&f->state!=PS_PORTAL_CONNECTED)return;
 if(!exact){f->matching=false;f->state=PS_PORTAL_CONNECTING;if((uint32_t)(now-f->submitted_ms)>=f->timeout_ms)ps_portal_fail(f);return;}
 if(!f->matching){f->matching=true;f->match_ms=now;}
 if(f->state!=PS_PORTAL_CONNECTED && (uint32_t)(now-f->match_ms)>=1500){f->state=PS_PORTAL_CONNECTED;f->connected_ms=now;}
}
bool ps_portal_done(const ps_portal_flow*f,uint32_t now){return f->state==PS_PORTAL_CONNECTED&&(uint32_t)(now-f->connected_ms)>=2500;}
unsigned ps_portal_advance(unsigned step,bool*advanced,bool phone){
 if(phone&&!*advanced){*advanced=true;if(step==0)return 1;}
 return step;
}
