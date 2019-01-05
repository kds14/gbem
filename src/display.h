#ifndef DISPLAY_H
#define DISPLAY_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NO_PRIORITY 0xFFFF // value that sprite priority can't be

int start_display(int scale_factor);
void end_display();
void clear_renderer();
void draw_pixel(int x, int y, uint8_t c, int bg, int prty, uint16_t sprty);

void display_render();
void finish_row();
void ready_render();

#endif
