#pragma once

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#define die(...) \
	do { \
		printf(__VA_ARGS__); \
		puts(""); \
		exit(1); \
	} while (0)

typedef void (*key_event_t)(char, int);

enum {
	GFX_CTRL = 1,
	GFX_SHIFT,
	GFX_UP,
	GFX_DOWN,
	GFX_LEFT,
	GFX_RIGHT,
	GFX_ESC,
};

void gfx_init(unsigned int _window_width, unsigned int _window_height);
void gfx_draw();
int gfx_update(key_event_t key_event_cb);
void gfx_set_pixel(int x, int y, uint32_t color);
void gfx_destroy();

