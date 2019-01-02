#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int debug_enabled;

void init_debug(int size);
void add_debug(uint16_t pc, uint8_t instruction, uint8_t cycles, uint16_t extra, uint8_t extra_flag, int cb);
void fprintf_debug_info(FILE* stream);
void printf_debug_op_count();
void print_registers();
void print_memory();

#endif
