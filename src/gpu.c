#include "mem.h"
#include "gpu.h"
#include "display.h"
#include "input.h"

static const int PIXEL_TIME = 1;
//static const int HDRAW_TIME = 240;
static const int HBLANK_TIME = 200;
static const int SCANLINE_TIME = 458;
static const int VDRAW_TIME = 65952;
//static const int VBLANK_TIME = 4580;
static const int REFRESH_TIME = 70224;

static const int OAM_COUNT = 40;
//static const int BG_TILE_COUNT = 32;
static const int SPRITE_X_OFFSET = 8;
static const int SPRITE_Y_OFFSET = 16;

int current_time = 0;
uint8_t current_line = 0x0;
int vblank = 0;

void draw_sprite_row(int x, int y, uint8_t row0, uint8_t row1, uint8_t pal, int sprite, int xflip) {
	uint8_t color, c;
	for (int i = 0; i < 8; i++) {
		if (x + i < 0)
			continue;
		if (xflip) {
			color = ((row1 << 1) & 0x02) | (row0 & 0x1);
			c = (pal >> (2 * color)) & 0x3;
		} else {
			color = ((row1 >> 7) << 1) | (row0 >> 7);
			c = (pal >> (2 * color)) & 0x3;
		}
		if (!sprite || color != 0)
			draw_pixel(x + i, y, c);
		if (xflip) {
			row0 = row0 >> 1;
			row1 = row1 >> 1;
		} else {
			row0 = row0 << 1;
			row1 = row1 << 1;
		}
	}
}

void y_flip_line(uint8_t line, uint8_t *data) {
}

void draw_sprites(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->obj_display)
		return;

	uint8_t obj_height = 8;
	if (lcdc->obj_size)
		obj_height *= 2;

	for (int i = 0; i < OAM_COUNT; i++) {
		struct sprite_attr *sprite_attr = get_sprite_attr(i);
		uint8_t y_start = sprite_attr->y - SPRITE_Y_OFFSET;
		uint8_t x_start = sprite_attr->x - SPRITE_X_OFFSET;
		// TODO: flipping when height is 16 bits

		if (y_start <= y && y_start + obj_height > y) {
			uint8_t line = y - y_start;
			uint8_t *data = get_sprite_data(sprite_attr->pattern, 0);
			if (sprite_attr->yflip)
				line = 15 - line;
			uint8_t row0 = data[line * 2];
			uint8_t row1 = data[line * 2 + 1];
			uint8_t pal = gb_mem[OBP0];
			if (sprite_attr->palette) {
				pal = gb_mem[OBP1];
			}
			draw_sprite_row(x_start, y, row0, row1, pal, 1, sprite_attr->xflip);
		}
	}
}

void draw_window(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->bg_win_display || !lcdc->win_display) {
		return;
	}

	// tile map address
	uint16_t tile_map_addr = BG_MAP_DATA0;
	if (lcdc->win_tile_map)
		tile_map_addr = BG_MAP_DATA1;

	//TODO: WX AND WY regs
	uint8_t wy = gb_mem[WY];
	uint8_t wx = gb_mem[WX];
	//printf("%d %d\n", wy, wx);

	uint8_t y_start = (y - wy) / 8;
	uint8_t line = (y - wy) % 8;
	for (int i = 0; i < 20; i++) {
		uint8_t tile = i * 8;
		uint8_t tile_start_x = tile + wx - 7;
		uint16_t tile_addr = tile_map_addr + tile + y_start * 32;
		uint8_t index = gb_mem[tile_addr];
		uint8_t *data = get_tile_data(index, 16, lcdc->bg_tile_sel);
		uint8_t row0 = data[line * 2];
		uint8_t row1 = data[line * 2 + 1];
		draw_sprite_row(tile_start_x, y, row0, row1, gb_mem[BGP], 0, 0);
	}
}

void draw_background(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->bg_win_display) {
		return;
	}

	// tile map address
	uint16_t tile_map_addr = BG_MAP_DATA0;
	if (lcdc->bg_tile_map)
		tile_map_addr = BG_MAP_DATA1;

	uint8_t scy = gb_mem[SCY];
	uint8_t scx = gb_mem[SCX];

	uint8_t x_start = scx / 8;
	uint8_t y_start = (uint8_t)(y + scy) / 8;
	uint8_t line = (scy + y) % 8;
	for (int i = 0; i < 20; i++) {
		uint8_t tile = i + x_start;
		uint8_t tile_start_x = i * 8;
		uint16_t tile_addr = tile_map_addr + tile + y_start * 32;
		uint8_t index = gb_mem[tile_addr];
		uint8_t *data = get_tile_data(index, 16, lcdc->bg_tile_sel);
		uint8_t row0 = data[line * 2];
		uint8_t row1 = data[line * 2 + 1];
		draw_sprite_row(tile_start_x, y, row0, row1, gb_mem[BGP], 0, 0);
	}
}

void draw_scan_line(uint8_t y) {
	if (y >= SCREEN_HEIGHT)
		return;
	draw_background(y);
	//draw_window(y);
	draw_sprites(y);
}

int gpu_tick() {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->lcd_control_op) {
		set_stat_mode(0x0);
		current_time = 0;
		current_line = 0;
		vblank = 0;
		return 0;
	}
	if (!(current_time % SCANLINE_TIME)) {
		// HDRAW
		if (!vblank)
			set_stat_mode(0x02);
		draw_scan_line(current_line++);
		set_ly(current_line);
	} else if (!(current_time % (SCANLINE_TIME - HBLANK_TIME))) {
		// HBLANK
		if (!vblank)
			set_stat_mode(0x0);
	} 

	if (!(current_time % VDRAW_TIME) && current_time) {
		// VBLANK
		get_if()->vblank = 1;
		set_stat_mode(0x01);
		vblank = 1;
	}
	if (!(current_time % REFRESH_TIME) && current_time) {
		// END
		display_render();
		on_frame_end();
		current_time = -1;
		current_line = 0;
		set_ly(current_line);
		set_stat_mode(0x02);
		vblank = 0;
	}
	current_time += PIXEL_TIME;
	return 0;
}
