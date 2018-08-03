#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>

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
	uint8_t *mem;

};

void set_add16_flags(struct gb_state *state, uint16_t a, uint16_t b) {
	state->fn = 0;
	state->fh = ((a & 0xfff) + (b & 0xfff)) & 0x100;
	state->fc = ((uint32_t)a + (uint32_t)b) & 0x1000;
}

void set_add8_flags(struct gb_state *state, uint8_t a, uint8_t b, int use_carry) {
	state->fn = 0;
	state->fz = !(a + b);
	state->fh = ((a & 0x0f) + (b & 0x0f)) & 0x10;
	if (use_carry) {
		state->fc = (((uint16_t)a & 0xff) + ((uint16_t)b & 0xff)) & 0x100;
	}
}

/* a - b */
void set_sub8_flags(struct gb_state *state, uint8_t a, uint8_t b, int use_carry) {
	state->fn = 1;
	state->fz = a == b;
	state->fh = (a & 0x0f) >= (b & 0x0f);
	if (use_carry) {
		state->fc = a >= b;
	}
}

int execute_cb(struct gb_state *state) {
	int pc = state->pc;
	uint8_t *op = &state->mem[pc];
	int cycles = 8;
	state->pc++;
	switch (*op) {
		case 0x7C:
			state->fz = state->h | 0x80;
			state->fn = 0;
			state->fh = 1;
			break;
		default:
			printf("CB %02X instruction not implemented yet\n", state->mem[pc]);
			break;
	}
	return cycles;
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
	set_sub8_flags(state, state->a, state->b, 1);
}

void pop(struct gb_state *state, uint16_t *dest) {
	memcpy(dest, &state->mem[state->sp], 2);
	state->sp++;
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
	push(state, state->pc - 1);
	jump(state, 1, val);
}

/*
 * Executes operation in memory at PC. Updates PC reference.
 * Returns number of clock cycles.
 */
