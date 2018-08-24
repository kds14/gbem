#ifndef DISPLAY_H
#define DISPLAY_H

int start_display();
void end_display();
void draw_pixel(int x, int y);

void draw_sprite(uint8_t y, uint8_t x, uint8_t *data, size_t height);
void display_render();
int handle_display_events();

#endif
