#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <memory.h>

struct cpu_state {
	uint8_t f;
	uint8_t a;
	uint8_t c;
	uint8_t b;
	uint8_t e;
	uint8_t d;
	uint8_t l;
	uint8_t h;
	uint16_t sp;
	uint16_t pc;
	uint8_t *mem;

};

void load8(uint8_t *dest, uint8_t src) {
	memcpy(dest, &src, 1);
}

void print_registers(struct cpu_state *s) {
	printf("A: %02X, B: %02X, C: %02X, D: %02X, E: %02X, F: %02X, H: %02X, L: %02X, SP: %04X, PC: %04X\n", s->a, s->b, s->c, s->d, s->e, s->f, s->h, s->l, s->sp, s->pc);
}

void print_memory(struct cpu_state *s) {
	for (int i = 0x00; i < 0x10000; i+=0x10) {
		printf("%04X\t", i);
		for (int j = i; j < i + 0x10; j++) {
			printf("%02X ", s->mem[j]);
		}
		puts("");
	}
}

void power_up(struct cpu_state *state) {
	// TODO: execute 256 byte program at mem 0
	// TODO: read $104 to $133 and place graphic
	// TODO: compare $103-$133 to table, stop on failure
	// TODO: GB & GB Pocket: add all bytes from $134-$0x14d then add 25, check
	// least sig bit eq to zero
	// TODO: disable internal ROM and begin cartridge exec at $100
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

void start() {
	struct cpu_state *state = calloc(1, sizeof(struct cpu_state));
	state->mem = calloc(0x10000, sizeof(uint8_t));
	print_registers(state);
	print_memory(state);
	power_up(state);
	print_registers(state);
	print_memory(state);
}

int main(int argc, char **argv) {
	start();
	return 0;
}
