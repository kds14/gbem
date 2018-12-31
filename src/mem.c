#include "mem.h"
#include "display.h"
#include <stdio.h>

static const size_t DMA_SIZE = 0xA0;

void dma(uint8_t addr) {
	uint8_t *dest = &gb_mem[OAM];
	uint16_t src_addr = addr << 8;
	//printf("DMA %04X\n", src_addr);
	uint8_t *src = &gb_mem[src_addr];
	memcpy(dest, src, DMA_SIZE);
}

void set_mem(uint16_t dest, uint8_t data) {
	if (dest < 0x8000) {
		//exit(0);
		return;
	}
	if (dest >= 0x8000 && dest <= 0x9FFF && get_stat()->mode_flag == 0x03)
		return;
	if (dest >= 0xFE00 && dest <= 0xFE9F && get_stat()->mode_flag > 0x01)
		return;
	if (dest == 0xFF02 && data == 0x81) {
		printf("%c", gb_mem[0xFF01]);
	}
	if (dest >= 0x9800 && dest <= 0x9BFF && data != 0x20) {
		//printf("%04X : %02X\n", dest, data);
	}

	// Setting 7th bit in LCDC sets LY = 0
	if (dest == LCDC) {
		uint8_t bit7 = data >> 7;
		uint8_t old_bit7 = gb_mem[LCDC] >> 7;
		if (bit7 != old_bit7) {
			gb_mem[LY] = 0;
		}
		/*if (!old_bit7 && bit7) {
			printf("CLEAR\n");
			clear_renderer();
		} else if (old_bit7 && !bit7) {
			printf("CLEAR\n");
			clear_renderer();
		}*/
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
