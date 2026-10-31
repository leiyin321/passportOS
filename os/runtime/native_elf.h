#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Strict v1 subset: ELF32 LE RV32 ILP32 ET_DYN, base 0, relative relocations only. */
bool ps_native_elf_validate(const void *bytes, size_t size, size_t capacity,
                            uint32_t *span);
bool ps_native_elf_executable(const void *bytes, size_t size, uint32_t address);
