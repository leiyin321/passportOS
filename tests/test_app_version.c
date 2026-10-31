#include <assert.h>
#include <stdio.h>
#include "../os/services/storage/app_version.h"

int main(void) {
  assert(ps_app_version_compare("1.2.3", "1.2.4") == -1);
  assert(ps_app_version_compare("1.2.3", "1.2.3") == 0);
  assert(ps_app_version_compare("2.0.0", "1.99.99") == 1);
  assert(ps_app_version_compare("1.10.0", "1.9.99") == 1);
  assert(ps_app_version_compare("1.2", "1.2.0") == -2);
  assert(ps_app_version_compare("1.2.3-beta", "1.2.3") == -2);
  assert(ps_app_version_compare("4294967296.0.0", "1.0.0") == -2);
  puts("App version comparison tests: PASS");
}
