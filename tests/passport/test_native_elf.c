#include "native_elf.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  assert(argc == 2);
  FILE *f = fopen(argv[1], "rb");
  assert(f);
  unsigned char original[32768], data[32768];
  size_t size = fread(original, 1, sizeof(original), f);
  fclose(f);
  assert(ps_native_elf_validate(original, size, 16384, NULL));
  for (size_t n = 0; n < size; n++)
    (void)ps_native_elf_validate(original, n, 16384, NULL);
  uint32_t random = 775;
  for (unsigned i = 0; i < 20000; i++) {
    memcpy(data, original, size);
    for (unsigned j = 0; j < 4; j++) {
      random = random * 1664525u + 1013904223u;
      data[random % size] ^= (unsigned char)(random >> 24);
    }
    (void)ps_native_elf_validate(data, i % 3 ? size : random % size, 16384,
                                 NULL);
  }
  puts("Native ELF parser PASS ASan/UBSan truncation and 20000 mutations");
}
