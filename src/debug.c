#include "debug.h"

long long opc[0x100];
long long opc_cb[0x100];

struct debug_node {
	uint8_t instruction;
	uint8_t cycles;
	uint16_t pc;
	uint16_t extra;
	uint8_t extra_flag;
	struct debug_node* next;
	struct debug_node* prev;
};

struct debug_info {
	int size;
	int max;
	struct debug_node* head;
	struct debug_node* rear;
};

struct debug_info* dbg = NULL;

void init_debug(int size) {
	debug_enabled = 1;
	dbg = (struct debug_info*)calloc(1, sizeof(struct debug_info));
	dbg->max = size;
	dbg->size = 0;
	dbg->head = NULL;
	dbg->rear = NULL;
}

void printf_debug_op_count() {
	int i;
	printf("OP:\n");
	for (i = 0; i < 0x100; ++i) {
		printf("%02X: %lld\n", i, opc[i]);
	}
	printf("\nOP CB:\n");
	for (i = 0; i < 0x100; ++i) {
		printf("%02X: %lld\n", i, opc_cb[i]);
	}
}

/* 
 * extra_flag = 0 => none
 * = 1 => extra8
 * = 2 => extra 16
 */
void add_debug(uint16_t pc, uint8_t instruction, uint8_t cycles, uint16_t extra, uint8_t extra_flag, int cb) {
	if (cb) {
		opc_cb[instruction]++;
	} else {
		opc[instruction]++;
	}
	struct debug_node* node = (struct debug_node*)calloc(1, sizeof(struct debug_node));
	node->pc = pc;
	node->instruction = instruction;
	node->cycles = cycles;
	node->extra_flag = extra_flag;
	node->extra = extra;

	node->next = NULL;
	node->prev = dbg->rear;
	if (dbg->rear == NULL) {
		dbg->rear = node;
		dbg->head = node;
	} else {
		dbg->rear->next = node;
		dbg->rear = node;
	}
	if (dbg->size++ == dbg->max) {
		struct debug_node* tmp = dbg->head->next;
		free(dbg->head);
		dbg->head = tmp;
		--dbg->size;
	}
}

void fprintf_debug_info(FILE* stream) {
	for (struct debug_node* ptr = dbg->head; ptr != NULL; ptr = ptr->next) {
		if (ptr->extra_flag == 2) {
			fprintf(stream, "%04X\t%02X\t%04X\t(%02X cycles)\n", ptr->pc, ptr->instruction, ptr->extra, ptr->cycles);
		} if (ptr->extra_flag == 1) {
			fprintf(stream, "%04X\t%02X\t%02X\t(%02X cycles)\n", ptr->pc, ptr->instruction, (uint8_t)ptr->extra, ptr->cycles);
		} else {
			fprintf(stream, "%04X\t%02X\t(%02X cycles)\n", ptr->pc, ptr->instruction, ptr->cycles);
		}
	}
}
