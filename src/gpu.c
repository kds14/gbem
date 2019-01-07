#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "mem.h"
#include "gpu.h"
#include "display.h"


#define SPRITE_X_OFFSET 8
#define SPRITE_Y_OFFSET 16

#define PIXEL_TIME 1
#define OAM_READ_TIME 83
#define OAM_VRAM_READ_TIME 169
#define HBLANK_TIME 207
#define SCANLINE_TIME 456
#define REFRESH_TIME 70224
#define VBLANK_LINE 144
#define FINAL_LINE 153

#define OAM_COUNT 40
#define BG_TILE_MAX 32

enum dstate {HBLANK, VBLANK, OAM_READ, OAM_VRAM_READ};
enum dstate dstate = OAM_READ;

int reset = 0;
uint8_t current_line = 0x0;

// tracks LCD status mode timing
struct gt {
	int ort;
	int ovrt;
	int hbt;
	int vbt;
};

struct gt gtt;

/*
 * Draws row based on row0, row1 at x and y with color from pal.
 * Does not draw sprite color 0. Flips sprites if xflip is set.
 * pstart is used to start at a pixel other than 0. prty is the sprite
 * priority flag (0 or 1). sprty is used to decide priority between
 * two sprites (leftmost has priority else OAM ordering is used)
 */
void draw_tile_row(int x, int y, uint8_t row0, uint8_t row1, uint8_t pal, int sprite, int xflip, int pstart, int prty, uint16_t sprty) {
	uint8_t color, c;
	int i;
	if (pstart) {
		if (xflip) {
			row0 = row0 >> pstart;
			row1 = row1 >> pstart;
		} else {
			row0 = row0 << pstart;
			row1 = row1 << pstart;
		}
	}
	for (i = 0; i < 8 - pstart; i++) {
		if (xflip) {
			// colors are 2 bits so 2 rows are combined to get the color
			color = ((row1 << 1) & 0x02) | (row0 & 0x1);
			c = (pal >> (2 * color)) & 0x3;
			row0 = row0 >> 1;
			row1 = row1 >> 1;
		} else {
			color = (((row1 >> 7) << 1) | (row0 >> 7)) & 0x3;
			c = (pal >> (2 * color)) & 0x3;
			row0 = row0 << 1;
			row1 = row1 << 1;
		}
		// sprite color 0 is transparent so do not draw
		if (!sprite || color != 0)
			draw_pixel(x + i, y, c, !sprite, c != (pal & 0x3) , prty, sprty);
	}
}

/*
 * Draws the appropriate sprite lines at the given y.
 * All sprites in the OAM table are checked if they are part of the
 * current y. If they are then their pattern is retrieved from the
 * sprite pattern table at 0x8000. Palette, xflip, and yflip are
 * all retrieved from the OAM table as well.
 */
void draw_sprites(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->obj_display)
		return;

	uint8_t obj_scale = lcdc->obj_size ? 2 : 1;
	uint8_t obj_height = obj_scale * 8;

	for (uint8_t i = 0; i < OAM_COUNT; i++) {
		struct sprite_attr *sprite_attr = get_sprite_attr(i);
		int y_start = sprite_attr->y - SPRITE_Y_OFFSET;
		int x_start = sprite_attr->x - SPRITE_X_OFFSET;

		if (y_start <= y && y_start + obj_height > y) {
			uint8_t line = y - y_start;
			uint8_t *data = get_sprite_data(sprite_attr->pattern, 0);
			if (sprite_attr->yflip)
				line = 8 * obj_scale - 1 - line;

			uint8_t row0 = data[line * 2];
			uint8_t row1 = data[line * 2 + 1];
			uint8_t pal = gb_mem[OBP0];
			if (sprite_attr->palette) {
				pal = gb_mem[OBP1];
			}
			draw_tile_row(x_start, y, row0, row1, pal, 1, sprite_attr->xflip, 0, sprite_attr->priority, ((uint16_t)sprite_attr->x << 8) | i);
		}
	}
}

/*
 * Draws the appropriate window lines at the given y.
 * Window tile indexes and patterns are taken from the appropriate
 * area in memory as set by the LCDC pattern. The window is then drawn
 * with its top left where the WY and WX registers indicate. The window
 * will overlay the background hiding it completely. This is often used
 * for menu windows or UI that overlays the game.
 */
