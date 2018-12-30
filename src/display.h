#ifndef DISPLAY_H
#define DISPLAY_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int start_display();
void end_display();
void clear_renderer();
int wait_clear_renderer();
void draw_pixel(int x, int y, uint8_t);

void draw_sprite(uint8_t y, uint8_t x, uint8_t *data, size_t height);
void display_render();

#endif
