#include "native_json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    char out[20];
    const char bounded[7]={'{','"','a','"',':','1','}'};
    assert(ps_native_json_get(bounded,sizeof(bounded),"a",out,sizeof(out))==1);
    const char json[]="{\"obj\":{\"str\":\"A\\n中文\",\"rate\":24000,\"bool\":true}}";
    assert(ps_native_json_get(json,strlen(json),"obj.str",out,sizeof(out))==8);
    assert(!strcmp(out,"A\n中文"));
    assert(ps_native_json_get(json,strlen(json),"obj.rate",out,sizeof(out))==5);
    assert(!strcmp(out,"24000"));
    assert(ps_native_json_get(json,strlen(json),"obj.bool",out,sizeof(out))==4);
    assert(!strcmp(out,"true"));
    assert(ps_native_json_get(json,strlen(json),"obj.str",out,2)==-2 && !out[0]);
    assert(ps_native_json_get(json,strlen(json),"obj.",out,sizeof(out))==-1);
    assert(ps_native_json_get(json,strlen(json),"obj..str",out,sizeof(out))==-1);
    assert(ps_native_json_get("{\"a\":1,\"a\":2}",13,"a",out,sizeof(out))==-1);
    assert(ps_native_json_get("{\"a\":\"x\\u0000y\"}",17,"a",out,sizeof(out))==-1);
    assert(ps_native_json_get(json,strlen(json)-1,"obj.str",out,sizeof(out))==-1);
    puts("Native JSON PASS bounded paths/decoded UTF8/scalars/duplicates/truncation");
}
