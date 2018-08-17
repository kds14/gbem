#ifndef MEM_H
#define MEM_H

#include <stdint.h>

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

/* Distance between ram and echo ram */
#define ECHO_OFFSET 0x2000

uint8_t *gb_mem;

/* 
 * All memory writing must go through set_mem because
 * there are special rules for some areas of memory
 */
void set_mem(uint16_t dest, uint8_t data);

#endif
