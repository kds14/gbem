#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "display.h"
#include "debug.h"
#include "cpu.h"
#include "mem.h"

uint8_t *read_file(char *path, long *size) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "Failed to open file %s\n", path);
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	uint8_t *bin = calloc(*size, sizeof(uint8_t));
	fread(bin, 1, *size, fp);
	return bin;
}

void at_exit_debug() {
	fprintf_debug_info(stdout);
	print_mem();
}

int main(int argc, char **argv) {
	char *bootstrap_path = NULL;
	char *cart_path = NULL;
	uint8_t bootstrap_flag = 0;
	int scale_factor = 2;
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
			} else if (!strcmp(argv[i],"-s") && i < argc - 1) {
				if (i+1 >= argc) {
					fprintf(stderr, "No argument after -s\n");
					return 1;
				}
				scale_factor = atoi(argv[++i]);
			} else {
				fprintf(stderr, "Illegal argument: %s\n", argv[i]);
				return 1;
			}
		}
	} else {
		fprintf(stderr, "Not enough arguments\n");
		return 1;
	}
	if (!cart_path) {
		fprintf(stderr, "No -c argument specified.\n");
		return 1;
	}

	if (debug_flag) {
		init_debug(debug_size);
		atexit(at_exit_debug);
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

	if (!cart_mem || start_display(scale_factor)) {
		return 1;
	}
	setup_mem_banks(cart_mem);
	start(bs_mem, cart_mem, bootstrap_flag);

	end_display();
	return 0;
}
