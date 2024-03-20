/* File: gl.c
 * ----------
 * This file implements graphics utility outlined in `gl.h`
 */
#include "gl.h"
#include "font.h"
#include "strings.h"

static int max(int a, int b) {
    return a >= b ? a : b;
}

static int min(int a, int b) {
    return a < b ? a : b;
}

bool is_valid(int x, int y, int width, int height) {
    // helper function that checks if `(x, y)` is within the bound of the framebuffer with dimension `width` times `height`; 1 if yes, 0 if no
    return x >= 0 && x < width && y >= 0 && y < height;
}

bool bound(int *x, int *y, int *w, int *h, int width, int height) {
    // helper function that brings the rectangle identified by (x, y, w, h) into the larger rectangle (framebuffer) of dimension `width` times `height`; if possible, return true; else return false
    int x1 = *x, y1 = *y;
    int x2 = x1 + *w - 1, y2 = y1 + *h - 1;    
    bool f1 = is_valid(x1, y1, width, height);
    bool f2 = is_valid(x2, y2, width, height);
    bool f3 = is_valid(x2, y1, width, height);
    bool f4 = is_valid(x1, y2, width, height);
    bool f5 = (x1 < 0 && x2 >= width && y1 < 0 && y2 >= width); // true if rectangle to be drawn contains the region
    if (!f1 && !f2 && !f3 && !f4 && !f5) { // no intersection; cannot draw anything
                                                                                        return false;
    } 
    if (f1 && f2 && f3 && f4) { // no action necessary; whole rectangle inside
        return true; 
    } 
    // bring (x1, y1) and (x2, y2) into bound
    x1 = max(x1, 0); x2 = max(x2, 0);
    x1 = min(x1, width - 1); x2 = min(x2, width - 1);
    y1 = max(y1, 0); y2 = max(y2, 0);
    y1 = min(y1, height - 1); y2 = min(y2, height - 1);
    
    // update variables accordingly
    *w = x2 - x1 + 1; 
    *h = y2 - y1 + 1;
    *x = x1; 
    *y = y1;

    return true;
}

void gl_init(int width, int height, gl_mode_t mode) {
    fb_init(width, height, mode);
}

int gl_get_width(void) {
    return fb_get_width();
}

int gl_get_height(void) {
    return fb_get_height();
}

color_t gl_color(unsigned char r, unsigned char g, unsigned char b) {
    color_t res = 0xff * (unsigned int)(1 << 24) + r * (unsigned int)(1 << 16) + g * (unsigned int)(1 << 8) + b;
    return res;
}

void gl_swap_buffer(void) {
    fb_swap_buffer();
}

void gl_clear(color_t c) {
    int width = gl_get_width(), height = gl_get_height();
    color_t (*fb)[width] = fb_get_draw_buffer();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            fb[y][x] = c;
        }
    }
}

void gl_draw_pixel(int x, int y, color_t c) {
    int width = gl_get_width(), height = gl_get_height();
    if (!is_valid(x, y, width, height)) { // out of bounds
        return;
    }    
    color_t (*fb)[width] = fb_get_draw_buffer();
    fb[y][x] = c;
}

color_t gl_read_pixel(int x, int y) {
    int width = gl_get_width(), height = gl_get_height();
    if (!is_valid(x, y, width, height)) { // out of bounds
        return 0;
    }    
    color_t (*fb)[width] = fb_get_draw_buffer();
    return fb[y][x];
}

void gl_draw_rect(int x, int y, int w, int h, color_t c) {
    int width = gl_get_width(), height = gl_get_height();
    if (!bound(&x, &y, &w, &h, width, height)) {
        return;
    }
    color_t (*fb)[width] = fb_get_draw_buffer();
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            fb[j][i] = c;
        }
    }
}

void gl_draw_char(int x, int y, char ch, color_t c) {
    int x0 = x, y0 = y, w = font_get_glyph_width(), h = font_get_glyph_height(), width = gl_get_width(), height = gl_get_height();
    unsigned char buf[h * w]; 
    int bufsize = sizeof(buf);
    if (!font_get_glyph(ch, buf, bufsize)) {
        return;
    }
    unsigned char (*im)[w] = (void *)buf;
    if (!bound(&x, &y, &w, &h, width, height)) {
        return;
    }
    int x1 = x - x0; // x can only increase; otherwise whole rectangle does not intersect with the region; x1 < font_width guaranteed
    int y1 = y - y0; // y1 < font_height guaranteed
    // x1, y1 starting positions for im
                   
    color_t (*fb)[width] = fb_get_draw_buffer();
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (im[y1 + j][x1 + i] == 0xff) {
                fb[y + j][x + i] = c; 
            }
        }
    }
}

void gl_draw_string(int x, int y, const char* str, color_t c) {
    int n = strlen(str);
    for (int i = 0; i < n; i++) {
        gl_draw_char(x + i * gl_get_char_width(), y, str[i], c);
    }
}

void gl_draw_line(int x1, int y1, int x2, int y2, color_t c) {
    // For final project: will only implement vertical / horizontal line
    int width = gl_get_width(), height = gl_get_height();
    color_t (*fb)[gl_get_width()] = fb_get_draw_buffer();
    if (x1 == x2) {
        for (int y = min(y1, y2); y <= max(y1, y2); y++) {
            if (y >= 0 && y < height && x1 >= 0 && x1 < width) {
                fb[y][x1] = c;
            }
        } 
    }
    else if (y1 == y2) {
       for (int x = min(x1, x2); x <= max(x1, x2); x++) {
           if (y1 >= 0 && y1 < height && x >= 0 && x < width) {
               fb[y1][x] = c;
           }
       } 
    }
}

int gl_get_char_height(void) {
    return font_get_glyph_height();    
}

int gl_get_char_width(void) {
    return font_get_glyph_width();
}
