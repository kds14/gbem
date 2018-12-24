#include "SDL.h"
#include "display.h"

SDL_Window *window = NULL;
//SDL_Surface *screenSurface = NULL;
SDL_Renderer *renderer = NULL;

const int SCREEN_WIDTH = 160;
const int SCREEN_HEIGHT = 144;


int start_display() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
		return 1;
	} else {
		SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer);
		if (window == NULL || renderer == NULL) {
			printf( "Window or renderer could not be created! SDL_Error: %s\n", SDL_GetError() );
			return 1;
		} else {
			SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
			SDL_RenderClear(renderer);
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
			SDL_RenderPresent(renderer);
		}
	}
	return 0;
}

void draw_pixel(int x, int y) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderDrawPoint(renderer, x, y);
}

void display_render() {
	SDL_RenderPresent(renderer);
}

void clear_renderer() {
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);
}

Uint32 frame_time = 0;

void wait_clear_renderer() {
	int wait_time = 16.75L - SDL_GetTicks() + frame_time;
	if (wait_time > 0) {
		SDL_Delay(wait_time);
	}
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);
	frame_time = SDL_GetTicks();
}

void end_display() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

int handle_display_events() {
	SDL_Event event;
	if (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			return 1;
		}
	}
	return 0;
}


