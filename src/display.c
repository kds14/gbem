#include <SDL.h>
#include "display.h"
#include "input.h"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define WHITE 255
#define LIGHT_GRAY 160
#define DARK_GRAY 80
#define BLACK 0

const char * const WINDOW_TITLE = "GBEM";
SDL_Window *window = NULL;
SDL_Texture *texture = NULL;
SDL_Renderer *renderer = NULL;

uint32_t colors[4];
uint32_t* pixels;

/*
 * sprite priority is 2 bytes XXOO
 * where XX is the x position and OO is OAM ordering
 */
uint16_t priority[SCREEN_WIDTH * SCREEN_HEIGHT];

/*
 * If coordinate has a background pixel (even if written over).
 * For hiding priority 1 sprites.
 */
uint8_t bgf[SCREEN_WIDTH * SCREEN_HEIGHT];

void clear_texture() {
	int i;
	for (i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
		pixels[i] = colors[0];
}

void lock_texture() {
	int pitch;
	memset(&priority[0], 0, sizeof(uint16_t) * SCREEN_WIDTH * SCREEN_HEIGHT);
	memset(&bgf[0], 0, SCREEN_WIDTH * SCREEN_HEIGHT);
	SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);
	clear_texture();
}

int start_display(int scale_factor) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
		return 1;
	} else {
		SDL_CreateWindowAndRenderer(SCREEN_WIDTH * scale_factor, SCREEN_HEIGHT * scale_factor, 0, &window, &renderer);
		if (window == NULL || renderer == NULL) {
			printf( "Window or renderer could not be created! SDL_Error: %s\n", SDL_GetError() );
			return 1;
		} else {
			SDL_RenderSetScale(renderer, scale_factor, scale_factor);
			SDL_SetWindowTitle(window, WINDOW_TITLE);
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
			SDL_RenderClear(renderer);
			texture = SDL_CreateTexture(renderer, SDL_GetWindowPixelFormat(window), SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

			SDL_PixelFormat* format = SDL_AllocFormat(SDL_GetWindowPixelFormat(window));

			colors[0] = SDL_MapRGB(format, WHITE, WHITE, WHITE);
			colors[1] = SDL_MapRGB(format, LIGHT_GRAY, LIGHT_GRAY, LIGHT_GRAY);
			colors[2] = SDL_MapRGB(format, DARK_GRAY, DARK_GRAY, DARK_GRAY);
			colors[3] = SDL_MapRGB(format, BLACK, BLACK, BLACK);

			lock_texture();
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderPresent(renderer);
		}
	}
	return 0;
}

/*
 * If pixel from texture pixel mapping is color[0] or not set
 */
int empty_pixel(int idx) {
	uint32_t p = pixels[idx];
	return p != colors[1] && p != colors[2] && p != colors[3];
}

void draw_pixel(int x, int y, uint8_t color, int bg, int prty, uint16_t sprty) {
	if (x < 0 || x > SCREEN_WIDTH || y < 0 || y > SCREEN_HEIGHT)
		return;

	int idx = SCREEN_WIDTH * y + x;

	/*
	 * bg pixels are always written because they are drawn first
	 */
	if (bg) {
		pixels[idx] = colors[color];
		bgf[idx] = !color;
		priority[idx] = NO_PRIORITY;
	}
	/*
	 * A non-bg pixel that isn't a priority 1 sprite hidden behind a bg
	 * is written if there is no pixel (color[0] or not written yet)
	 * or the sprite has higher priority than a previously
	 * placed sprite (based on X position then OAM ordering). 
	 */
	else if (empty_pixel(idx) ||  ((sprty < priority[idx]) && !(prty && bgf[idx]))) {
		priority[idx] = sprty;
		pixels[idx] = colors[color];
	}
}

void clear_renderer() {
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);
}

void ready_render() {
	SDL_UnlockTexture(texture);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_SetRenderTarget(renderer, texture);
}

void display_render() {
	SDL_RenderPresent(renderer);
	lock_texture();
}


void end_display() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
