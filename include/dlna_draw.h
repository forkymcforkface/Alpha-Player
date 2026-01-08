/*
 * DLNA Browser - Drawing Utilities Header
 * 
 * Simple framebuffer drawing functions for the DLNA browser UI.
 * Adapted from DOSBox Pure's DBP_BufferDrawing.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#ifndef DLNA_DRAW_H
#define DLNA_DRAW_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Color constants - DOSBox Pure inspired palette */
#define DLNA_COL_BG_MENU        0xFF1A1E20  /* Dark background */
#define DLNA_COL_BG_SELECTION   0xFF117EB7  /* Blue selection highlight */
#define DLNA_COL_BG_SCROLL      0xFF093F5B  /* Scrollbar background */
#define DLNA_COL_BG_HEADER      0xFF582204  /* Header/title bar */
#define DLNA_COL_BORDER         0xFFFF7126  /* Orange border */
#define DLNA_COL_TEXT_TITLE     0xFFFBD655  /* Yellow title text */
#define DLNA_COL_TEXT_NORMAL    0xFF4DCCF5  /* Cyan normal text */
#define DLNA_COL_TEXT_DIM       0xFF4B7A93  /* Dimmed text */
#define DLNA_COL_TEXT_WHITE     0xFFFFFFFF  /* White text */
#define DLNA_COL_TEXT_WARN      0xFFFF7126  /* Warning text (orange) */

/* Buffer drawing context */
typedef struct dlna_buffer {
    uint32_t *video;    /* Pointer to framebuffer */
    uint32_t width;     /* Buffer width in pixels */
    uint32_t height;    /* Buffer height in pixels */
} dlna_buffer_t;

/* Initialize drawing buffer */
static inline void dlna_draw_init(dlna_buffer_t *buf, uint32_t *video, int w, int h)
{
    buf->video = video;
    buf->width = (uint32_t)w;
    buf->height = (uint32_t)h;
}

/* Clear entire buffer to a color */
static inline void dlna_draw_clear(dlna_buffer_t *buf, uint32_t color)
{
    uint32_t *p = buf->video;
    uint32_t count = buf->width * buf->height;
    while (count--)
        *p++ = color;
}

/* Fill a rectangle with a solid color */
static inline void dlna_draw_rect(dlna_buffer_t *buf, int x, int y, int w, int h, uint32_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)buf->width)  w = (int)buf->width - x;
    if (y + h > (int)buf->height) h = (int)buf->height - y;
    if (w <= 0 || h <= 0) return;
    
    uint32_t *row = buf->video + y * buf->width + x;
    for (int iy = 0; iy < h; iy++, row += buf->width)
    {
        uint32_t *p = row;
        for (int ix = 0; ix < w; ix++)
            *p++ = color;
    }
}

/* Draw a rectangle outline */
static inline void dlna_draw_rect_outline(dlna_buffer_t *buf, int x, int y, int w, int h, uint32_t color)
{
    /* Top and bottom */
    dlna_draw_rect(buf, x, y, w, 1, color);
    dlna_draw_rect(buf, x, y + h - 1, w, 1, color);
    /* Left and right */
    dlna_draw_rect(buf, x, y, 1, h, color);
    dlna_draw_rect(buf, x + w - 1, y, 1, h, color);
}

/* Draw a box with fill and border */
static inline void dlna_draw_box(dlna_buffer_t *buf, int x, int y, int w, int h, 
                                  uint32_t fill_color, uint32_t border_color)
{
    dlna_draw_rect(buf, x + 1, y + 1, w - 2, h - 2, fill_color);
    dlna_draw_rect_outline(buf, x, y, w, h, border_color);
}

/* Get text width in pixels */
static inline int dlna_text_width(const char *text)
{
    return (int)strlen(text) * 8;
}

/* Function declarations - implemented in dlna_draw.c */
void dlna_draw_char(dlna_buffer_t *buf, int x, int y, char c, uint32_t color);
void dlna_draw_text(dlna_buffer_t *buf, int x, int y, const char *text, uint32_t color);
void dlna_draw_text_truncated(dlna_buffer_t *buf, int x, int y, const char *text, uint32_t color, int max_width);
void dlna_draw_text_centered(dlna_buffer_t *buf, int y, const char *text, uint32_t color);

#endif /* DLNA_DRAW_H */
