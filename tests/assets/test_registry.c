#include "icon_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){assert(ps_icon_count()>=12);assert(ps_icon_index("missing")==0);assert(ps_icon_get(999)==ps_icon_get(0));for(unsigned i=0;i<ps_icon_count();i++){const ps_system_icon *p=ps_icon_get(i);assert(ps_icon_index(p->id)==i);assert(p->symbol||p->alpha);for(unsigned j=i+1;j<ps_icon_count();j++)assert(strcmp(p->id,ps_icon_get(j)->id));}puts("I1 PASS >=12 stable IDs, one-copy registry and unknown-ID fallback");}
