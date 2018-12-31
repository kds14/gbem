#include "input.h"

#define P10 0x1
#define P11 0x2
#define P12 0x4
#define P13 0x8

uint8_t p14 = 0;
uint8_t p15 = 0;

uint8_t request_input(int r) {
	return r ? p15 : p14;
}

void handle_events() {
	SDL_Event e;
	if (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) {
			exit(0);
		} else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
			int ks = e.type == SDL_KEYDOWN;
			switch (e.key.keysym.sym) {
				case SDLK_RIGHT:
					p14 = ks ? p14 | P10 : p14 & 0x0E;
					break;
				case SDLK_LEFT:
					p14 = ks ? p14 | P11 : p14 & 0x0D;
					break;
				case SDLK_UP:
					p14 = ks ? p14 | P12 : p14 & 0x0B;
					break;
				case SDLK_DOWN:
					p14 = ks ? p14 | P13 : p14 & 0x07;
					break;
				case SDLK_z:
					p15 = ks ? p15 | P10 : p15 & 0x0E;
					break;
				case SDLK_x:
					p15 = ks ? p15 | P11 : p15 & 0x0D;
					break;
				case SDLK_RSHIFT:
				case SDLK_LSHIFT:
					p15 = ks ? p15 | P12 : p15 & 0x0B;
					break;
				case SDLK_RETURN:
					p15 = ks ? p15 | P13 : p15 & 0x07;
					break;
			}
		}
	}
	printf("%02X %02X\n", p14, p15);
}
