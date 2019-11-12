#ifndef DISPLAY_H
#define DISPLAY_H

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>

// value that sprite priority can't be OAM size is less than 0xFF
#define NO_PRIORITY 0xFFFF

int start_display(int scale_factor);
void end_display();
void clear_renderer();

// prty: sprite priority flag, sprty: sprite priority (see display.c)
void draw_pixel(int x, int y, uint8_t c, int bg, uint8_t rc, int prty, uint16_t sprty);

void display_render();
void finish_row();
void ready_render();

#endif