int execute(struct gb_state *state) {
	int pc = state->pc;
	uint8_t *op = &state->mem[pc];
	int cycles = 4;
	state->pc++;
	uint16_t nn = (op[2] << 8) | op[1];
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
			state->mem[state->bc] = state->a;
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
			break;
		case 0x06:
			/* LD B,n */
			cycles = load8val2reg(state, &state->b, op[1]);
			break;
		case 0x07:
			/* RLCA */
			state->fc = state->a >> 7;
			state->a = state->a << 1 | state->fc;
			state->fz = 0;
			state->fh = 0;
			state->fn = 0;
			break;
		case 0x08:
			/* LD (nn),SP */
			state->mem[nn] = state->sp;
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
			state->fc = state->a & 0x01;
			state->a = state->fc << 7 | state->a >> 1;
			state->fz = 0;
			state->fn = 0;
			state->fh = 0;
			break;
		case 0x10:
			/* STOP 0 */
			puts("STOP 0 (0x1000) not implemented\n");
			state->pc++;
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
			state->mem[state->de] = state->a;
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
			uint8_t bit0 = state->fc;
			state->fc = state->a >> 7;
			state->a = (state->a << 1) | bit0;
			state->fn = 0;
			state->fz = 0;
			state->fh = 0;
			break;
		case 0x18:
			/* JR n */
			state->pc += 1 + op[1];
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
			uint8_t bit7 = state->fc << 7;
			state->fc = state->a & 0x01;
			state->a = bit7 | state->a >> 1;
			state->fn = 0;
			state->fz = 0;
			state->fh = 0;
			break;
		case 0x20:
			/* JR NZ,n */
			if (!state->fz) {
				state->pc += 1 + op[1];
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
			state->mem[state->hl] = state->a;
			cycles = 8;
			state->hl++;
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
			if (state->fn) {
				if (state->fc) {
					state->a -= 0x60;
				}
				if (state->fh) {
					state->a -= 0x06;
				}
			} else {
				if ((state->a & 0xff) > 0x99 || state->fh) {
					state->a += 0x60;
					state->fc = 1;
				}
				if ((state->a & 0x0f) > 0x09 || state->fc) {
					state->a += 0x06;
				}
			}
			state->fz = state->a == 0;
			state->fh = 0;
			break;
		case 0x28:
			/* JR Z,n */
			if (state->fz) {
				state->pc += 1 + op[1];
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
			state->a = state->mem[state->hl];
			cycles = 8;
			state->hl++;
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
			if (!state->fc) {
				state->pc += 1 + op[1];
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
			state->mem[state->hl] = state->a;
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
			set_sub8_flags(state, state->mem[state->hl]++, 1, 0);
			break;
		case 0x36:
			/* LD (HL),n */
			state->mem[state->hl] = op[1];
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
			if (state->fc) {
				state->pc += 1 + op[1];
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
			state->mem[state->hl] = state->b;
			cycles = 8;
			break;
		case 0x71:
			/* LD (HL),C */
			state->mem[state->hl] = state->c;
			cycles = 8;
			break;
		case 0x72:
			/* LD (HL),D */
			state->mem[state->hl] = state->d;
			cycles = 8;
			break;
		case 0x73:
			/* LD (HL),E */
			state->mem[state->hl] = state->e;
			cycles = 8;
			break;
		case 0x74:
			/* LD (HL),H */
			state->mem[state->hl] = state->h;
			cycles = 8;
			break;
		case 0x75:
			/* LD (HL),L */
			state->mem[state->hl] = state->l;
			cycles = 8;
			break;
		case 0x76:
			/* HALT */
			// TODO: implement halt
			break;
		case 0x77:
			/* LD (HL),A */
			state->mem[state->hl] = state->a;
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
			addA(state, state->b + state->fc);
			break; 
		case 0x89:
			/* ADC A,C */
			addA(state, state->c + state->fc);
			break; 
		case 0x8A:
			/* ADC A,D */
			addA(state, state->d + state->fc);
			break; 
		case 0x8B:
			/* ADC A,E */
			addA(state, state->e + state->fc);
			break; 
		case 0x8C:
			/* ADC A,H */
			addA(state, state->h + state->fc);
			break; 
		case 0x8D:
			/* ADC A,L */
			addA(state, state->l + state->fc);
			break; 
		case 0x8E:
			/* ADC A,(HL) */
			addA(state, state->mem[state->hl] + state->fc);
			cycles = 8;
			break; 
		case 0x8F:
			/* ADC A,A */
			addA(state, state->a + state->fc);
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
			subA(state, state->b + state->fc);
			break; 
		case 0x99:
			/* SBC A,C */
			subA(state, state->c + state->fc);
			break; 
		case 0x9A:
			/* SBC A,D */
			subA(state, state->d + state->fc);
			break; 
		case 0x9B:
			/* SBC A,E */
			subA(state, state->e + state->fc);
			break; 
		case 0x9C:
			/* SBC A,H */
			subA(state, state->h + state->fc);
			break; 
		case 0x9D:
			/* SBC A,L */
			subA(state, state->l + state->fc);
			break; 
		case 0x9E:
			/* SBC A,(HL) */
			subA(state, state->mem[state->hl] + state->fc);
			cycles = 8;
			break; 
		case 0x9F:
			/* SBC A,A */
			subA(state, state->a + state->fc);
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
			state->pc += 2;
			jump(state, !state->fz, nn);
			cycles = 12;
			break;
		case 0xC3:
			/* JP nn */
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
			addA(state, op[1] + state->fc);
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
			// TODO: ENABLE INTERRUPTS
			ret(state, 1);
			cycles = 8;
			break;
		case 0xDA:
			/* JP C,nn */
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
			subA(state, op[1] + state->fc);
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
			state->mem[op[1] + 0xFF00] = state->a;
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
			state->mem[state->c + 0xFF00] = state->a;
			cycles = 8;
			state->pc++;
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
			state->pc = state->hl;
			break;
		case 0xEA:
			/* LD (nn),A */
			state->mem[nn] = state->a;
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
			state->a = state->mem[op[1] + 0xFF00];
			cycles = 12;
			state->pc++;
			break;
		case 0xF1:
			/* POP AF */
			pop(state, &state->af);
			cycles = 12;
			break;
		case 0xF2:
			/* LD A,(C+$FF00) */
			state->a = state->mem[state->c + 0xFF00];
			cycles = 8;
			state->pc++;
			break;
		case 0xF3:
			/* DI */
			// TODO: Implement disable interrupts
			break;
		case 0xF5:
			/* PUSH AF */
			cycles = 16;
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
			// TODO: Implement enable interrupts
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
			printf("Not implemented yet\n");
			break;
	};
	return cycles;
}


void print_registers(struct gb_state *state) {
	printf("A: %02X, B: %02X, C: %02X, D: %02X, E: %02X, F: %02X, H: %02X, L: %02X, SP: %04X, PC: %04X\n", state->a, state->b, state->c, state->d, state->e, state->f, state->h, state->l, state->sp, state->pc);
	printf("FLAGS: Z:%d N:%d H:%d C:%d None:%d\n", state->fz, state->fn, state->fh, state->fc, state->fl );
}

void print_memory(struct gb_state *s) {
	int column_length = 0x40;
	for (int i = 0x00; i < 0x10000; i+= column_length) {
		printf("%04X\t", i);
		for (int j = i; j < i + column_length; j++) {
			printf("%02X ", s->mem[j]);
		}
		puts("");
	}
}

void run_bootstrap(struct gb_state *state) {
	while (state->mem[0xFF50] != 0x01 && state->pc < 0x100) {
		execute(state);
	}
}

void power_up(struct gb_state *state) {
	state->pc = 0x0000;
	run_bootstrap(state);

	// potentially wrong find other docs
	state->a = 0x01;
	state->f = 0xB0;
	state->b = 0x00;
	state->c = 0x13;
	state->d = 0x00;
	state->e = 0xD8;
	state->h = 0x01;
	state->l = 0x4D;
	state->sp = 0xFFFE;
	state->mem[0xFF05] = 0x00;
	state->mem[0xFF06] = 0x00;
	state->mem[0xFF07] = 0x00;
	state->mem[0xFF10] = 0x80;
	state->mem[0xFF11] = 0xBF;
	state->mem[0xFF12] = 0xF3;
	state->mem[0xFF14] = 0xBF;
	state->mem[0xFF16] = 0x3F;
	state->mem[0xFF17] = 0x00;
	state->mem[0xFF19] = 0xBF;
	state->mem[0xFF1A] = 0x7F;
	state->mem[0xFF1B] = 0xFF;
	state->mem[0xFF1C] = 0x9F;
	state->mem[0xFF1E] = 0xBF;
	state->mem[0xFF20] = 0xFF;
	state->mem[0xFF21] = 0x00;
	state->mem[0xFF22] = 0x00;
	state->mem[0xFF23] = 0xBF;
	state->mem[0xFF24] = 0x77;
	state->mem[0xFF25] = 0xF3;
	state->mem[0xFF26] = 0xF1;
	state->mem[0xFF40] = 0x91;
	state->mem[0xFF42] = 0x00;
	state->mem[0xFF43] = 0x00;
	state->mem[0xFF45] = 0x00;
	state->mem[0xFF47] = 0xFC;
	state->mem[0xFF48] = 0xFF;
	state->mem[0xFF49] = 0xFF;
	state->mem[0xFF4A] = 0x00;
	state->mem[0xFF4B] = 0x00;
	state->mem[0xFFFF] = 0x00;
}


void start(uint8_t *bs_mem, uint8_t *cart_mem, long cart_size) {
	struct gb_state *state = calloc(1, sizeof(struct gb_state));
	state->mem = calloc(0x10000, sizeof(uint8_t));

	memcpy(state->mem, cart_mem, cart_size);
	uint8_t *cart_first256 = calloc(0x100, sizeof(uint8_t));
	memcpy(cart_first256, cart_mem, 0x100);
	memcpy(state->mem, bs_mem, 0x100);

	print_registers(state);

	power_up(state);
	memcpy(state->mem, cart_first256, 0x100);

	free(cart_first256);
	free(bs_mem);
	free(cart_mem);

	print_registers(state);
	//print_memory(state);

	// normal fetch-execute from here
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

int main(int argc, char **argv) {
	char *bootstrap_path;
	char *cart_path;
	if (argc > 1) {
		for (int i = 1; i < argc; i++) {
			if (!strcmp(argv[i],"-c") && i < argc - 1) {
				cart_path = argv[++i];
			} else if (!strcmp(argv[i],"-b") && i < argc - 1) {
				bootstrap_path = argv[++i];
			} else {
				printf("Illegal argument: %s", argv[i]);
				exit(1);
			}
		}
	} else {
		puts("Not enough arguments");
		exit(1);
	}

	long bs_size = 0;
	long cart_size = 0;
	uint8_t *bs_mem = read_file(bootstrap_path, &bs_size);
	if (bs_size != 0x100) {
		printf("Bootstrap excepted to be size of 256 bytes (Actual: %ld)", bs_size);
		exit(1);
	}
	uint8_t *cart_mem = read_file(cart_path, &cart_size);

	start(bs_mem, cart_mem, cart_size);
	return 0;
}
