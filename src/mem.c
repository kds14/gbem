#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include "mem.h"
#include "display.h"
#include "input.h"

#define DMA_SIZE 0xA0

enum mbc1_mod {ROM_MODE = 0, RAM_MODE = 1};
enum mb_ctrl {NO_MBC, MBC1, MBC2, MBC3, MBC4, MBC5};
/*
 * Memory banking variables 
 */
struct mb_data {
	uint8_t rom_idx; // current ROM bank index
	uint8_t ram_idx; // current ROM bank index
	uint8_t ram_rw; // cannot read or write from ext RAM without this set
	uint16_t ram_size;
	uint8_t ram_count;
	uint8_t rom_count;
	enum mbc1_mod mode; 
	enum mb_ctrl mbc;
	int rtc;
	uint8_t *rtc_bank;
	uint8_t **rom_banks;
	uint8_t **ram_banks;
};

struct mb_data mbd;

time_t last_rtc = 0;

void latch_rtc() {
}

uint8_t *get_mem_ptr(uint16_t addr) {
	// if accessing ROM bank memory, select from appropriate bank
	if (mbd.mbc != NO_MBC && addr >= 0x4000 && addr < 0x8000
		&& mbd.rom_idx < mbd.rom_count) {
		return &mbd.rom_banks[mbd.rom_idx][addr - 0x4000];
	}
	if (addr >= 0xA000 && addr < 0xC000 && mbd.ram_rw) {
		if (mbd.rtc) {
		} else if (mbd.ram_count && (addr < 0xA800 || mbd.ram_size != 0x800)
			&& mbd.ram_idx < mbd.ram_count) {
			return &mbd.ram_banks[mbd.ram_idx][addr - 0xA000];
		}
	}
	return &gb_mem[addr];
}

uint8_t get_mem(uint16_t addr) {
	return *get_mem_ptr(addr);
}

void dma(uint8_t addr) {
	uint8_t *dest = &gb_mem[OAM];
	uint16_t src_addr = addr << 8;
	uint8_t *src = get_mem_ptr(src_addr);
	memcpy(dest, src, DMA_SIZE);
}

void mbc3_set_mem(uint16_t dest, uint8_t data) {
	// TODO only RAM bank 00 can be used during mode 0
	if (dest >= 0x000 && dest < 0x2000)
		mbd.ram_rw = (data & 0x0A) ? 1 : 0;
	else if (dest >= 0x6000 && dest < 0x8000) {
		if (mbd.rtc == 1 && !data) {
			mbd.rtc = 2;
		} else if (mbd.rtc == 2 && data == 1) {
			mbd.rtc = 1;
		}
	}
	else if (dest >= 0x2000 && dest < 0x4000) {
		mbd.rom_idx = data & 0x7F;
	}
	else if (dest >= 0x4000 && dest < 0x6000) {
		if (data >= 0x08 && data <= 0x0C) {
			mbd.rtc = 1;
		} else {
			if (data <= 0x3)
				mbd.ram_idx = data & 0x3;
			mbd.rtc = 0;
		}
	}
}

void mbc1_set_mem(uint16_t dest, uint8_t data) {
	// TODO only RAM bank 00 can be used during mode 0
	if (dest >= 0x000 && dest < 0x2000)
		mbd.ram_rw = (data & 0x0A) ? 1 : 0;
	else if (dest >= 0x6000 && dest < 0x8000) {
		mbd.mode = data & 0x1;
	}
	else if (dest >= 0x2000 && dest < 0x4000) {
		mbd.rom_idx = data & 0x1F;
		if (mbd.rom_idx == 0x20)
			mbd.rom_idx = 0x21;	
		else if (mbd.rom_idx == 0x40)
			mbd.rom_idx = 0x41;	
		else if (mbd.rom_idx == 0x60) {
			mbd.rom_idx = 0x61;
		}	
		if (mbd.rom_idx >= mbd.rom_count)
			printf("%04X vs %04X\n", mbd.rom_idx, mbd.rom_count);
	}
	else if (dest >= 0x4000 && dest < 0x6000) {
		if (mbd.mode == ROM_MODE) {
			uint8_t pi = mbd.rom_idx;
			mbd.rom_idx |= (data & 0x3) << 5;
			if (mbd.rom_idx == 0x20)
				mbd.rom_idx = 0x21;	
			else if (mbd.rom_idx == 0x40)
				mbd.rom_idx = 0x41;	
			else if (mbd.rom_idx == 0x60)
				mbd.rom_idx = 0x61;	
			if (mbd.rom_idx >= mbd.rom_count) {
				printf("B: %04X apply %04X\n", pi, data);
				printf("%04X vs %04X\n", mbd.rom_idx, mbd.rom_count);
			}
		} else {
			mbd.ram_idx = data & 0x3;
			if (mbd.ram_idx >= mbd.ram_count)
				printf("%04X vs %04X\n", mbd.ram_idx, mbd.ram_count);
		}
	}
}

