#include "SDL.h"

SDL_Window *window = NULL;
SDL_Surface *screenSurface = NULL;

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;


int start_display() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
		return 1;
	} else {
		window = SDL_CreateWindow("gbem", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (window == NULL) {
			printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
			return 1;
		} else {
			screenSurface = SDL_GetWindowSurface(window);
			SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0xFF, 0xFF));
			SDL_UpdateWindowSurface(window);
			SDL_Delay(5000);
		}
	}
	return 0;
}

void end_display() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}
