#include "mem.h"
#include "gpu.h"

static const int PIXEL_TIME = 1;
static const int HDRAW_TIME = 240;
static const int HBLANK_TIME = 68;
static const int SCANLINE_TIME = 308;
static const int VDRAW_TIME = 49280;
static const int VBLANK_TIME = 20944;
static const int REFRESH_TIME = 70224;

static const int OAM_COUNT = 40;
const int SPRITE_X_OFFSET = 8;
const int SPRITE_Y_OFFSET = 16;

int current_time = 0;
int current_line = 0;

void draw_sprite_row(int x, int y, uint8_t row0, uint8_t row1) {
	for (int i = 0; i < 8; i++) {
		uint8_t color = (row0 & 0x01) | ((row1 & 0x01) << 1);
		if (color) {
			draw_pixel(x + i, y);
		}
		row0 = row0 >> 1;
		row1 = row1 >> 1;
	}
}

void draw_scan_line(uint8_t y) {
	uint8_t obj_height = 16;
	if (get_lcdc()->obj_size) {
		obj_height *= 2;
	}
	for (int i = 0; i < 40; i++) {
		struct sprite_attr *sprite_attr = get_sprite_attr(i);
		uint8_t y_start = sprite_attr->y - SPRITE_Y_OFFSET;
		uint8_t x_start = sprite_attr->x - SPRITE_X_OFFSET;
		if (y_start <= y && y_start + obj_height >= y) {
			uint8_t line = y - y_start;
			uint8_t *data = get_sprite_data(sprite_attr->pattern);
			uint8_t row0 = data[line * 2];
			uint8_t row1 = data[line * 2 + 1];
			draw_sprite_row(x_start, y, row0, row1);
		}
	}
}

int gpu_tick() {
	if (!(current_time % SCANLINE_TIME)) {
		// HDRAW
		get_stat()->mode_flag = 10;
		// TODO: mode_flag = 11;
		draw_scan_line(current_line++);
	} else if (!(current_time % (SCANLINE_TIME - HBLANK_TIME))) {
		// HBLANK
		get_stat()->mode_flag = 00;
	} 
	if (!(current_time % VDRAW_TIME) && current_time) {
		// VBLANK
		get_if()->vblank = 1;
		get_stat()->mode_flag = 01;
	}
	if (!(current_time % REFRESH_TIME) && current_time) {
		// END
		display_render();
		get_if()->vblank = 0;
		current_time = -1;
		current_line = 0;
	}
	current_time += PIXEL_TIME;
	return handle_display_events();
}
