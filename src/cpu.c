#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>

#include <time.h>

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
					uint8_t fl : 4;
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

uint8_t rot_left8(uint8_t val) {
	return val << 1 | val >> 7;

}

uint8_t rot_right8(uint8_t val) {
	return val >> 1 | val << 7;

}

void set_left_shift_flags(struct gb_state *state, uint8_t val, uint8_t old7bit) {
	state->fn = 0;
	state->fh = 0;
	if (val == 0) {
		state->fz = 1;
	}
	state->fc = old7bit;

}

void set_right_shift_flags(struct gb_state *state, uint8_t val, uint8_t old0bit) {
	state->fn = 0;
	state->fh = 0;
	if (val == 0) {
		state->fz = 1;
	}
	state->fc = old0bit;

}

void set_add16_flags(struct gb_state *state, uint16_t a, uint16_t b) {
	state->fn = 0;
	state->fh = ((a & 0xfff) + (b & 0xfff)) & 0x100;
	state->fc = ((uint32_t)a + (uint32_t)b) & 0x1000;
}

void set_add8_flags(struct gb_state *state, uint8_t a, uint8_t b, int use_carry) {
	state->fn = 0;
	if (a + b == 0) {
		state->fz = 1;
	}
	state->fh = ((a & 0x0f) + (b & 0x0f)) & 0x10;
	if (use_carry) {
		state->fc = (((uint16_t)a & 0xff) + ((uint16_t)b & 0xff)) & 0x100;
	}
}

void set_dec_flags(struct gb_state *state, uint8_t val) {
	state->fn = 1;
	if (val == 0) {
		state->fz = 1;
	}
	if ((val + 1 >> 4) == 0x00) {
		state->fz = 1;
	}

}

int execute_cb(struct gb_state *state) {
	int pc = state->pc;
	uint8_t *op = &state->mem[pc];
	int cycles = 0;
	switch (*op) {
		case 0x7C:
			state->fz = state->h | 0x80;
			state->fn = 0;
			state->fh = 1;
			state->pc++;
			cycles = 8;
			break;
		default:
			printf("CB %02X instruction not implemented yet\n", state->mem[pc]);
			break;
	}
	return cycles;
}

/*
 * Executes operation in memory at PC. Updates PC reference.
 * Returns number of clock cycles.
 */
int execute(struct gb_state *state) {
	int pc = state->pc;
	uint8_t *op = &state->mem[pc];
	int cycles = 0;
	state->pc++;
	switch (*op) {
		case 0x00:
			/* NOP */
			cycles = 4;
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
			cycles = 4;
			break;
		case 0x05:
			/* DEC B */
			set_dec_flags(state, state->b--);
			cycles = 4;
			break;
		case 0x06:
			/* LD B,n */
			state->b = op[1];
			state->pc++;
			cycles = 8;
			break;
		case 0x07:
			/* RLCA */
			state->a = rot_left8(state->a);
			set_left_shift_flags(state, state->a, state->a >> 7);
			cycles = 4;
			break;
		case 0x08:
			/* LD (nn),SP */
			uint16_t addr = (op[2] << 8) | op[1];
			state->mem[addr] = state->sp;
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
			set_add8_flags(state, state->c, 1, 0);
			state->c++;
			cycles = 4;
			break;
		case 0x0D:
			/* DEC C */
			set_dec_flags(state, state->c--);
			cycles = 4;
			break;
		case 0x0E:
			/* LD C,n */
			state->c = op[1];
			state->pc++;
			cycles = 8;
			break;
		case 0x0F:
			/* RRCA */
			state->fn = 0;
			state->fh = 0;
			state->fc = state->a & 0x01;
			state->a = rot_right8(state->a);
			if (state->a == 0) {
				state->fz = 1;
			}
			cycles = 4;
			break;
		case 0x10:
			/* STOP 0 */
			puts("STOP 0 (0x1000) not implemented\n");
			state->pc++;
			cycles = 4;
		case 0x20:
			// LEFT OFF HERE
			int8_t offset = op[1];
			state->pc++;
			if (state->fz == 0) {
				state->pc += offset;
			}
			break;
		case 0x21:
			/* LD HL,nn */
			state->l = op[1];
			state->h = op[2];
			cycles = 12;
			state->pc += 2;
			break;
		case 0x31:
			/* LD SP,nn */
			uint16_t nn = (op[2] << 8) | op[1];
			state->sp = nn;
			cycles = 12;
			state->pc += 2;
			break;
		case 0x32:
			/* LD (HL-),A */
			state->hl = state->a;
			state->hl--;
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
		case 0xCB:
			cycles = execute_cb(state);
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
	print_memory(state);

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
