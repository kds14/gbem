#include <stdio.h>

#include "mem.h"
#include "display.h"
#include "input.h"

static const size_t DMA_SIZE = 0xA0;

void dma(uint8_t addr) {
	uint8_t *dest = &gb_mem[OAM];
	uint16_t src_addr = addr << 8;
	uint8_t *src = &gb_mem[src_addr];
	memcpy(dest, src, DMA_SIZE);
}

void set_mem(uint16_t dest, uint8_t data) {
	// cannot write to ROM
	if (dest < 0x8000) {
		return;
	}

	if (dest >= 0x8000 && dest <= 0x9FFF && get_stat()->mode_flag == 0x03)
		return;
	if (dest >= 0xFE00 && dest <= 0xFE9F && get_stat()->mode_flag > 0x01)
		return;

	if (dest == 0xFF00) {
		uint8_t p15 = data & 0x20;
		uint8_t p14 = data & 0x10;
		if (!p14 || !p15) {
			int f = p15 == 0;
			uint8_t res = request_input(f);
			uint8_t r = f ? (data & 0xF0) | res : (data & 0xF0) | res;
			gb_mem[0xFF00] = r;
			return;
		}
	}

	// Setting 7th bit in LCDC sets LY = 0
	if (dest == LCDC) {
		uint8_t bit7 = data >> 7;
		uint8_t old_bit7 = gb_mem[LCDC] >> 7;
		if (bit7 != old_bit7) {
			gb_mem[LY] = 0;
		}
	}

	gb_mem[dest] = data;
	if (dest >= INTERNAL_RAM0 && dest <= 0xDDFF) {
		gb_mem[dest + ECHO_OFFSET] = data;
	} else if (dest >= ECHO_RAM && dest <= 0xFDFF) {
		gb_mem[dest - ECHO_OFFSET] = data;
	}

	if (dest == DMA && data <= 0xF1) {
		dma(data);
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
	return (struct sprite_attr *)&gb_mem[OAM + index * 4];
}

uint8_t *get_sprite_data(uint8_t index, int bg) {
	int size = 16;
	if (get_lcdc()->obj_size && !bg) {
		size *= 2;
	}
	return &gb_mem[SPRITE_TILES + (uint8_t)index * size];
}

uint8_t *get_tile_data(uint8_t index, int size, int bg_tile_sel) {
	if (bg_tile_sel) {
		return &gb_mem[SPRITE_TILES + (uint8_t)index * size];
	} else {
		return &gb_mem[BG_TILES + 0x800 + (int8_t)index * size];
	}
}
