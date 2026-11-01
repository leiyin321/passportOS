from pathlib import Path
import subprocess,tempfile
s=Path('main/passport_main.c').read_text();a=s.index('static void refresh_store_catalog(void)');b=s.index('\nstatic bool installed_app_is_current',a);helper=s[a:b]
program=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static void *catalog=(void*)1;static int catalog_selection=2,store_request,deleted,fetches;
static char store_installed_id[64]="old",notice[160],store_url[512]="https://example.com/apps/index.json",service_ip[64]="192.168.1.10";
static unsigned service_http=8040;static bool service_approved,service_pending,accept=true;
static void cJSON_Delete(void *p){if(p)deleted++;}
static bool ps_net_fetch(void*p,const char*u,uint32_t generation,bool store){(void)p;assert(*u);assert(generation==0xffffffff);assert(store);fetches++;return accept;}
'''+helper+r'''
int main(void){
refresh_store_catalog();assert(!catalog&&deleted==1&&fetches==1&&store_request==1&&!catalog_selection&&!store_installed_id[0]);
refresh_store_catalog();assert(fetches==1);
store_request=0;catalog=(void*)1;refresh_store_catalog();assert(deleted==2&&fetches==2);
store_request=2;catalog=(void*)1;refresh_store_catalog();assert(catalog&&fetches==2&&deleted==2);
store_request=0;accept=false;refresh_store_catalog();assert(!catalog&&!store_request&&strstr(notice,"Offline"));
accept=true;refresh_store_catalog();assert(store_request==1&&fetches==4);
store_request=0;store_url[0]=0;refresh_store_catalog();assert(service_pending&&fetches==4);
service_approved=true;refresh_store_catalog();assert(fetches==5&&store_request==1);
puts("Store refresh PASS repeat entry, pending transfer, stale rejection, offline retry and consent");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(program);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