void draw_window(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->bg_win_display || !lcdc->win_display) {
		return;
	}

	// tile map address
	uint16_t tm_addr = BG_MAP_DATA0;
	if (lcdc->win_tile_map)
		tm_addr = BG_MAP_DATA1;

	uint8_t wy = gb_mem[WY];
	uint8_t wx = gb_mem[WX];
	if (y < wy)
		return;

	for (int i = 0; i < BG_TILE_MAX; i++) {
		uint8_t tile = i;
		// WX = Window X Position - 7
		uint8_t tile_start_x = tile * 8 + (wx - 7);
		if (tile_start_x < wx - 8 || tile_start_x > SCREEN_WIDTH)
			continue;
		uint8_t y_start = (y - wy) / 8;
		uint8_t line = (y - wy) % 8;
		uint16_t tile_addr = tm_addr + tile + y_start * BG_TILE_MAX;
		uint8_t index = gb_mem[tile_addr];
		uint8_t *data = get_tile_data(index, 16, lcdc->bg_tile_sel);
		uint8_t row0 = data[line * 2];
		uint8_t row1 = data[line * 2 + 1];
		draw_tile_row(tile_start_x, y, row0, row1, gb_mem[BGP], 0, 0, 0, 0, NO_PRIORITY);
	}
}

/*
 * Draws the appropriate background lines at the given y.
 * The background is scrolled according to the SCY and SCX registers.
 * The tile map and patterns are taken from the appropriate areas based
 * on the LCDC register. The background will also wrap around if it goes
 * off the screen.
 */
void draw_background(uint8_t y) {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->bg_win_display) {
		return;
	}

	// tile map address
	uint16_t tm_addr = BG_MAP_DATA0;
	if (lcdc->bg_tile_map)
		tm_addr = BG_MAP_DATA1;

	uint8_t scy = gb_mem[SCY];
	uint8_t scx = gb_mem[SCX];

	// important to cast to uint8_t to assure it wraps around the screen
	uint8_t x_off = scx % 8;
	uint8_t y_start = (uint8_t)(y + scy) / 8;
	uint8_t line = (uint8_t)(scy + y) % 8;
	for (int i = 0; i < BG_TILE_MAX; i++) {
		uint8_t tile = (uint8_t)(i * 8 + scx) / 8;
		uint8_t offset = i ? x_off : 0;
		uint8_t tile_start_x = i * 8 - offset;
		int pstart = !i ? x_off : 0;
		uint16_t tile_addr = tm_addr + tile + y_start * BG_TILE_MAX;
		uint8_t index = gb_mem[tile_addr];
		uint8_t *data = get_tile_data(index, 16, lcdc->bg_tile_sel);
		uint8_t row0 = data[line * 2];
		uint8_t row1 = data[line * 2 + 1];
		draw_tile_row(tile_start_x, y, row0, row1, gb_mem[BGP], 0, 0, pstart, 0, NO_PRIORITY);
	}
}

void draw_scan_line(uint8_t y) {
	if (y >= SCREEN_HEIGHT)
		return;
	draw_background(y);
	draw_window(y);
	draw_sprites(y);
}

/*
 * Advances the GPU one tick. Rotates between various LCD status modes.
 * Each scanline has a period of OAM_READ, OAM_VRAM_READ * and HBLANK.
 * After all the scanlines are drawn there is a period of VBLANK.
 * Each limits CPU access to OAM and/or VRAM and sets the STAT
 * register. At the end of each scanline, it draws the appropriate line.
 */
int gpu_tick() {
	struct lcdc *lcdc = get_lcdc();
	if (!lcdc->lcd_control_op) {
		set_stat_mode(VBLANK);
		dstate = VBLANK;
		set_ly(0);
		reset = 1;
		return 0;
	}
	current_line = gb_mem[LY];
	if (!current_line && reset) {
		reset = 0;
		set_stat_mode(OAM_READ);
		dstate = OAM_READ;
		memset(&gtt, 0, sizeof(gtt));
	}

	switch (dstate) {
		case OAM_READ:
			if (++gtt.ort >= OAM_READ_TIME) {
				set_stat_mode(OAM_VRAM_READ);
				dstate = OAM_VRAM_READ;
				gtt.ovrt = 0;
			} 
			break;
		case OAM_VRAM_READ:
			if (++gtt.ovrt >= OAM_VRAM_READ_TIME) {
				set_stat_mode(HBLANK);
				dstate = HBLANK;
				gtt.hbt = 0;
			}
			break;
		case HBLANK:
			if (!(++gtt.hbt % HBLANK_TIME)) {
				draw_scan_line(current_line++);
				set_ly(current_line);
				set_stat_mode(OAM_READ);
				dstate = OAM_READ;
				gtt.ort = 0;
			}
			if (current_line >= VBLANK_LINE) {
				// VBLANK
				get_if()->vblank = 1;
				set_stat_mode(VBLANK);
				dstate = VBLANK;
				ready_render();
				gtt.vbt = 0;
			}
			break;
		case VBLANK:
			if (!(++gtt.vbt % SCANLINE_TIME)) {
				// LY still increments during VBLANK
				set_ly(current_line + 1);
			}
			// TODO: ends at 153 or 154?
			if (current_line > FINAL_LINE) {
				// END
				display_render();
				reset = 1;
				set_ly(0);
				set_stat_mode(OAM_READ);
				dstate = OAM_READ;
				gtt.ort = 0;
			}
			break;
	}
	return 0;
}
