#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdint.h>

int debug_enabled;

void init_debug(int size);
void add_debug_info(uint8_t instruction, uint8_t cycles);
void fprintf_debug_info(FILE* stream);

#endif
