#include "session.h"
#include <assert.h>
#include <stdio.h>
static int live;
static void drop(void *p) { --*(int *)p; }
int main(void) {
  ps_session s = {0};
  uint32_t previous = 0;
  for (int n = 0; n < 500; n++) {
    ps_session_begin(&s);
    assert(!ps_session_valid(&s, previous));
    for (int i = 0; i < PS_MAX_RESOURCES; i++) {
      live++;
      previous = ps_session_own(&s, &live, drop);
      assert(ps_session_valid(&s, previous));
    }
    assert(!ps_session_own(&s, &live, drop));
    ps_session_end(&s);
    assert(live == 0 && !ps_session_valid(&s, previous));
    ps_session_end(&s);
  }
  puts("K00 PASS 500 sessions, stale handles rejected, reverse cleanup, "
       "bounded resources");
}
