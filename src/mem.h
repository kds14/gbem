#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Special Registers */
#define SB 0xFF01
#define SC 0xFF02
#define DIV 0xFF04
#define TIMA 0xFF05
#define TMA 0xFF06
#define TAC 0xFF07
#define IF 0xFF0F
#define NR10 0xFF10
#define NR11 0xFF11
#define NR12 0xFF12
#define NR13 0xFF13
#define NR14 0xFF14
#define NR21 0xFF16
#define NR22 0xFF17
#define NR23 0xFF18
#define NR24 0xFF19
#define NR30 0xFF1A
#define NR31 0xFF1B
#define NR32 0xFF1C
#define NR33 0xFF1D
#define NR34 0xFF1E
#define NR41 0xFF20
#define NR42 0xFF21
#define NR43 0xFF22
#define NR44 0xFF23
#define NR50 0xFF24
#define NR51 0xFF25
#define NR52 0xFF26
#define WAVE_PATTERN_RAM 0xFF30
#define LCDC 0xFF40
#define STAT 0xFF41
#define SCY 0xFF42
#define SCX 0xFF43
#define LY 0xFF44
#define LYC 0xFF45
#define DMA 0xFF46
#define BGP 0xFF47
#define OBP0 0xFF48
#define OBP1 0xFF49
#define WY 0xFF4A
#define WX 0xFF4B
#define IE 0xFFFF

/* Memory map sections */
#define ROM_BANK0 0x0000
#define SW16_ROM_BANK 0x4000
#define VIDEO_RAM 0x8000
#define SW8_ROM_BANK 0xA000
#define INTERNAL_RAM0 0xC000
#define ECHO_RAM 0xE000
#define SPRITE_ATTR_MEM 0xFE00
#define IO_PORTS 0xFF00
#define INTERNAL_RAM1 0xFF80

/* Video memory map sections */
#define SPRITE_TILES 0x8000
#define BG_TILES 0x8800
#define BG_MAP_DATA0 0x9800
#define BG_MAP_DATA0_END 0x9BFF
#define BG_MAP_DATA1 0x9C00
#define OAM 0xFE00

/* Distance between ram and echo ram */
#define ECHO_OFFSET 0x2000

/* Interrupt addresses */
#define VBLANK_ADDR 0x0040
#define LCDC_ADDR  0x0048
#define TIMER_OVERFLOW_ADDR  0x0050
#define SERIAL_IO_TRANS_ADDR  0x0058
#define P10_P13_TRANSITION_ADDR  0x0060

struct lcdc
{
	uint8_t bg_win_display : 1; // 0: off, 1: on
	uint8_t obj_display : 1; // 0: off, 1: on
	uint8_t obj_size : 1; // 0: 8x8, 1: 8x16
	uint8_t bg_tile_map : 1; // 0: 0x9800-0x9BFF, 1: 0x9C00-0x9FFF
	uint8_t bg_tile_sel : 1; // 0: 0x8800-0x97FF, 1: 0x8000-0x8FFF
	uint8_t win_display : 1; // 0: off, 1: on
	uint8_t win_tile_map : 1; // 0: 0x9800-0x9BFF, 1: 0x9C00-0x9FFF
	uint8_t lcd_control_op : 1; // 0: stop, 1: op
};

struct sprite_attr
{
	uint8_t y; // y position
	uint8_t x; // x position
	uint8_t pattern; // 0-255 unsigned
	union {
		uint8_t flags;
		struct {
			uint8_t none : 4; // first 4 lsb not used
			uint8_t palette : 1;
			uint8_t xflip : 1;
			uint8_t yflip : 1;
			uint8_t priority : 1;
		};
	};
};

struct stat
{
	uint8_t mode_flag: 2;
	uint8_t coincidence : 1;
	uint8_t mode00 : 1;
	uint8_t mode01 : 1;
	uint8_t mode10 : 1;
	uint8_t lcy_eq_ly_coinc : 1;
	uint8_t none : 1;
};

struct interrupt_flag
{
	uint8_t vblank : 1;
	uint8_t lcdc : 1;
	uint8_t timer_overflow : 1;
	uint8_t serial_io : 1;
	uint8_t p10p13_transfer : 1;
	uint8_t none : 3;
};

struct sprite_attr *get_sprite_attr(int index);

struct lcdc *get_lcdc();

struct stat *get_stat();

struct interrupt_flag *get_if();

uint8_t *get_sprite_data(uint8_t index, int bg);

uint8_t *get_tile_data(uint8_t index, int size, int obj_tiles);

uint8_t *gb_mem;

/* 
 * All memory writing must go through set_mem because
 * there are special rules for some areas of memory
 */
void set_mem(uint16_t dest, uint8_t data);

#endif
