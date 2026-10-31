#include "asset_loader.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned u32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
int main(void){
 const char *path="examples/packages/appstore_grid.pap";FILE *f=fopen(path,"rb");assert(f);unsigned char header[16],code[4];assert(fread(header,1,16,f)==16&&!memcmp(header,"PAP2",4));unsigned ml=u32(header+4),pl=u32(header+8);char *raw=calloc(ml+1,1);assert(fread(raw,1,ml,f)==ml);assert(fread(code,1,4,f)==4);ps_package p={0};p.manifest=ps_json_parse_budget(raw,ml,4096);free(raw);assert(p.manifest);snprintf(p.identity,sizeof(p.identity),"first");snprintf(p.package_path,sizeof(p.package_path),"%s",path);p.asset_base=16+ml+4+u32(code);p.asset_bytes=pl-4-u32(code);fclose(f);
 ps_assets_budget(16384);uint32_t initial=0;
 for(unsigned page=0;page<100;page++){
  uint32_t next[6];for(unsigned i=0;i<6;i++){char uri[64];snprintf(uri,sizeof(uri),"app://icons/item-%02u",(page%5)*6+i);next[i]=ps_asset_open(&p,uri);assert(next[i]);if(!initial)initial=next[i];}
  ps_assets_unpin();for(unsigned i=0;i<6;i++)ps_asset_pin(next[i]);assert(ps_assets_metrics()->current<=16384);
 }
 const char *bad[]={"../icons/item-00","file:///nvs","app://../other","app://icons/../../app","app://other.app/icon","http://host/img","app://icons/not-declared"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!ps_asset_open(&p,bad[i]));
 ps_assets_unpin();ps_assets_close();assert(ps_assets_metrics()->current==0&&!ps_asset_get(initial,NULL));
 uint32_t fresh=ps_asset_open(&p,"app://icons/item-00");assert(fresh&&fresh!=initial);snprintf(p.identity,sizeof(p.identity),"second");uint32_t other=ps_asset_open(&p,"app://icons/item-00");assert(other&&other!=fresh);ps_assets_close();
 char copy[]="/tmp/passport-assets-XXXXXX";int fd=mkstemp(copy);assert(fd>=0);FILE *out=fdopen(fd,"wb");f=fopen(path,"rb");assert(f&&out);unsigned char chunk[1024];size_t n;while((n=fread(chunk,1,sizeof(chunk),f)))assert(fwrite(chunk,1,n,out)==n);fclose(f);fclose(out);
 out=fopen(copy,"r+b");assert(out);assert(!fseek(out,p.asset_base+1240-1,SEEK_SET));int byte=fgetc(out);assert(!fseek(out,-1,SEEK_CUR));fputc(byte^1,out);fclose(out);snprintf(p.package_path,sizeof(p.package_path),"%s",copy);assert(!ps_asset_open(&p,"app://icons/item-00"));assert(ps_assets_metrics()->current==0);unlink(copy);cJSON_Delete(p.manifest);
 puts("I3/I4 PASS 600 bounded loads, LRU/pins, cross-identity handles, stale tokens, corrupt hashes, close bytes=0");
}
