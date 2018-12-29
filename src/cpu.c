#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>
#include <time.h>
#include <signal.h>

#include "mem.h"
#include "debug.h"
#include "display.h"
#include "gpu.h"

//static const int CLOCK_SPEED = 4195304;
static const int MAX_CYCLES_PER_FRAME = 70224;

struct gb_state *gbs = NULL;

/*
 * Registers:
 * A F (Z, N, H, C flag bits)
 * B C
 * D E
 * H L
 * SP
 * PC
 */
struct gb_state
{
	union {
		uint16_t af;
		struct {
			union {
				uint8_t f;
				struct {
					uint8_t fl : 4; // 4 least sig bits not used
					uint8_t fc : 1; // (C) Carry
					uint8_t fh : 1; // (H) Half Carry
					uint8_t fn : 1; // (N) Subtract
					uint8_t fz : 1; // (Z) Zero
				};
			};
			uint8_t a;
		};
	};
	union {
		uint16_t bc;
		struct {
			uint8_t c;
			uint8_t b;
		};
	};
	union {
		uint16_t de;
		struct {
			uint8_t e;
			uint8_t d;
		};
	};
	union {
		uint16_t hl;
		struct {
			uint8_t l;
			uint8_t h;
		};
	};
	uint16_t sp;
	uint16_t pc;
	uint16_t ime;
	uint8_t halt;
	uint8_t di;
	uint8_t ei_flag;
	uint8_t *mem;

};

void daa(struct gb_state *state) {
	int res = state->a;
	if (state->fn) {
		if (state->fh) {
			res -= 0x06;
			if (!state->fc)
				res &= 0xFF;
		}
		if (state->fc) {
			res -= 0x60;
		}
	}
	else {
		if ((res & 0x0F) > 0x9 || state->fh) {
			res += 0x06;
		}
		if (res > 0x9F || state->fc) {
			res += 0x60;
		}
	}
	state->fc |= res > 0xFF;
	state->fh = 0;
	state->a = res & 0xFF;
	state->fz = !state->a;
}

void set_add16_flags(struct gb_state *state, uint16_t a, uint16_t b) {
	state->fn = 0;
	state->fh = (((a & 0x0FFF) + (b & 0x0FFF)) & 0x1000) == 0x1000;
	state->fc = ((((uint32_t)a & 0x0000FFFF) + ((uint32_t)b & 0x0000FFFF)) & 0x10000) == 0x10000;
}

void set_add8_flags(struct gb_state *state, uint8_t a, uint8_t b, int use_carry) {
	state->fn = 0;
	state->fz = !(uint8_t)(a + b);
	state->fh = (((a & 0xF) + (b & 0xF)) & 0x10) == 0x10;
	if (use_carry) {
		state->fc = ((((uint16_t)a & 0xFF) + ((uint16_t)b & 0xFF)) & 0x100) == 0x100;
	}
}

/* a - b */
void set_sub8_flags(struct gb_state *state, uint8_t a, uint8_t b, int use_carry) {
	state->fn = 1;
	state->fz = a == b;
	state->fh = ((a - b) & 0xF) > (a & 0xF);
	if (use_carry) {
		state->fc = a < b;
	}
}

/*
 * Loads a literal value into an 8bit reg. Such as LD C,n.
 */
int load8val2reg(struct gb_state *state, uint8_t *reg, uint8_t val) {
	state->pc++;
	*reg = val;
	return 8;
}

void addA(struct gb_state *state, uint8_t val) {
	set_add8_flags(state, state->a, val, 1);
	state->a += val;
}

void adc(struct gb_state *state, uint8_t val) {
	uint16_t res = state->a + val + state->fc;
	state->fn = 0;
	state->fz = !(res & 0xFF);
	state->fh = (((state->a & 0xF) + (val & 0xF) + state->fc) & 0x10) == 0x10;
	state->fc = (res & 0x100) == 0x100;
	state->a = res & 0xFF;
}

void subc(struct gb_state *state, uint8_t val) {
	uint8_t tmp = state->a - state->fc;
	state->fh = ((state->a - state->fc) & 0xF) > (state->a & 0xF);
	state->fc = state->a < state->fc;
	uint8_t res = tmp - val;
	state->fh |= ((tmp - val) & 0xF) > (tmp & 0xF);
	state->fc |= tmp < val;
	state->fn = 1;
	state->fz = !res;
	state->a = res;
}

void subA(struct gb_state *state, uint8_t val) {
	set_sub8_flags(state, state->a, val, 1);
	state->a -= val;
}

void andA(struct gb_state *state, uint8_t val) {
	state->a &= val;
	state->fz = !state->a;
	state->fn = 0;
	state->fh = 1;
	state->fc = 0;
}

void xorA(struct gb_state *state, uint8_t val) {
	state->a ^= val;
	state->fz = !state->a;
	state->fn = 0;
	state->fh = 0;
	state->fc = 0;
}

void orA(struct gb_state *state, uint8_t val) {
	state->a |= val;
	state->fz = !state->a;
	state->fn = 0;
	state->fh = 0;
	state->fc = 0;
}

void cpA(struct gb_state *state, uint8_t val) {
	set_sub8_flags(state, state->a, val, 1);
}

void pop(struct gb_state *state, uint16_t *dest) {
	//memcpy(dest, &state->mem[state->sp], 2);
	uint8_t *ptr = (uint8_t*)dest;
	ptr[0] = state->mem[state->sp];
	ptr[1] = state->mem[state->sp + 1];
	state->sp += 2;
}

void ret(struct gb_state *state, uint8_t condition) {
	if (condition) {
		pop(state, &state->pc);
	}
}

void jump(struct gb_state *state, uint8_t condition, uint16_t dest) {
	if (condition) {
		state->pc = dest;
	}
}

void call(struct gb_state *state, uint8_t condition, uint16_t addr) {
	if (condition) {
		state->sp -= 2;
		memcpy(&state->mem[state->sp], &state->pc, 2);
		state->pc = addr;
	}
}

void push(struct gb_state *state, uint16_t val) {
	state->sp -= 2;
	memcpy(&state->mem[state->sp], (uint8_t *)&val, 2);
}

void rst(struct gb_state *state, uint16_t val) {
	push(state, state->pc);
	jump(state, 1, val);
}

void rot_right(struct gb_state *state, uint8_t *reg) {
	uint8_t bit7 = state->fc << 7;
	uint8_t val = *reg;
	state->fc = val & 0x01;
	*reg = bit7 | val >> 1;
	state->fn = 0;
	state->fz = *reg == 0;
	state->fh = 0;
}

void rot_right_carry(struct gb_state *state, uint8_t *reg) {
	uint8_t val = *reg;
	state->fc = val & 0x01;
	*reg = (state->fc << 7) | (val >> 1);
	state->fz = *reg == 0;
	state->fn = 0;
	state->fh = 0;
}

void rot_left_carry(struct gb_state *state, uint8_t *reg) {
	uint8_t val = *reg;
	state->fc = val >> 7;
	*reg = (val << 1) | state->fc;
	state->fz = *reg == 0;
	state->fn = 0;
	state->fh = 0;
}

void rot_left(struct gb_state *state, uint8_t *reg) {
	uint8_t val = *reg;
	uint8_t bit0 = state->fc & 0x01;
	state->fc = val >> 7;
	*reg = (val << 1) | bit0;
	state->fn = 0;
	state->fz = *reg == 0;
	state->fh = 0;
}

void sra(struct gb_state *state, uint8_t *reg) {
	uint8_t msb = *reg & 0x80;
	rot_right(state, reg);
	*reg = msb | (0x7F & *reg);
	state->fz = *reg == 0;
}

void srl(struct gb_state *state, uint8_t *reg) {
	uint8_t val = *reg;
	state->fc = val * 0x1;
	*reg = val >> 1;
	state->fn = 0;
	state->fh = 0;
	state->fz = *reg == 0;
}

void swap(struct gb_state *state, uint8_t *reg) {
	uint8_t val = *reg;
	*reg =  val << 4 | val >> 4;
	state->fz = *reg == 0;
	state->fn = 0;
	state->fc = 0;
	state->fh = 0;
}

void bit(struct gb_state *state, uint8_t bit, uint8_t *reg) {
	state->fz = !((*reg >> bit) & 0x01);
	state->fn = 0;
	state->fh = 1;
}

void res(struct gb_state *state, uint8_t bit, uint8_t *reg) {
	*reg &= ~(0x01 << bit);
}

void set(struct gb_state *state, uint8_t bit, uint8_t *reg) {
	*reg |= (0x01 << bit);
}

void print_registers(struct gb_state *state) {
	printf("A: %02X, B: %02X, C: %02X, D: %02X, E: %02X, F: %02X, H: %02X, L: %02X, SP: %04X, PC: %04X\n", state->a, state->b, state->c, state->d, state->e, state->f, state->h, state->l, state->sp, state->pc);
	printf("FLAGS: Z:%d N:%d H:%d C:%d None:%d\n", state->fz, state->fn, state->fh, state->fc, state->fl );
	/*printf("SPECIAL REGISTERS:\n");
	  printf("SPECIAL", state->a, state->b, state->c, state->d, state->e, state->f, state->h, state->l, state->sp, state->pc);*/
}