void set_mem(uint16_t dest, uint8_t data) {
	// Writes to ROM are instead used to set MBC based options
	if (dest < 0x8000) {
		if (mbd.mbc == MBC1)
			mbc1_set_mem(dest, data);
		else if (mbd.mbc == MBC3)
			mbc3_set_mem(dest, data);
	}

	if (dest >= 0xA000 && dest < 0xC000) {
		if (mbd.ram_count && (dest < 0xA800 || mbd.ram_size != 0x800)
				&& mbd.ram_rw && mbd.ram_idx < mbd.ram_count)
			mbd.ram_banks[mbd.ram_idx][dest - 0xA000] = data;
		return;
	}

	// STAT register limits access to OAM and VRAM based on the LCD mode
	struct statr *stat = get_stat();
	struct lcdc *lcdc = get_lcdc();
	if (dest >= 0x8000 && dest <= 0x9FFF && stat->mode_flag == 0x03 && lcdc->lcd_control_op)
		return;
	if (dest >= 0xFE00 && dest <= 0xFE9F && stat->mode_flag > 0x01 && lcdc->lcd_control_op)
		return;

	// writing to 0xFF00 requests button input info
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
	// TODO: this may actually be false despite docs
	if (dest == LCDC) {
		uint8_t bit7 = data >> 7;
		uint8_t old_bit7 = gb_mem[LCDC] >> 7;
		if (bit7 != old_bit7) {
			gb_mem[LY] = 0;
		}
	}

	if (dest == LY)
		data = 0;

	gb_mem[dest] = data;

	// writes to 0xC000-0xDDFF are mirrored at 0xE000-0xFE00 and vice versa
	if (dest >= INTERNAL_RAM0 && dest <= 0xDDFF) {
		gb_mem[dest + ECHO_OFFSET] = data;
	} else if (dest >= ECHO_RAM && dest <= 0xFDFF) {
		gb_mem[dest - ECHO_OFFSET] = data;
	}

	// writes to FF46 initiate a DMA transfer at the given start address
	if (dest == DMA && data <= 0xF1) {
		dma(data);
	}
	// TODO: look at specifics of some special registers
}

char* sav_file_name = NULL;

void save_ram() {
	if (mbd.ram_count == 0)
		return;
	int i;
	if (sav_file_name == NULL) {
		fprintf(stderr, "Unable to save file\n");
		return;
	}
	FILE* fp = fopen(sav_file_name, "w+");
	if (!fp) {
		fprintf(stderr, "Unable to save file %s\n", sav_file_name);
		return;
	}
	for (i = 0; i < mbd.ram_count; ++i) {	
		fwrite(mbd.ram_banks[i], mbd.ram_size, 1, fp);
	}
	fclose(fp);
}

void load_ram(char* file_name) {
	if (mbd.ram_count == 0)
		return;
	int i;
	sav_file_name = calloc(strlen(file_name) + 5, sizeof(char));
	strcpy(sav_file_name, file_name);
	strcat(sav_file_name, ".sav");
	FILE* fp = fopen(sav_file_name, "r");
	if (!fp) {
		fprintf(stderr, "Unable to load file %s\n", sav_file_name);
		return;
	}
	for (i = 0; i < mbd.ram_count; ++i) {	
		fread(mbd.ram_banks[i], mbd.ram_size, 1, fp);
	}
	fclose(fp);
}

