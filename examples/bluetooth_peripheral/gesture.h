#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { BT_BUTTON=1 };
typedef struct { bool active; unsigned kind,phase; int value; uint32_t due; } bt_gesture;
bool bt_gesture_begin(bt_gesture *g,unsigned kind,int value,uint32_t now);
bool bt_gesture_packet(const bt_gesture *g,uint32_t now,unsigned *id,uint8_t data[4],unsigned *n);
void bt_gesture_commit(bt_gesture *g,uint32_t now);
