#include "redirect.h"
#include "policy.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){char out[512];const char *base="https://first.example/apps/start";
 assert(ps_redirect_url(base,"https://second.example/result",out,sizeof(out)));assert(!strcmp(out,"https://second.example/result"));
 assert(ps_endpoint_allowed("https","second.example",443,0x08080808,"second.example",0,0));
 assert(!ps_endpoint_allowed("https","second.example",443,0xc0a80101,"second.example",0,0));
 assert(!ps_endpoint_allowed("https","evil.example",443,0x08080808,"second.example",0,0));
 assert(ps_redirect_url(base,"/next",out,sizeof(out))&&!strcmp(out,"https://first.example/next"));
 assert(ps_redirect_url(base,"next?q=1",out,sizeof(out))&&!strcmp(out,"https://first.example/apps/next?q=1"));
 assert(ps_redirect_url(base,"//second.example/x",out,sizeof(out))&&!strcmp(out,"https://second.example/x"));
 assert(!ps_redirect_url(base,"https://user@host/x",out,sizeof(out)));
 assert(!ps_redirect_url(base,"/x\r\nInjected:yes",out,sizeof(out)));
 assert(ps_redirect_status(301)&&ps_redirect_status(308)&&!ps_redirect_status(200));assert(PS_REDIRECT_MAX==3);
 puts("N07-N10 PASS relative/absolute redirects, alternate allowlisted domain, rebinding and unauthorized target rejected; hop cap=3");
}
