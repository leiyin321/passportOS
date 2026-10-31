#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { PS_PORTAL_WAITING,PS_PORTAL_CONNECTING,PS_PORTAL_CONNECTED,PS_PORTAL_FAILED } ps_portal_state;
typedef struct {ps_portal_state state;bool matching;uint32_t timeout_ms,submitted_ms,match_ms,connected_ms;} ps_portal_flow;
void ps_portal_submit(ps_portal_flow *flow,uint32_t now_ms);
void ps_portal_observe(ps_portal_flow *flow,bool exact_network,uint32_t now_ms);
void ps_portal_fail(ps_portal_flow *flow);
bool ps_portal_done(const ps_portal_flow *flow,uint32_t now_ms);
unsigned ps_portal_advance(unsigned step,bool *advanced,bool phone_has_ip);
