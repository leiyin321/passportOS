#include "native_json.h"
#include "package.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int ps_native_json_get(const void *json, uint32_t bytes, const char *path,
                       char *out, uint32_t capacity) {
    if (!json || !path || !out || !capacity || capacity>4096 || bytes>16384) return -1;
    out[0]=0;
    if (strlen(path)>127) return -1;
    /* Callers pass length-delimited frames, not necessarily NUL terminated. */
    char *owned=malloc((size_t)bytes+1);
    if (!owned) return -1;
    memcpy(owned,json,bytes); owned[bytes]=0;
    cJSON *root=ps_json_parse(owned,bytes);
    free(owned);
    if (!root) return -1;
    const cJSON *value=root;
    const char *part=path;
    while (*part && value) {
        const char *end=strchr(part,'.');
        size_t size=end?(size_t)(end-part):strlen(part);
        if (!size || size>63 || !cJSON_IsObject(value)) { value=NULL; break; }
        char key[64]; memcpy(key,part,size); key[size]=0;
        value=cJSON_GetObjectItemCaseSensitive(value,key);
        if (!end) break;
        part=end+1;
        if (!*part) value=NULL;
    }
    char scalar[64]; const char *text=NULL;
    if (cJSON_IsString(value)) text=value->valuestring;
    else if (cJSON_IsBool(value)) text=cJSON_IsTrue(value)?"true":"false";
    else if (cJSON_IsNumber(value)) { snprintf(scalar,sizeof(scalar),"%.17g",value->valuedouble); text=scalar; }
    int result=-1;
    if (text) {
        size_t length=strlen(text);
        if (length>=capacity) result=-2;
        else { memcpy(out,text,length+1); result=(int)length; }
    }
    cJSON_Delete(root); return result;
}