void handle_debug(int start_pc, int pc, uint8_t* op, int cycles, int cb) {
	if (debug_enabled) {
		uint16_t extra = 0;
		uint8_t extra_flag = 0;
		add_debug(start_pc, *op, cycles, extra, extra_flag, cb);
	}
}

int execute_cb(struct gb_state *state) {
	int pc = state->pc;
	uint8_t *op = &state->mem[pc];
	int cycles = 8;
	int pc_start = state->pc++;
	switch (*op) {
		case 0x00:
			/* RLC B */
			rot_left_carry(state, &state->b);
			break;
		case 0x01:
			/* RLC C */
			rot_left_carry(state, &state->c);
			break;
		case 0x02:
			/* RLC D */
			rot_left_carry(state, &state->d);
			break;
		case 0x03:
			/* RLC E */
			rot_left_carry(state, &state->e);
			break;
		case 0x04:
			/* RLC H */
			rot_left_carry(state, &state->h);
			break;
		case 0x05:
			/* RLC L */
			rot_left_carry(state, &state->l);
			break;
		case 0x06:
			/* RLC (HL) */
			rot_left_carry(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x07:
			/* RLC A */
			rot_left_carry(state, &state->a);
			break;
		case 0x08:
			/* RRC B */
			rot_right_carry(state, &state->b);
			break;
		case 0x09:
			/* RRC C */
			rot_right_carry(state, &state->c);
			break;
		case 0x0A:
			/* RRC D */
			rot_right_carry(state, &state->d);
			break;
		case 0x0B:
			/* RRC E */
			rot_right_carry(state, &state->e);
			break;
		case 0x0C:
			/* RRC H */
			rot_right_carry(state, &state->h);
			break;
		case 0x0D:
			/* RRC L */
			rot_right_carry(state, &state->l);
			break;
		case 0x0E:
			/* RRC (HL) */
			rot_right_carry(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x0F:
			/* RRC A */
			rot_right_carry(state, &state->a);
			break;
		case 0x10:
			/* RL B */
			rot_left(state, &state->b);
			break;
		case 0x11:
			/* RL C */
			rot_left(state, &state->c);
			break;
		case 0x12:
			/* RL D */
			rot_left(state, &state->d);
			break;
		case 0x13:
			/* RL E */
			rot_left(state, &state->e);
			break;
		case 0x14:
			/* RL H */
			rot_left(state, &state->h);
			break;
		case 0x15:
			/* RL L */
			rot_left(state, &state->l);
			break;
		case 0x16:
			/* RL (HL) */
			rot_left(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x17:
			/* RL A */
			rot_left(state, &state->a);
			break;
		case 0x18:
			/* RR B */
			rot_right(state, &state->b);
			break;
		case 0x19:
			/* RR C */
			rot_right(state, &state->c);
			break;
		case 0x1A:
			/* RR D */
			rot_right(state, &state->d);
			break;
		case 0x1B:
			/* RR E */
			rot_right(state, &state->e);
			break;
		case 0x1C:
			/* RR H */
			rot_right(state, &state->h);
			break;
		case 0x1D:
			/* RR L */
			rot_right(state, &state->l);
			break;
		case 0x1E:
			/* RR (HL) */
			rot_right(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x1F:
			/* RR A */
			rot_right(state, &state->a);
			break;
		case 0x20:
			/* SLA B */
			rot_left(state, &state->b);
			state->b &= 0xFE;
			state->fz = !state->b;
			break;
		case 0x21:
			/* SLA C */
			rot_left(state, &state->c);
			state->c &= 0xFE;
			state->fz = !state->c;
			break;
		case 0x22:
			/* SLA D */
			rot_left(state, &state->d);
			state->d &= 0xFE;
			state->fz = !state->d;
			break;
		case 0x23:
			/* SLA E */
			rot_left(state, &state->e);
			state->e &= 0xFE;
			state->fz = !state->e;
			break;
		case 0x24:
			/* SLA H */
			rot_left(state, &state->h);
			state->h &= 0xFE;
			state->fz = !state->h;
			break;
		case 0x25:
			/* SLA L */
			rot_left(state, &state->l);
			state->l &= 0xFE;
			state->fz = !state->l;
			break;
		case 0x26:
			/* SLA (HL) */
			rot_left(state, &state->mem[state->hl]);
			set_mem(state->hl, state->mem[state->hl] & 0xFE);
			state->fz = !state->mem[state->hl];
			cycles = 16;
			break;
		case 0x27:
			/* SLA A */
			rot_left(state, &state->a);
			state->a &= 0xFE;
			state->fz = !state->a;
			break;
		case 0x28:
			/* SRA B */
			sra(state, &state->b);
			break;
		case 0x29:
			/* SRA C */
			sra(state, &state->c);
			break;
		case 0x2A:
			/* SRA D */
			sra(state, &state->d);
			break;
		case 0x2B:
			/* SRA E */
			sra(state, &state->e);
			break;
		case 0x2C:
			/* SRA H */
			sra(state, &state->h);
			break;
		case 0x2D:
			/* SRA L */
			sra(state, &state->l);
			break;
		case 0x2E:
			/* SRA (HL) */
			sra(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x2F:
			/* SRA A */
			sra(state, &state->a);
			break;
		case 0x30:
			/* SWAP B */
			swap(state, &state->b);
			break;
		case 0x31:
			/* SWAP C */
			swap(state, &state->c);
			break;
		case 0x32:
			/* SWAP D */
			swap(state, &state->d);
			break;
		case 0x33:
			/* SWAP E */
			swap(state, &state->e);
			break;
		case 0x34:
			/* SWAP H */
			swap(state, &state->h);
			break;
		case 0x35:
			/* SWAP L */
			swap(state, &state->l);
			break;
		case 0x36:
			/* SWAP (HL) */
			swap(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x37:
			/* SWAP A */
			swap(state, &state->a);
			break;
		case 0x38:
			/* SRL B */
			srl(state, &state->b);
			break;
		case 0x39:
			/* SRL C */
			srl(state, &state->c);
			break;
		case 0x3A:
			/* SRL D */
			srl(state, &state->d);
			break;
		case 0x3B:
			/* SRL E */
			srl(state, &state->e);
			break;
		case 0x3C:
			/* SRL H */
			srl(state, &state->h);
			break;
		case 0x3D:
			/* SRL L */
			srl(state, &state->l);
			break;
		case 0x3E:
			/* SRL (HL) */
			srl(state, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x3F:
			/* SRL A */
			srl(state, &state->a);
			break;
		case 0x40:
			/* BIT 0,B */
			bit(state, 0, &state->b);
			break;
		case 0x41:
			/* BIT 0,C */
			bit(state, 0, &state->c);
			break;
		case 0x42:
			/* BIT 0,D */
			bit(state, 0, &state->d);
			break;
		case 0x43:
			/* BIT 0,E */
			bit(state, 0, &state->e);
			break;
		case 0x44:
			/* BIT 0,H */
			bit(state, 0, &state->h);
			break;
		case 0x45:
			/* BIT 0,L */
			bit(state, 0, &state->l);
			break;
		case 0x46:
			/* BIT 0,(HL) */
			bit(state, 0, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x47:
			/* BIT 0,A */
			bit(state, 0, &state->a);
			break;
		case 0x48:
			/* BIT 1,B */
			bit(state, 1, &state->b);
			break;
		case 0x49:
			/* BIT 1,C */
			bit(state, 1, &state->c);
			break;
		case 0x4A:
			/* BIT 1,D */
			bit(state, 1, &state->d);
			break;
		case 0x4B:
			/* BIT 1,E */
			bit(state, 1, &state->e);
			break;
		case 0x4C:
			/* BIT 1,H */
			bit(state, 1, &state->h);
			break;
		case 0x4D:
			/* BIT 1,L */
			bit(state, 1, &state->l);
			break;
		case 0x4E:
			/* BIT 1,(HL) */
			bit(state, 1, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x4F:
			/* BIT 1,A */
			bit(state, 1, &state->a);
			break;
		case 0x50:
			/* BIT 2,B */
			bit(state, 2, &state->b);
			break;
		case 0x51:
			/* BIT 2,C */
			bit(state, 2, &state->c);
			break;
		case 0x52:
			/* BIT 2,D */
			bit(state, 2, &state->d);
			break;
		case 0x53:
			/* BIT 2,E */
			bit(state, 2, &state->e);
			break;
		case 0x54:
			/* BIT 2,H */
			bit(state, 2, &state->h);
			break;
		case 0x55:
			/* BIT 2,L */
			bit(state, 2, &state->l);
			break;
		case 0x56:
			/* BIT 2,(HL) */
			bit(state, 2, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x57:
			/* BIT 2,A */
			bit(state, 2, &state->a);
			break;
		case 0x58:
			/* BIT 3,B */
			bit(state, 3, &state->b);
			break;
		case 0x59:
			/* BIT 3,C */
			bit(state, 3, &state->c);
			break;
		case 0x5A:
			/* BIT 3,D */
			bit(state, 3, &state->d);
			break;
		case 0x5B:
			/* BIT 3,E */
			bit(state, 3, &state->e);
			break;
		case 0x5C:
			/* BIT 3,H */
			bit(state, 3, &state->h);
			break;
		case 0x5D:
			/* BIT 3,L */
			bit(state, 3, &state->l);
			break;
		case 0x5E:
			/* BIT 3,(HL) */
			bit(state, 3, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x5F:
			/* BIT 3,A */
			bit(state, 3, &state->a);
			break;
		case 0x60:
			/* BIT 4,B */
			bit(state, 4, &state->b);
			break;
		case 0x61:
			/* BIT 4,C */
			bit(state, 4, &state->c);
			break;
		case 0x62:
			/* BIT 4,D */
			bit(state, 4, &state->d);
			break;
		case 0x63:
			/* BIT 4,E */
			bit(state, 4, &state->e);
			break;
		case 0x64:
			/* BIT 4,H */
			bit(state, 4, &state->h);
			break;
		case 0x65:
			/* BIT 4,L */
			bit(state, 4, &state->l);
			break;
		case 0x66:
			/* BIT 4,(HL) */
			bit(state, 4, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x67:
			/* BIT 4,A */
			bit(state, 4, &state->a);
			break;
		case 0x68:
			/* BIT 5,B */
			bit(state, 5, &state->b);
			break;
		case 0x69:
			/* BIT 5,C */
			bit(state, 5, &state->c);
			break;
		case 0x6A:
			/* BIT 5,D */
			bit(state, 5, &state->d);
			break;
		case 0x6B:
			/* BIT 5,E */
			bit(state, 5, &state->e);
			break;
		case 0x6C:
			/* BIT 5,H */
			bit(state, 5, &state->h);
			break;
		case 0x6D:
			/* BIT 5,L */
			bit(state, 5, &state->l);
			break;
		case 0x6E:
			/* BIT 5,(HL) */
			bit(state, 5, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x6F:
			/* BIT 5,A */
			bit(state, 5, &state->a);
			break;
		case 0x70:
			/* BIT 6,B */
			bit(state, 6, &state->b);
			break;
		case 0x71:
			/* BIT 6,C */
			bit(state, 6, &state->c);
			break;
		case 0x72:
			/* BIT 6,D */
			bit(state, 6, &state->d);
			break;
		case 0x73:
			/* BIT 6,E */
			bit(state, 6, &state->e);
			break;
		case 0x74:
			/* BIT 6,H */
			bit(state, 6, &state->h);
			break;
		case 0x75:
			/* BIT 6,L */
			bit(state, 6, &state->l);
			break;
		case 0x76:
			/* BIT 6,(HL) */
			bit(state, 6, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x77:
			/* BIT 6,A */
			bit(state, 6, &state->a);
			break;
		case 0x78:
			/* BIT 7,B */
			bit(state, 7, &state->b);
			break;
		case 0x79:
			/* BIT 7,C */
			bit(state, 7, &state->c);
			break;
		case 0x7A:
			/* BIT 7,D */
			bit(state, 7, &state->d);
			break;
		case 0x7B:
			/* BIT 7,E */
			bit(state, 7, &state->e);
			break;
		case 0x7C:
			/* BIT 7,H */
			bit(state, 7, &state->h);
			break;
		case 0x7D:
			/* BIT 7,L */
			bit(state, 7, &state->l);
			break;
		case 0x7E:
			/* BIT 7,(HL) */
			bit(state, 7, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x7F:
			/* BIT 7,A */
			bit(state, 7, &state->a);
			break;
		case 0x80:
			/* RES 0,B */
			res(state, 0, &state->b);
			break;
		case 0x81:
			/* RES 0,C */
			res(state, 0, &state->c);
			break;
		case 0x82:
			/* RES 0,D */
			res(state, 0, &state->d);
			break;
		case 0x83:
			/* RES 0,E */
			res(state, 0, &state->e);
			break;
		case 0x84:
			/* RES 0,H */
			res(state, 0, &state->h);
			break;
		case 0x85:
			/* RES 0,L */
			res(state, 0, &state->l);
			break;
		case 0x86:
			/* RES 0,(HL) */
			res(state, 0, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x87:
			/* RES 0,A */
			res(state, 0, &state->a);
			break;
		case 0x88:
			/* RES 1,B */
			res(state, 1, &state->b);
			break;
		case 0x89:
			/* RES 1,C */
			res(state, 1, &state->c);
			break;
		case 0x8A:
			/* RES 1,D */
			res(state, 1, &state->d);
			break;
		case 0x8B:
			/* RES 1,E */
			res(state, 1, &state->e);
			break;
		case 0x8C:
			/* RES 1,H */
			res(state, 1, &state->h);
			break;
		case 0x8D:
			/* RES 1,L */
			res(state, 1, &state->l);
			break;
		case 0x8E:
			/* RES 1,(HL) */
			res(state, 1, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x8F:
			/* RES 1,A */
			res(state, 1, &state->a);
			break;
		case 0x90:
			/* RES 2,B */
			res(state, 2, &state->b);
			break;
		case 0x91:
			/* RES 2,C */
			res(state, 2, &state->c);
			break;
		case 0x92:
			/* RES 2,D */
			res(state, 2, &state->d);
			break;
		case 0x93:
			/* RES 2,E */
			res(state, 2, &state->e);
			break;
		case 0x94:
			/* RES 2,H */
			res(state, 2, &state->h);
			break;
		case 0x95:
			/* RES 2,L */
			res(state, 2, &state->l);
			break;
		case 0x96:
			/* RES 2,(HL) */
			res(state, 2, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x97:
			/* RES 2,A */
			res(state, 2, &state->a);
			break;
		case 0x98:
			/* RES 3,B */
			res(state, 3, &state->b);
			break;
		case 0x99:
			/* RES 3,C */
			res(state, 3, &state->c);
			break;
		case 0x9A:
			/* RES 3,D */
			res(state, 3, &state->d);
			break;
		case 0x9B:
			/* RES 3,E */
			res(state, 3, &state->e);
			break;
		case 0x9C:
			/* RES 3,H */
			res(state, 3, &state->h);
			break;
		case 0x9D:
			/* RES 3,L */
			res(state, 3, &state->l);
			break;
		case 0x9E:
			/* RES 3,(HL) */
			res(state, 3, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0x9F:
			/* RES 3,A */
			res(state, 3, &state->a);
			break;
		case 0xA0:
			/* RES 4,B */
			res(state, 4, &state->b);
			break;
		case 0xA1:
			/* RES 4,C */
			res(state, 4, &state->c);
			break;
		case 0xA2:
			/* RES 4,D */
			res(state, 4, &state->d);
			break;
		case 0xA3:
			/* RES 4,E */
			res(state, 4, &state->e);
			break;
		case 0xA4:
			/* RES 4,H */
			res(state, 4, &state->h);
			break;
		case 0xA5:
			/* RES 4,L */
			res(state, 4, &state->l);
			break;
		case 0xA6:
			/* RES 4,(HL) */
			res(state, 4, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xA7:
			/* RES 4,A */
			res(state, 4, &state->a);
			break;
		case 0xA8:
			/* RES 5,B */
			res(state, 5, &state->b);
			break;
		case 0xA9:
			/* RES 5,C */
			res(state, 5, &state->c);
			break;
		case 0xAA:
			/* RES 5,D */
			res(state, 5, &state->d);
			break;
		case 0xAB:
			/* RES 5,E */
			res(state, 5, &state->e);
			break;
		case 0xAC:
			/* RES 5,H */
			res(state, 5, &state->h);
			break;
		case 0xAD:
			/* RES 5,L */
			res(state, 5, &state->l);
			break;
		case 0xAE:
			/* RES 5,(HL) */
			res(state, 5, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xAF:
			/* RES 5,A */
			res(state, 5, &state->a);
			break;
		case 0xB0:
			/* RES 6,B */
			res(state, 6, &state->b);
			break;
		case 0xB1:
			/* RES 6,C */
			res(state, 6, &state->c);
			break;
		case 0xB2:
			/* RES 6,D */
			res(state, 6, &state->d);
			break;
		case 0xB3:
			/* RES 6,E */
			res(state, 6, &state->e);
			break;
		case 0xB4:
			/* RES 6,H */
			res(state, 6, &state->h);
			break;
		case 0xB5:
			/* RES 6,L */
			res(state, 6, &state->l);
			break;
		case 0xB6:
			/* RES 6,(HL) */
			res(state, 6, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xB7:
			/* RES 6,A */
			res(state, 6, &state->a);
			break;
		case 0xB8:
			/* RES 7,B */
			res(state, 7, &state->b);
			break;
		case 0xB9:
			/* RES 7,C */
			res(state, 7, &state->c);
			break;
		case 0xBA:
			/* RES 7,D */
			res(state, 7, &state->d);
			break;
		case 0xBB:
			/* RES 7,E */
			res(state, 7, &state->e);
			break;
		case 0xBC:
			/* RES 7,H */
			res(state, 7, &state->h);
			break;
		case 0xBD:
			/* RES 7,L */
			res(state, 7, &state->l);
			break;
		case 0xBE:
			/* RES 7,(HL) */
			res(state, 7, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xBF:
			/* RES 7,A */
			res(state, 7, &state->a);
			break;
		case 0xC0:
			/* SET 0,B */
			set(state, 0, &state->b);
			break;
		case 0xC1:
			/* SET 0,C */
			set(state, 0, &state->c);
			break;
		case 0xC2:
			/* SET 0,D */
			set(state, 0, &state->d);
			break;
		case 0xC3:
			/* SET 0,E */
			set(state, 0, &state->e);
			break;
		case 0xC4:
			/* SET 0,H */
			set(state, 0, &state->h);
			break;
		case 0xC5:
			/* SET 0,L */
			set(state, 0, &state->l);
			break;
		case 0xC6:
			/* SET 0,(HL) */
			set(state, 0, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xC7:
			/* SET 0,A */
			set(state, 0, &state->a);
			break;
		case 0xC8:
			/* SET 1,B */
			set(state, 1, &state->b);
			break;
		case 0xC9:
			/* SET 1,C */
			set(state, 1, &state->c);
			break;
		case 0xCA:
			/* SET 1,D */
			set(state, 1, &state->d);
			break;
		case 0xCB:
			/* SET 1,E */
			set(state, 1, &state->e);
			break;
		case 0xCC:
			/* SET 1,H */
			set(state, 1, &state->h);
			break;
		case 0xCD:
			/* SET 1,L */
			set(state, 1, &state->l);
			break;
		case 0xCE:
			/* SET 1,(HL) */
			set(state, 1, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xCF:
			/* SET 1,A */
			set(state, 1, &state->a);
			break;
		case 0xD0:
			/* SET 2,B */
			set(state, 2, &state->b);
			break;
		case 0xD1:
			/* SET 2,C */
			set(state, 2, &state->c);
			break;
		case 0xD2:
			/* SET 2,D */
			set(state, 2, &state->d);
			break;
		case 0xD3:
			/* SET 2,E */
			set(state, 2, &state->e);
			break;
		case 0xD4:
			/* SET 2,H */
			set(state, 2, &state->h);
			break;
		case 0xD5:
			/* SET 2,L */
			set(state, 2, &state->l);
			break;
		case 0xD6:
			/* SET 2,(HL) */
			set(state, 2, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xD7:
			/* SET 2,A */
			set(state, 2, &state->a);
			break;
		case 0xD8:
			/* SET 3,B */
			set(state, 3, &state->b);
			break;
		case 0xD9:
			/* SET 3,C */
			set(state, 3, &state->c);
			break;
		case 0xDA:
			/* SET 3,D */
			set(state, 3, &state->d);
			break;
		case 0xDB:
			/* SET 3,E */
			set(state, 3, &state->e);
			break;
		case 0xDC:
			/* SET 3,H */
			set(state, 3, &state->h);
			break;
		case 0xDD:
			/* SET 3,L */
			set(state, 3, &state->l);
			break;
		case 0xDE:
			/* SET 3,(HL) */
			set(state, 3, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xDF:
			/* SET 3,A */
			set(state, 3, &state->a);
			break;
		case 0xE0:
			/* SET 4,B */
			set(state, 4, &state->b);
			break;
		case 0xE1:
			/* SET 4,C */
			set(state, 4, &state->c);
			break;
		case 0xE2:
			/* SET 4,D */
			set(state, 4, &state->d);
			break;
		case 0xE3:
			/* SET 4,E */
			set(state, 4, &state->e);
			break;
		case 0xE4:
			/* SET 4,H */
			set(state, 4, &state->h);
			break;
		case 0xE5:
			/* SET 4,L */
			set(state, 4, &state->l);
			break;
		case 0xE6:
			/* SET 4,(HL) */
			set(state, 4, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xE7:
			/* SET 4,A */
			set(state, 4, &state->a);
			break;
		case 0xE8:
			/* SET 5,B */
			set(state, 5, &state->b);
			break;
		case 0xE9:
			/* SET 5,C */
			set(state, 5, &state->c);
			break;
		case 0xEA:
			/* SET 5,D */
			set(state, 5, &state->d);
			break;
		case 0xEB:
			/* SET 5,E */
			set(state, 5, &state->e);
			break;
		case 0xEC:
			/* SET 5,H */
			set(state, 5, &state->h);
			break;
		case 0xED:
			/* SET 5,L */
			set(state, 5, &state->l);
			break;
		case 0xEE:
			/* SET 5,(HL) */
			set(state, 5, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xEF:
			/* SET 5,A */
			set(state, 5, &state->a);
			break;
		case 0xF0:
			/* SET 6,B */
			set(state, 6, &state->b);
			break;
		case 0xF1:
			/* SET 6,C */
			set(state, 6, &state->c);
			break;
		case 0xF2:
			/* SET 6,D */
			set(state, 6, &state->d);
			break;
		case 0xF3:
			/* SET 6,E */
			set(state, 6, &state->e);
			break;
		case 0xF4:
			/* SET 6,H */
			set(state, 6, &state->h);
			break;
		case 0xF5:
			/* SET 6,L */
			set(state, 6, &state->l);
			break;
		case 0xF6:
			/* SET 6,(HL) */
			set(state, 6, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xF7:
			/* SET 6,A */
			set(state, 6, &state->a);
			break;
		case 0xF8:
			/* SET 7,B */
			set(state, 7, &state->b);
			break;
		case 0xF9:
			/* SET 7,C */
			set(state, 7, &state->c);
			break;
		case 0xFA:
			/* SET 7,D */
			set(state, 7, &state->d);
			break;
		case 0xFB:
			/* SET 7,E */
			set(state, 7, &state->e);
			break;
		case 0xFC:
			/* SET 7,H */
			set(state, 7, &state->h);
			break;
		case 0xFD:
			/* SET 7,L */
			set(state, 7, &state->l);
			break;
		case 0xFE:
			/* SET 7,(HL) */
			set(state, 7, &state->mem[state->hl]);
			cycles = 16;
			break;
		case 0xFF:
			/* SET 7,A */
			set(state, 7, &state->a);
			break;
		default:
			fprintf(stderr, "%04X : CB %02X instruction does not exit\n", state->pc - 1, state->mem[pc]);
			fprintf_debug_info(stdout);
			cycles = 0;
			break;
	}
	handle_debug(pc_start, state->pc, op, cycles, 1);
	return cycles;
}

/*
 * Executes operation in memory at PC. Updates PC reference.
 * Returns number of clock cycles.
 */
int execute(struct gb_state *state) {
	int pc = state->pc;
	uint8_t *op = &state->mem[pc];
	int cycles = 4;
	int pc_start = state->pc++;
	uint16_t nn = ((uint16_t)op[2] << 8) | op[1];
	switch (*op) {
		case 0x00:
			/* NOP */
			break;
		case 0x01:
			/* LD BC,nn */
			state->c = op[1];
			state->b = op[2];
			cycles = 12;
			state->pc += 2;
			break;
		case 0x02:
			/* LD (BC),A */
			set_mem(state->bc, state->a);
			cycles = 8;
			break;
		case 0x03:
			/* INC BC */
			state->bc++;
			cycles = 8;
			break;
		case 0x04:
			/* INC B */
			set_add8_flags(state, state->b, 1, 0);
			state->b++;
			break;
		case 0x05:
			/* DEC B */
			set_sub8_flags(state, state->b, 1, 0);
			state->b--;
			break;
		case 0x06:
			/* LD B,n */
			cycles = load8val2reg(state, &state->b, op[1]);
			break;
		case 0x07:
			/* RLCA */
			rot_left_carry(state, &state->a);
			state->fz = 0;
			break;
		case 0x08:
			/* LD (nn),SP */
			set_mem(nn, state->sp);
			state->pc += 2;
			cycles = 20;
			break;
		case 0x09:
			/* ADD HL,BC */
			set_add16_flags(state, state->hl, state->bc);
			state->hl += state->bc;
			cycles = 8;
			break;
		case 0x0A:
			/* LD A,(BC) */
			state->a = state->mem[state->bc];
			cycles = 8;
			break;
		case 0x0B:
			/* DEC BC */
			state->bc--;
			cycles = 8;
			break;
		case 0x0C:
			/* INC C */
			set_add8_flags(state, state->c++, 1, 0);
			break;
		case 0x0D:
			/* DEC C */
			set_sub8_flags(state, state->c--, 1, 0);
			break;
		case 0x0E:
			/* LD C,n */
			cycles = load8val2reg(state, &state->c, op[1]);
			break;
		case 0x0F:
			/* RRCA */
			rot_right_carry(state, &state->a);
			state->fz = 0;
			break;
		case 0x10:
			/* STOP 0 */
			fprintf(stderr, "STOP 0 not implemented\n");
			state->pc++;
			//fprintf_debug_info(stdout);
			break;
		case 0x11:
			/* LD DE,nn */
			state->e = op[1];
			state->d = op[2];
			cycles = 12;
			state->pc += 2;
			break;
		case 0x12:
			/* LD (DE),A */
			set_mem(state->de, state->a);
			cycles = 8;
			break;
		case 0x13:
			/* INC DE */
			state->de++;
			cycles = 8;
			break;
		case 0x14:
			/* INC D */
			set_add8_flags(state, state->d++, 1, 0);
			break;
		case 0x15:
			/* DEC D */
			set_sub8_flags(state, state->d--, 1, 0);
			break;
		case 0x16:
			/* LD D,n */
			cycles = load8val2reg(state, &state->d, op[1]);
			break;
		case 0x17:
			/* RLA */
			rot_left(state, &state->a);
			state->fz = 0;
			break;
		case 0x18:
			/* JR n */
			//printf("%04X JR %02X\n", state->pc-1, op[1]);
			state->pc += 1 + (int8_t)op[1];
			cycles = 8;
			break;
		case 0x19:
			/* ADD HL,DE */
			set_add16_flags(state, state->hl, state->de);
			state->hl += state->de;
			cycles = 8;
			break;
		case 0x1A:
			/* LD A,(DE) */
			state->a = state->mem[state->de];
			cycles = 8;
			break;
		case 0x1B:
			/* DEC DE */
			state->de--;
			cycles = 8;
			break;
		case 0x1C:
			/* INC E */
			set_add8_flags(state, state->e++, 1, 0);
			break;
		case 0x1D:
			/* DEC E */
			set_sub8_flags(state, state->e--, 1, 0);
			break;
		case 0x1E:
			/* LD E,n */
			cycles = load8val2reg(state, &state->e, op[1]);
			break;
		case 0x1F:
			/* RRA */
			rot_right(state, &state->a);
			state->fz = 0;
			break;
		case 0x20:
			/* JR NZ,n */
			state->pc++;
			if (!state->fz) {
				//printf("%04X JR %02X\n", state->pc-2, op[1]);
				state->pc += (int8_t)op[1];
			}
			cycles = 8;
			break;
		case 0x21:
			/* LD HL,nn */
			state->l = op[1];
			state->h = op[2];
			cycles = 12;
			state->pc += 2;
			break;
		case 0x22:
			/* LD (HL+),A */
			set_mem(state->hl++, state->a);
			cycles = 8;
			break;
		case 0x23:
			/* INC HL */
			state->hl++;
			cycles = 8;
			break;
		case 0x24:
			/* INC H */
			set_add8_flags(state, state->h++, 1, 0);
			break;
		case 0x25:
			/* DEC H */
			set_sub8_flags(state, state->h--, 1, 0);
			break;
		case 0x26:
			/* LD H,n */
			cycles = load8val2reg(state, &state->h, op[1]);
			break;
		case 0x27:
			/* DAA */
			//printf("DAA\n");
			daa(state);
			break;
		case 0x28:
			/* JR Z,n */
			state->pc++;
			if (state->fz) {
				//printf("%04X JR Z %02X\n", state->pc-2, op[1]);
				//print_registers(state);
				state->pc += op[1];
			}
			cycles = 8;
			break;
		case 0x29:
			/* ADD HL,HL */
			set_add16_flags(state, state->hl, state->hl);
			state->hl += state->hl;
			cycles = 8;
			break;
		case 0x2A:
			/* LD A,(HL+) */
			state->a = state->mem[state->hl++];
			cycles = 8;
			break;
		case 0x2B:
			/* DEC HL */
			state->hl--;
			cycles = 8;
			break;
		case 0x2C:
			/* INC L */
			set_add8_flags(state, state->l++, 1, 0);
			break;
		case 0x2D:
			/* DEC L */
			set_sub8_flags(state, state->l--, 1, 0);
			break;
		case 0x2E:
			/* LD L,n */
			cycles = load8val2reg(state, &state->l, op[1]);
			break;
		case 0x2F:
			/* CPL */
			state->a = ~state->a;
			state->fn = 1;
			state->fh = 1;
			break;
		case 0x30:
			/* JR NC,n */
			state->pc++;
			if (!state->fc) {
				//printf("%04X JR NC %02X\n", state->pc-2, op[1]);
				state->pc += (int8_t)op[1];
			}
			cycles = 8;
			break;
		case 0x31:
			/* LD SP,nn */
			state->sp = nn;
			cycles = 12;
			state->pc += 2;
			break;
		case 0x32:
			/* LD (HL-),A */
			set_mem(state->hl, state->a);
			cycles = 8;
			state->hl--;
			break;
		case 0x33:
			/* INC SP */
			state->sp++;
			cycles = 8;
			break;
		case 0x34:
			/* INC (HL) */
			set_add8_flags(state, state->mem[state->hl]++, 1, 0);
			cycles = 12;
			break;
		case 0x35:
			/* DEC (HL) */
			set_sub8_flags(state, state->mem[state->hl]--, 1, 0);
			cycles = 12;
			break;
		case 0x36:
			/* LD (HL),n */
			set_mem(state->hl, op[1]);
			state->pc++;
			cycles = 12;
			break;
		case 0x37:
			/* SCF */
			state->fc = 1;
			state->fn = 0;
			state->fh = 0;
			break;
		case 0x38:
			/* JR C,n */
			state->pc++;
			if (state->fc) {
				//printf("%04X JR C %02X\n", state->pc-2, op[1]);
				state->pc += (int8_t)op[1];
			}
			cycles = 8;
			break;
		case 0x39:
			/* ADD HL,SP */
			set_add16_flags(state, state->hl, state->sp);
			state->hl += state->sp;
			cycles = 8;
			break;
		case 0x3A:
			/* LD A,(HL-) */
			state->a = state->mem[state->hl];
			cycles = 8;
			state->hl--;
			break;
		case 0x3B:
			/* DEC SP */
			state->sp--;
			cycles = 8;
			break;
		case 0x3C:
			/* INC A */
			set_add8_flags(state, state->a++, 1, 0);
			break;
		case 0x3D:
			/* DEC A */
			set_sub8_flags(state, state->a--, 1, 0);
			break;
		case 0x3E:
			/* LD A,n */
			cycles = load8val2reg(state, &state->a, op[1]);
			break;
		case 0x3F:
			/* CCF */
			state->fc = ~state->fc;
			state->fn = 0;
			state->fh = 0;
			break;
		case 0x40:
			/* LD B,B */
			state->b = state->b;
			break;
		case 0x41:
			/* LD B,C */
			state->b = state->c;
			break;
		case 0x42:
			/* LD B,D */
			state->b = state->d;
			break;
		case 0x43:
			/* LD B,E */
			state->b = state->e;
			break;
		case 0x44:
			/* LD B,H */
			state->b = state->h;
			break;
		case 0x45:
			/* LD B,L */
			state->b = state->l;
			break;
		case 0x46:
			/* LD B,(HL) */
			state->b = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x47:
			/* LD B,A */
			state->b = state->a;
			break;
		case 0x48:
			/* LD C,B */
			state->c = state->b;
			break;
		case 0x49:
			/* LD C,C */
			state->c = state->c;
			break;
		case 0x4A:
			/* LD C,D */
			state->c = state->d;
			break;
		case 0x4B:
			/* LD C,E */
			state->c = state->e;
			break;
		case 0x4C:
			/* LD C,H */
			state->c = state->h;
			break;
		case 0x4D:
			/* LD C,L */
			state->c = state->l;
			break;
		case 0x4E:
			/* LD C,(HL) */
			state->c = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x4F:
			/* LD C,A */
			state->c = state->a;
			break;
		case 0x50:
			/* LD D,B */
			state->d = state->b;
			break;
		case 0x51:
			/* LD D,C */
			state->d = state->c;
			break;
		case 0x52:
			/* LD D,D */
			state->d = state->d;
			break;
		case 0x53:
			/* LD D,E */
			state->d = state->e;
			break;
		case 0x54:
			/* LD D,H */
			state->d = state->h;
			break;
		case 0x55:
			/* LD D,L */
			state->d = state->l;
			break;
		case 0x56:
			/* LD D,(HL) */
			state->d = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x57:
			/* LD D,A */
			state->d = state->a;
			break;
		case 0x58:
			/* LD E,B */
			state->e = state->b;
			break;
		case 0x59:
			/* LD E,C */
			state->e = state->c;
			break;
		case 0x5A:
			/* LD E,D */
			state->e = state->d;
			break;
		case 0x5B:
			/* LD E,E */
			state->e = state->e;
			break;
		case 0x5C:
			/* LD E,H */
			state->e = state->h;
			break;
		case 0x5D:
			/* LD E,L */
			state->e = state->l;
			break;
		case 0x5E:
			/* LD E,(HL) */
			state->e = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x5F:
			/* LD E,A */
			state->e = state->a;
			break;
		case 0x60:
			/* LD H,B */
			state->h = state->b;
			break;
		case 0x61:
			/* LD H,C */
			state->h = state->c;
			break;
		case 0x62:
			/* LD H,D */
			state->h = state->d;
			break;
		case 0x63:
			/* LD H,E */
			state->h = state->e;
			break;
		case 0x64:
			/* LD H,H */
			state->h = state->h;
			break;
		case 0x65:
			/* LD H,L */
			state->h = state->l;
			break;
		case 0x66:
			/* LD H,(HL) */
			state->h = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x67:
			/* LD H,A */
			state->h = state->a;
			break;
		case 0x68:
			/* LD L,B */
			state->l = state->b;
			break;
		case 0x69:
			/* LD L,C */
			state->l = state->c;
			break;
		case 0x6A:
			/* LD L,D */
			state->l = state->d;
			break;
		case 0x6B:
			/* LD L,E */
			state->l = state->e;
			break;
		case 0x6C:
			/* LD L,H */
			state->l = state->h;
			break;
		case 0x6D:
			/* LD L,L */
			state->l = state->l;
			break;
		case 0x6E:
			/* LD L,(HL) */
			state->l = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x6F:
			/* LD L,A */
			state->l = state->a;
			break;
		case 0x70:
			/* LD (HL),B */
			set_mem(state->hl, state->b);
			cycles = 8;
			break;
		case 0x71:
			/* LD (HL),C */
			set_mem(state->hl, state->c);
			cycles = 8;
			break;
		case 0x72:
			/* LD (HL),D */
			set_mem(state->hl, state->d);
			cycles = 8;
			break;
		case 0x73:
			/* LD (HL),E */
			set_mem(state->hl, state->e);
			cycles = 8;
			break;
		case 0x74:
			/* LD (HL),H */
			set_mem(state->hl, state->h);
			cycles = 8;
			break;
		case 0x75:
			/* LD (HL),L */
			set_mem(state->hl, state->l);
			cycles = 8;
			break;
		case 0x76:
			/* HALT */
			state->halt = 1;
			// TODO: figure out if supposed to enable IME
			state->ime = 1;
			state->ime = 1;
			break;
		case 0x77:
			/* LD (HL),A */
			set_mem(state->hl, state->a);
			cycles = 8;
			break;
		case 0x78:
			/* LD A,B */
			state->a = state->b;
			break;
		case 0x79:
			/* LD A,C */
			state->a = state->c;
			break;
		case 0x7A:
			/* LD A,D */
			state->a = state->d;
			break;
		case 0x7B:
			/* LD A,E */
			state->a = state->e;
			break;
		case 0x7C:
			/* LD A,H */
			state->a = state->h;
			break;
		case 0x7D:
			/* LD A,L */
			state->a = state->l;
			break;
		case 0x7E:
			/* LD A,(HL) */
			state->a = state->mem[state->hl];
			cycles = 8;
			break;
		case 0x7F:
			/* LD A,A */
			state->a = state->a;
			break;
		case 0x80:
			/* ADD A,B */
			addA(state, state->b);
			break; 
		case 0x81:
			/* ADD A,C */
			addA(state, state->c);
			break; 
		case 0x82:
			/* ADD A,D */
			addA(state, state->d);
			break; 
		case 0x83:
			/* ADD A,E */
			addA(state, state->e);
			break; 
		case 0x84:
			/* ADD A,H */
			addA(state, state->h);
			break; 
		case 0x85:
			/* ADD A,L */
			addA(state, state->l);
			break; 
		case 0x86:
			/* ADD A,(HL) */
			addA(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0x87:
			/* ADD A,A */
			addA(state, state->a);
			break; 
		case 0x88:
			/* ADC A,B */
			adc(state, state->b);
			break; 
		case 0x89:
			/* ADC A,C */
			adc(state, state->c);
			break; 
		case 0x8A:
			/* ADC A,D */
			adc(state, state->d);
			break; 
		case 0x8B:
			/* ADC A,E */
			adc(state, state->e);
			break; 
		case 0x8C:
			/* ADC A,H */
			adc(state, state->h);
			break; 
		case 0x8D:
			/* ADC A,L */
			adc(state, state->l);
			break; 
		case 0x8E:
			/* ADC A,(HL) */
			adc(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0x8F:
			/* ADC A,A */
			adc(state, state->a);
			break; 
		case 0x90:
			/* SUB A,B */
			subA(state, state->b);
			break; 
		case 0x91:
			/* SUB A,C */
			subA(state, state->c);
			break; 
		case 0x92:
			/* SUB A,D */
			subA(state, state->d);
			break; 
		case 0x93:
			/* SUB A,E */
			subA(state, state->e);
			break; 
		case 0x94:
			/* SUB A,H */
			subA(state, state->h);
			break; 
		case 0x95:
			/* SUB A,L */
			subA(state, state->l);
			break; 
		case 0x96:
			/* SUB A,(HL) */
			subA(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0x97:
			/* SUB A,A */
			subA(state, state->a);
			break; 
		case 0x98:
			/* SBC A,B */
			subc(state, state->b);
			break; 
		case 0x99:
			/* SBC A,C */
			subc(state, state->c);
			break; 
		case 0x9A:
			/* SBC A,D */
			subc(state, state->d);
			break; 
		case 0x9B:
			/* SBC A,E */
			subc(state, state->e);
			break; 
		case 0x9C:
			/* SBC A,H */
			subc(state, state->h);
			break; 
		case 0x9D:
			/* SBC A,L */
			subc(state, state->l);
			break; 
		case 0x9E:
			/* SBC A,(HL) */
			subc(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0x9F:
			/* SBC A,A */
			subc(state, state->a);
			break;
			//HERE
		case 0xA0:
			/* AND A,B */
			andA(state, state->b);
			break; 
		case 0xA1:
			/* AND A,C */
			andA(state, state->c);
			break; 
		case 0xA2:
			/* AND A,D */
			andA(state, state->d);
			break; 
		case 0xA3:
			/* AND A,E */
			andA(state, state->e);
			break; 
		case 0xA4:
			/* AND A,H */
			andA(state, state->h);
			break; 
		case 0xA5:
			/* AND A,L */
			andA(state, state->l);
			break; 
		case 0xA6:
			/* AND A,(HL) */
			andA(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0xA7:
			/* AND A,A */
			andA(state, state->a);
			break; 
		case 0xA8:
			/* XOR A,B */
			xorA(state, state->b);
			break; 
		case 0xA9:
			/* XOR A,C */
			xorA(state, state->c);
			break; 
		case 0xAA:
			/* XOR A,D */
			xorA(state, state->d);
			break; 
		case 0xAB:
			/* XOR A,E */
			xorA(state, state->e);
			break; 
		case 0xAC:
			/* XOR A,H */
			xorA(state, state->h);
			break; 
		case 0xAD:
			/* XOR A,L */
			xorA(state, state->l);
			break; 
		case 0xAE:
			/* XOR A,(HL) */
			xorA(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0xAF:
			/* XOR A */
			state->a = 0;
			state->fz = 1;
			state->fh = 0;
			state->fn = 0;
			state->fc = 0;
			break;
		case 0xB0:
			/* OR A,B */
			orA(state, state->b);
			break; 
		case 0xB1:
			/* OR A,C */
			orA(state, state->c);
			break; 
		case 0xB2:
			/* OR A,D */
			orA(state, state->d);
			break; 
		case 0xB3:
			/* OR A,E */
			orA(state, state->e);
			break; 
		case 0xB4:
			/* OR A,H */
			orA(state, state->h);
			break; 
		case 0xB5:
			/* OR A,L */
			orA(state, state->l);
			break; 
		case 0xB6:
			/* OR A,(HL) */
			orA(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0xB7:
			/* OR A,A */
			orA(state, state->a);
			break; 
		case 0xB8:
			/* CP A,B */
			cpA(state, state->b);
			break; 
		case 0xB9:
			/* CP A,C */
			cpA(state, state->c);
			break; 
		case 0xBA:
			/* CP A,D */
			cpA(state, state->d);
			break; 
		case 0xBB:
			/* CP A,E */
			cpA(state, state->e);
			break; 
		case 0xBC:
			/* CP A,H */
			cpA(state, state->h);
			break; 
		case 0xBD:
			/* CP A,L */
			cpA(state, state->l);
			break; 
		case 0xBE:
			/* CP A,(HL) */
			cpA(state, state->mem[state->hl]);
			cycles = 8;
			break; 
		case 0xBF:
			/* CP A */
			cpA(state, state->a);
			break;
		case 0xC0:
			/* RET NZ */
			ret(state, !state->fz);
			cycles = 8;
			break;
		case 0xC1:
			/* POP BC */
			pop(state, &state->bc);
			cycles = 12;
			break;
		case 0xC2:
			/* JP NZ,nn */
			//printf("%04X JP NZ %04X\n", state->pc-1, nn);
			state->pc += 2;
			jump(state, !state->fz, nn);
			cycles = 12;
			break;
		case 0xC3:
			/* JP nn */
			//printf("%04X JP %04X\n", state->pc-1, nn);
			state->pc += 2;
			jump(state, 1, nn);
			cycles = 12;
			break;
		case 0xC4:
			/* CALL NZ,nn */
			state->pc += 2;
			call(state, !state->fz, nn);
			cycles = 12;
			break;
		case 0xC5:
			/* PUSH BC */
			cycles = 16;
			push(state, state->bc);
			break;
		case 0xC6:
			/* ADD A,n */
			cycles = 8;
			state->pc++;
			addA(state, op[1]);
			break;
		case 0xC7:
			/* RST 0x00 */
			cycles = 32;
			rst(state, 0x00);
			break;
		case 0xC8:
			/* RET Z */
			ret(state, state->fz);
			cycles = 8;
			break;
		case 0xC9:
			/* RET */
			ret(state, 1);
			cycles = 8;
			break;
		case 0xCA:
			/* JP Z,nn */
			//printf("%04X JP Z %04X\n", state->pc-1, nn);
			state->pc += 2;
			jump(state, state->fz, nn);
			cycles = 12;
			break;
		case 0xCB:
			cycles = execute_cb(state);
			break;
		case 0xCC:
			/* CALL Z,nn */
			state->pc += 2;
			call(state, state->fz, nn);
			cycles = 12;
			break;
		case 0xCD:
			/* CALL nn */
			state->pc += 2;
			call(state, 1, nn);
			cycles = 12;
			break;
		case 0xCE:
			/* ADC A,n */
			state->pc++;
			adc(state, op[1]);
			cycles = 8;
			break;
		case 0xCF:
			/* RST 0x08 */
			cycles = 32;
			rst(state, 0x08);
			break;
		case 0xD0:
			/* RET NC */
			ret(state, !state->fc);
			cycles = 8;
			break;
		case 0xD1:
			/* POP DE */
			pop(state, &state->de);
			cycles = 12;
			break;
		case 0xD2:
			/* JP NC,nn */
			//printf("%04X JP NC %04X\n", state->pc-1, nn);
			state->pc += 2;
			jump(state, !state->fc, nn);
			cycles = 12;
			break;
		case 0xD4:
			/* CALL NC,nn */
			state->pc += 2;
			call(state, !state->fc, nn);
			cycles = 12;
			break;
		case 0xD5:
			/* PUSH DE */
			cycles = 16;
			push(state, state->de);
			break;
		case 0xD6:
			/* SUB A,n */
			cycles = 8;
			state->pc++;
			subA(state, op[1]);
			break;
		case 0xD7:
			/* RST 0x10 */
			cycles = 32;
			rst(state, 0x10);
			break;
		case 0xD8:
			/* RET C */
			ret(state, state->fc);
			cycles = 8;
			break;
		case 0xD9:
			/* RETI */
			ret(state, 1);
			state->ime = 1;
			cycles = 8;
			break;
		case 0xDA:
			/* JP C,nn */
			//printf("%04X JP C %04X\n", state->pc-1, nn);
			state->pc += 2;
			jump(state, state->fc, nn);
			cycles = 12;
			break;
		case 0xDC:
			/* CALL C,nn */
			state->pc += 2;
			call(state, state->fc, nn);
			cycles = 12;
			break;
		case 0xDE:
			/* SDC A,n */
			state->pc++;
			subc(state, op[1]);
			cycles = 8;
			break;
		case 0xDF:
			/* RST 0x18 */
			cycles = 32;
			rst(state, 0x18);
			break;
		case 0xE0:
			/* 
			 * LDH (n),A
			 * LD (n+$FF00),A
			 */
			set_mem(op[1] + IO_PORTS, state->a);
			cycles = 12;
			state->pc++;
			break;
		case 0xE1:
			/* POP HL */
			pop(state, &state->hl);
			cycles = 12;
			break;
		case 0xE2:
			/* LD (C+$FF00),A */
			set_mem(state->c + IO_PORTS, state->a);
			cycles = 8;
			break;
		case 0xE5:
			/* PUSH HL */
			cycles = 16;
			push(state, state->hl);
			break;
		case 0xE6:
			/* AND A,n */
			cycles = 8;
			state->pc++;
			andA(state, op[1]);
			break;
		case 0xE7:
			/* RST 0x20 */
			cycles = 32;
			rst(state, 0x20);
			break;
		case 0xE8:
			/* ADD SP,n */
			cycles = 16;
			set_add8_flags(state, state->sp, op[1], 1);
			state->a += op[1];
			break;
		case 0xE9:
			/* JP (HL) */
			//printf("%04X JP (%04X)\n", state->pc-1, state->hl);
			state->pc = state->hl;
			break;
		case 0xEA:
			/* LD (nn),A */
			set_mem(nn, state->a);
			state->pc += 2;
			cycles = 16;
			break;
		case 0xEE:
			/* XOR A,n */
			state->pc++;
			xorA(state, op[1]);
			cycles = 8;
			break;
		case 0xEF:
			/* RST 0x28 */
			cycles = 32;
			rst(state, 0x28);
			break;
		case 0xF0:
			/* 
			 * LDH A,(n)
			 * LD A,(n+$FF00)
			 */
			state->a = state->mem[op[1] + IO_PORTS];
			cycles = 12;
			state->pc++;
			break;
		case 0xF1:
			/* POP AF */
			pop(state, &state->af);
			state->fl = 0;
			cycles = 12;
			break;
		case 0xF2:
			/* LD A,(C+$FF00) */
			state->a = state->mem[state->c + IO_PORTS];
			cycles = 8;
			break;
		case 0xF3:
			/* DI */
			state->di = 1;
			break;
		case 0xF5:
			/* PUSH AF */
			cycles = 16;
			state->fl = 0;
			push(state, state->af);
			break;
		case 0xF6:
			/* OR A,n */
			cycles = 8;
			state->pc++;
			orA(state, op[1]);
			break;
		case 0xF7:
			/* RST 0x30 */
			cycles = 32;
			rst(state, 0x30);
			break;
		case 0xF8:
			/* LD HL,SP+n */
			/* LDHL SP,n */
			state->pc++;
			set_add8_flags(state, state->sp, op[1], 1);
			state->fz = 0;
			state->hl = state->sp + op[1];
			cycles = 12;
			break;
		case 0xF9:
			/* LD SP,HL */
			state->sp = state->hl;
			cycles = 8;
			break;
		case 0xFA:
			/* LD A,(nn) */
			state->pc += 2;
			state->a = state->mem[nn];
			cycles = 16;
			break;
		case 0xFB:
			/* EI */
			state->ei_flag = 1;
			break;
		case 0xFE:
			/* CP A,n */
			state->pc++;
			cpA(state, op[1]);
			cycles = 8;
			break;
		case 0xFF:
			/* RST 0x38 */
			cycles = 32;
			rst(state, 0x38);
			break;
		default:
			fprintf(stderr, "%04X : %02X does not exist\n", state->pc - 2, *op);
			fprintf_debug_info(stdout);
			cycles = 0;
			break;
	};
	if (state->di) {
		state->di = 0;
		state->ime = 0;
	}
	if (state->ei_flag) {
		state->ime = 1;
		state->ei_flag = 0;
	}
	/*printf("%04X:", pc);
	  int k = 0;
	  int diff = state->pc - pc;
	  if (1 || diff <= 0 || diff > 3) {
	  printf("%02X %02X %02X", state->mem[pc], state->mem[pc+1], state->mem[pc+2]);
	  } else {
	  for (k = 0; k < diff; ++k) {
	  printf(" %02X", state->mem[pc+k]);
	  }
	  }
	  puts("");
	  print_registers(state);*/
	handle_debug(pc_start, state->pc, op, cycles, 0);
	return cycles;
}

void print_memory(struct gb_state *s) {
	int column_length = 0x20;
	for (int i = 0x00; i < 0x10000; i+= column_length) {
		printf("%04X\t", i);
		for (int j = i; j < i + column_length; j++) {
			printf("%02X ", s->mem[j]);
		}
		puts("");
	}
}

void handle_interrupts(struct gb_state *state) {
	if (!state->ime)
		return;
	uint8_t ie = state->mem[IE];
	uint8_t iff = state->mem[IF];
	uint16_t addr = 0x0000;
	//printf("IE: %02X, IF: %02X, IME: %02X\n", ie, iff, state->ime);
	if (iff & ie & 0x01) {
		// V-Blank
		addr = VBLANK_ADDR;
		//printf("VBLANK INT\n");
	} else if (iff & ie & 0x02) {
		// LCDC
		addr = LCDC_ADDR;
		//printf("LCDC_ADDR INT\n");
	} else if (iff & ie & 0x04) {
		// Timer overflow
		addr = TIMER_OVERFLOW_ADDR;
		//printf("TIMER_OVERFLOW INT\n");
	} else if (iff & ie & 0x08) {
		// Serial I/O transfer complete
		addr = SERIAL_IO_TRANS_ADDR;
		//printf("SERIAL_IO_TRANS INT\n");
	} else if (iff & ie & 0x10) {
		// Transition from High to Low of Pin number P10-P13
		addr = P10_P13_TRANSITION_ADDR;
		//printf("P10_P13 INT\n");
	}

	if (addr != 0x000) {
		state->ime = 0;
		push(state, state->pc);
		state->pc = addr;
		state->halt = 0;
		set_mem(IF, 0);
	}

}

void handle_timers(struct gb_state *state, uint8_t cycles, uint16_t *div_cycles, uint32_t *timer_cycles) {
	*div_cycles += cycles;
	*timer_cycles += cycles;
	if (*div_cycles >= 16384) {
		*div_cycles = 0;
		state->mem[DIV] += 1;
	}
	if ((state->mem[TAC] & 0x04) != 0x04)
		return;
	uint32_t tima_freq = 2147483647;
	switch (state->mem[TAC] & 0x03) {
		case 0x00:
			tima_freq = 4096;
			break;
		case 0x01:
			tima_freq = 262144;
			break;
		case 0x10:
			tima_freq = 65536;
			break;
		case 0x11:
			tima_freq = 16384;
			break;
	}
	if (*timer_cycles >= tima_freq) {
		*timer_cycles = 0;
		if (state->mem[TIMA] == 0xFF) {
			state->mem[IF] |= 0x04;
			state->mem[TIMA] = state->mem[TMA];
		} else {
			state->mem[TIMA] += 1;
		}
	}
}

int tick(struct gb_state *state, int *total_cycles, uint16_t *div_cycles, uint32_t *timer_cycles) {
	//printf("%04X: %02X ", state->pc, state->mem[state->pc]);
	if (state->pc == 0xC7D2 && state->mem[state->pc] == 0x18) {
		//exit(0);
	}
	int cycles = 4;
	if (!state->halt) {
		cycles = execute(state);
		if (cycles == 0) return 1;
		for (int i = 0; i < cycles; i++) {
			if (++*total_cycles > MAX_CYCLES_PER_FRAME)
			{
				*total_cycles = 0;
			}
			if (gpu_tick())
				return 1;
		}
	}
	handle_timers(state, cycles, div_cycles, timer_cycles);
	handle_interrupts(state);
	return 0;
}

int run_bootstrap(struct gb_state *state) {
	uint16_t div_cycles = 0;
	uint32_t timer_cycles = 0;
	int total_cycles = 0;
	while (state->mem[0xFF50] != 0x01)
	{
		if (state->mem[SCY] < 0x05 || state->mem[SCY] > 0x80) {
			//print_registers(state);
		}
		if (state->mem[SCY] > 0x80) {
			//return 1;
		}
		if (tick(state, &total_cycles, &div_cycles, &timer_cycles)) {
			return 1;
		}
	}
	/*
	   clock_t diff = clock() - start;
	   uint32_t ms = diff * 1000 / CLOCKS_PER_SEC;
	   printf("%d frames in %d ms\n", frame_count, ms);
	 */
	return 0;
}

int power_up(struct gb_state *state, int bootstrap_flag) {
	state->pc = 0x0000;
	//custom
	state->ime = 0;
	state->halt = 0;

	if (bootstrap_flag) {
		set_mem(LY, 0x90); // needed for bootstrap
		if (run_bootstrap(state)) {
			fprintf(stderr, "Bootstrap exited early\n");
			return 1;
		}
		//print_memory(state);
	}

	state->a = 0x01;
	state->f = 0xB0;
	state->b = 0x00;
	state->c = 0x13;
	state->d = 0x00;
	state->e = 0xD8;
	state->h = 0x01;
	state->l = 0x4D;
	state->sp = 0xFFFE;
	set_mem(TIMA, 0x00);
	set_mem(TMA, 0x00);
	set_mem(TAC, 0x00);
	set_mem(NR10, 0x80);
	set_mem(NR11, 0xBF);
	set_mem(NR12, 0xF3);
	set_mem(NR14, 0xBF);
	set_mem(NR21, 0x3F);
	set_mem(NR22, 0x00);
	set_mem(NR24, 0xBF);
	set_mem(NR30, 0x7F);
	set_mem(NR31, 0xFF);
	set_mem(NR32, 0x9F);
	set_mem(NR34, 0xBF);
	set_mem(NR41, 0xFF);
	set_mem(NR42, 0x00);
	set_mem(NR43, 0x00);
	set_mem(NR44, 0xBF);
	set_mem(NR50, 0x77);
	set_mem(NR51, 0xF3);
	set_mem(NR52, 0xF1);
	set_mem(LCDC, 0x91);
	set_mem(SCY, 0x00);
	set_mem(SCX, 0x00);
	set_mem(LYC, 0x00);
	set_mem(BGP, 0xFC);
	set_mem(OBP0, 0xFF);
	set_mem(OBP1, 0xFF);
	set_mem(WY, 0x00);
	set_mem(WX, 0x00);
	set_mem(IE, 0x00);

	return 0;
}

void instruction_cycle(struct gb_state *state) {
	uint16_t div_cycles = 0;
	uint32_t timer_cycles = 0;
	int total_cycles = 0;
	while (1)
	{
		if (tick(state, &total_cycles, &div_cycles, &timer_cycles)) {
			return;
		}
	}
}


void start(uint8_t *bs_mem, uint8_t *cart_mem, long cart_size, int bootstrap_flag) {
	struct gb_state *state = calloc(1, sizeof(struct gb_state));
	gbs = state;
	state->mem = calloc(0x10000, sizeof(uint8_t));
	gb_mem = state->mem;

	memcpy(state->mem, cart_mem, cart_size);
	uint8_t *cart_first256 = calloc(0x100, sizeof(uint8_t));
	memcpy(cart_first256, cart_mem, 0x100);
	if (bootstrap_flag) {
		memcpy(state->mem, bs_mem, 0x100);
	}

	if (power_up(state, bootstrap_flag)) {
		return;
	}

	if (bootstrap_flag) {
		memcpy(state->mem, cart_first256, 0x100);
		free(bs_mem);
	}

	state->pc = 0x100;

	free(cart_first256);
	free(cart_mem);

	print_registers(state);

	instruction_cycle(state);

	print_registers(state);

}

uint8_t *read_file(char *path, long *size) {
	FILE *fp = fopen(path, "rb");

	fseek(fp, 0L, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	uint8_t *bin = calloc(*size, sizeof(uint8_t));
	fread(bin, 1, *size, fp);
	return bin;
}

void at_exit() {
	//printf_debug_op_count();
	fprintf_debug_info(stdout);
	print_memory(gbs);
}

int main(int argc, char **argv) {
	char *bootstrap_path;
	char *cart_path;
	uint8_t bootstrap_flag = 0;
	if (start_display()) {
		return 1;
	}
	int debug_flag = 0;
	int debug_size = 0;
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i],"-c") && i < argc - 1) {
				if (i+1 >= argc) {
					fprintf(stderr, "No argument after -c\n");
					return 1;
				}
				cart_path = argv[++i];
			} else if (!strcmp(argv[i],"-b") && i < argc - 1) {
				if (i+1 >= argc) {
					fprintf(stderr, "No argument after -b\n");
					return 1;
				}
				bootstrap_path = argv[++i];
				bootstrap_flag = 1;
			} else if (!strcmp(argv[i],"-d")) {
				if (i+1 >= argc) {
					fprintf(stderr, "No argument after -d\n");
					return 1;
				}
				debug_size = atoi(argv[++i]);
				debug_flag = 1;
			} else {
				fprintf(stderr, "Illegal argument: %s\n", argv[i]);
				return 1;
			}
		}
	} else {
		fprintf(stderr, "Not enough arguments\n");
		return 1;
	}

	if (debug_flag) {
		init_debug(debug_size);
		atexit(at_exit);
	} else {
		debug_enabled = 0;
	}

	long bs_size = 0;
	long cart_size = 0;
	uint8_t *bs_mem = 0;
	if (bootstrap_flag) {
		bs_mem = read_file(bootstrap_path, &bs_size);
		if (bs_size != 0x100) {
			fprintf(stderr, "Bootstrap excepted to be size of 256 bytes (Actual: %ld)\n", bs_size);
			return 1;
		}
	}
	uint8_t *cart_mem = read_file(cart_path, &cart_size);

	start(bs_mem, cart_mem, cart_size, bootstrap_flag);

	end_display();
	return 0;
}
