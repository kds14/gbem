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
static const int BG_TILE_COUNT = 32;
static const int SPRITE_X_OFFSET = 8;
static const int SPRITE_Y_OFFSET = 16;

int current_time = 0;
uint8_t current_line = 0x0;
int vblank = 0;
uint32_t frame_time = 0;

void draw_sprite_row(int x, int y, uint8_t row0, uint8_t row1) {
	for (int i = 0; i < 8; i++) {
		if (x + i < 0)
			continue;
		uint8_t color = (row0 >> 7) | ((row1 >> 7));
		if (color) {
			draw_pixel(x + i, y);
		}
		row0 = row0 << 1;
		row1 = row1 << 1;
	}
}

void draw_sprites(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->obj_display)
		return;

	uint8_t obj_height = 16;
	if (lcdc->obj_size)
		obj_height *= 2;

	for (int i = 0; i < OAM_COUNT; i++) {
		struct sprite_attr *sprite_attr = get_sprite_attr(i);
		uint8_t y_start = sprite_attr->y - SPRITE_Y_OFFSET;
		uint8_t x_start = sprite_attr->x - SPRITE_X_OFFSET;

		if (y_start <= y && y_start + obj_height >= y) {
			uint8_t line = y - y_start;
			uint8_t *data = get_sprite_data(sprite_attr->pattern, 0);
			uint8_t row0 = data[line * 2];
			uint8_t row1 = data[line * 2 + 1];
			draw_sprite_row(x_start, y, row0, row1);
		}
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
	uint8_t x_offset = scx % 8;
	uint8_t y_start = (y + scy) / 8;
	uint8_t line = (scy + y) % 8;
	for (int i = 0; i < 18; i++) {
		uint8_t tile = i + x_start;
		/*while (tile >= 32) {
			tile -= 32;
		}*/
		uint8_t tile_start_x = i * 8;
		uint16_t tile_addr = tile_map_addr + tile + y_start * 32;
		/*while (tile_addr > BG_MAP_DATA0_END) {
			tile_addr = BG_MAP_DATA0 + tile_addr - BG_MAP_DATA0_END;
		}*/
		uint8_t index = gb_mem[tile_addr];
		uint8_t *data = get_tile_data(index, 16, lcdc->bg_tile_sel);
		uint8_t row0 = data[line * 2];
		uint8_t row1 = data[line * 2 + 1];
		draw_sprite_row(tile_start_x, y, row0, row1);
	}
}

void draw_scan_line(uint8_t y) {
	if (y >= SCREEN_HEIGHT)
		return;
	draw_background(y);
	draw_sprites(y);
}

int gpu_tick() {
	struct lcdc *lcdc = get_lcdc();
	if (lcdc->win_display) {
		return 0;
	}
	if (!(current_time % SCANLINE_TIME)) {
		// HDRAW
		if (!vblank)
			get_stat()->mode_flag = 0x02;
		// TODO: mode_flag = 11;
		draw_scan_line(current_line++);
		set_mem(LY, current_line);
	} else if (!(current_time % (SCANLINE_TIME - HBLANK_TIME))) {
		// HBLANK
		get_stat()->mode_flag = 00;
	} 

	if (!(current_time % VDRAW_TIME) && current_time) {
		// VBLANK
		get_if()->vblank = 1;
		get_stat()->mode_flag = 0x01;
		vblank = 1;
	}

	if (!(current_time % REFRESH_TIME) && current_time) {
		// END
		display_render();
		wait_clear_renderer(&frame_time);
		get_if()->vblank = 0;
		current_time = -1;
		current_line = 0;
		set_mem(LY, current_line);
		get_stat()->mode_flag = 0x02;
		vblank = 0;
	}
	current_time += PIXEL_TIME;
	return handle_display_events();
}
