#include "mem.h"

void set_mem(uint16_t dest, uint8_t data) {
	gb_mem[dest] = data;
	if (dest >= INTERNAL_RAM0 && dest <= 0xDDFF) {
		gb_mem[data + ECHO_OFFSET] = data;
	} else if (dest >= ECHO_RAM && dest <= 0xFDFF) {
		gb_mem[data - ECHO_OFFSET] = data;
	}
	// TODO: look at specifics of some special registers
}
