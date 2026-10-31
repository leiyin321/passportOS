#pragma once

// Compares strict MAJOR.MINOR.PATCH strings. Returns -1/0/1 for older/equal/
// newer, or -2 when either input is invalid.
int ps_app_version_compare(const char *installed, const char *catalog);
