#include "passport_native.h"
static const passport_native_host_v1 *os;
static unsigned count;
static void render(void) {
    char buffer[128];
    const char *title=os->language()[0]=='z'?"应用\nOK: +1\n":"Native ELF\nOK: +1\n";
    unsigned n=0;
    while (*title) buffer[n++]=*title++;
    unsigned value=count, digits=1, divisor=1;
    while (value/divisor>=10 && digits<6) { divisor*=10; ++digits; }
    while (divisor) { buffer[n++]=(char)('0'+value/divisor%10); divisor/=10; }
    buffer[n]=0;
    int light=os->display_mode()[0]=='l';
    os->clear(light?0xffffff:0x101820);
    os->text(buffer,light?0x101820:0xf5f8ff);
}
static void event(uint32_t event,int32_t key) {
    if (event==PASSPORT_NATIVE_SHORT) {
        if (key==1 && count) --count;
        else if (key!=1 && count<999999) ++count;
        render();
    }
}
int passport_app_entry(const passport_native_host_v1 *host,passport_native_app_v1 *app) {
    if (!host || host->abi!=PASSPORT_NATIVE_ABI || host->size<sizeof(*host)) return -1;
    os=host; count=0;
    *app=(passport_native_app_v1){PASSPORT_NATIVE_ABI,sizeof(*app),event,0};
    render(); return 0;
}
