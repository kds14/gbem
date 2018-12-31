#include <SDL.h>
#include "display.h"
#include "input.h"

#define WHITE 255
#define LIGHT_GRAY 160
#define DARK_GRAY 80
#define BLACK 0

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

const int SCREEN_WIDTH = 160;
const int SCREEN_HEIGHT = 144;
const char const * WINDOW_TITLE = "GBEM";

const uint8_t colors[4] = {WHITE, LIGHT_GRAY, DARK_GRAY, BLACK};

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
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderPresent(renderer);
		}
	}
	return 0;
}

void draw_pixel(int x, int y, uint8_t color) {
	uint8_t c = colors[color];
	SDL_SetRenderDrawColor(renderer, c, c, c, 0);
	SDL_RenderDrawPoint(renderer, x, y);
}

void display_render() {
	SDL_RenderPresent(renderer);
}

void clear_renderer() {
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);
}

void end_display() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
