#include "gesture.h"
bool bt_gesture_begin(bt_gesture *g,unsigned kind,int value,uint32_t now) {
  if(!g || g->active || kind!=BT_BUTTON || (value!=1 && value!=2 && value!=4))return false;
  *g=(bt_gesture){.active=true,.kind=kind,.value=value,.due=now};return true;
}
bool bt_gesture_packet(const bt_gesture *g,uint32_t now,unsigned *id,uint8_t d[4],unsigned *n) {
  if(!g || !g->active || (int32_t)(now-g->due)<0)return false;
  for(unsigned i=0;i<4;i++)d[i]=0;
  *id=1;*n=4;
  if(!g->phase)d[0]=g->value;
  return true;
}
void bt_gesture_commit(bt_gesture *g,uint32_t now) {
  if(!g || !g->active)return;
  if(++g->phase>=2)g->active=false;
  g->due=now+50;
}
