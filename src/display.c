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
SDL_Surface *surface = NULL;
SDL_Texture *texture = NULL;
SDL_Renderer *renderer = NULL;

uint32_t colors[4];
//uint8_t pixels[SCREEN_WIDTH];
uint32_t* pixels;

void lock_texture() {
	int pitch;
	SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch);
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
			surface = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_RGBA8888);
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

int count = 0;
void draw_pixel(int x, int y, uint8_t color) {
	count++;
	if (x < 0 || x > SCREEN_WIDTH || y < 0 || y > SCREEN_HEIGHT)
		return;
	pixels[y * SCREEN_WIDTH + x] = colors[color];
	//SDL_RenderDrawPoint(renderer, x, y);
	//uint32_t *p = (uint32_t*)((uint8_t*)surface->pixels + y * surface->pitch + x * sizeof(uint32_t));
	//*p = SDL_MapRGB(surface->format, c, c, c);
}

void finish_row(int y) {
	int x;
	for (x = 0; x < SCREEN_WIDTH; ++x) {
		uint8_t c = pixels[x];
		SDL_SetRenderDrawColor(renderer, c, c, c, 0);
		SDL_RenderDrawPoint(renderer, x, y);
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
	//SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * 4);
	//texture = SDL_CreateTextureFromSurface(renderer, surface);
	//SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
	lock_texture();
	//printf("%d\n", count);
	count = 0;
}


void end_display() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
