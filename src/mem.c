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

struct lcdc *get_lcdc() {
	return (struct lcdc *)&gb_mem[LCDC];
}

struct stat *get_stat() {
	return (struct stat *)&gb_mem[STAT];
}

struct interrupt_flag *get_if() {
	return (struct interrupt_flag *)&gb_mem[IF];
}

struct sprite_attr *get_sprite_attr(int index) {
	return (struct sprite_attr *)&gb_mem[OAM + index * sizeof(struct sprite_attr)];
}

uint8_t *get_sprite_data(uint8_t index) {
	int size = 16;
	if (get_lcdc()->obj_size) {
		size *= 2;
	}
	return &gb_mem[SPRITE_TILES + index * size];
}