/*
 * Sets up memory banks based on cartridge header.
 * 0x0147: cartridge type (ROM only, MBC1, MBC2, etc)
 * 0x0148: rom size aka number of ROM banks (val isn't the number)
 */
void setup_mem_banks(uint8_t *cart_mem, char* name) {
	memset(&mbd, 0, sizeof(mbd));
	uint8_t ct = cart_mem[0x0147];

	int i, rom_bank_count = 2;
	mbd.mbc = NO_MBC;

	switch (cart_mem[0x0147]) {
		case 0x01:
		case 0x02:
		case 0x03:
			mbd.mbc = MBC1;
			break;
		case 0x05:
		case 0x06:
			mbd.mbc = MBC2;
			break;
		case 0x0F:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			mbd.mbc = MBC3;
			break;
		case 0x15:
		case 0x16:
		case 0x17:
			mbd.mbc = MBC4;
			break;
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
			mbd.mbc = MBC5;
			break;
		default:
			mbd.mbc = NO_MBC;
	}

	if (!ct)
		return;

	mbd.ram_count = cart_mem[0x0149];

	mbd.ram_size = 0x2000;
	if (mbd.ram_count == 0x01) {
		mbd.ram_size = 0x800;
	} else if (mbd.ram_count == 0x02) {
		mbd.ram_count = 1;
	} else if (mbd.ram_count == 0x03) {
		mbd.ram_count = 4;
	}

	mbd.ram_banks = calloc(mbd.ram_count, sizeof(uint8_t*));
	for (i = 0; i < mbd.ram_count; ++i) {	
		mbd.ram_banks[i] = calloc(mbd.ram_size, sizeof(uint8_t));
	}
	load_ram(name);
	atexit(save_ram);

	uint8_t rom_size = cart_mem[0x0148];
	if (rom_size <= 7)
		rom_bank_count = 0x1 << (rom_size + 1);
	else if (rom_size == 0x52)
		rom_bank_count = 72;
	else if (rom_size == 0x53)
		rom_bank_count = 72;
	else if (rom_size == 0x54)
		rom_bank_count = 96;

	mbd.rom_banks = calloc(rom_bank_count, sizeof(uint8_t*));
	uint32_t rom_ptr;
	mbd.rom_banks[0] = &cart_mem[0x4000];
	for (i = 1; i < rom_bank_count; ++i) {
		rom_ptr = 0x4000 * i;
		mbd.rom_banks[i] = &cart_mem[rom_ptr];
	}
	mbd.rom_count = rom_bank_count;
}

struct lcdc *get_lcdc() {
	return (struct lcdc *)&gb_mem[LCDC];
}

struct statr *get_stat() {
	return (struct statr *)&gb_mem[STAT];
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
		index &= 0xFE;
	}
	return &gb_mem[SPRITE_TILES + (uint8_t)index * size];
}

uint8_t *get_tile_data(uint8_t index, int size, int bg_tile_sel) {
	if (bg_tile_sel) {
		return &gb_mem[SPRITE_TILES + (uint8_t)index * size];
	} else {
		return &gb_mem[0x9000 + (int8_t)index * size];
	}
}

/*
 * Set's the stat mode and starts an interrupt if the appropriate
 * STAT flag is set.
 */
void set_stat_mode(uint8_t mode) {
	int no_change = (gb_mem[STAT] & 0x03) == mode;
	if (no_change)
		return;
	gb_mem[STAT] = (gb_mem[STAT] & 0xFC) | mode;
	if (gb_mem[STAT] & 0x8 && mode == 0) {
		gb_mem[IF] |= 0x2;
	} else if (gb_mem[STAT] & 0x10 && mode == 0x01) {
		gb_mem[IF] |= 0x2;
	} else if (gb_mem[STAT] & 0x20 && mode == 0x10) {
		gb_mem[IF] |= 0x2;
	}
}

void set_ly(uint8_t val) {
	gb_mem[LY] = val;
	// set LYC=LY coincidence flag which may cause an interrupt based on STAT
	if (val == gb_mem[LYC]) {
		gb_mem[STAT] |= 0x4;
		if (gb_mem[STAT] & 0x40) {
			gb_mem[IF] |= 0x2;
		}
	} else {
		gb_mem[STAT] &= 0xFB;
	}
}
