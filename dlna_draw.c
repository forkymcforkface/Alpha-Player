/*
 * DLNA Browser - Drawing Utilities Implementation
 * 
 * Text rendering and drawing functions for the DLNA browser UI.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#include "include/dlna_draw.h"
#include "include/dlna_font.h"
#include <string.h>

/* Draw a single character at position (x, y) */
void dlna_draw_char(dlna_buffer_t *buf, int x, int y, char c, uint32_t color)
{
    if (x < 0 || x + 8 > (int)buf->width) return;
    if (y < 0 || y + 8 > (int)buf->height) return;
    
    unsigned char uc = (unsigned char)c;
    const uint8_t *glyph = &dlna_font_8x8[uc * 8];
    
    uint32_t *row = buf->video + y * buf->width + x;
    
    for (int gy = 0; gy < 8; gy++, row += buf->width)
    {
        uint8_t bits = glyph[gy];
        uint32_t *p = row;
        
        for (int gx = 0; gx < 8; gx++, p++)
        {
            if (bits & (0x80 >> gx))
                *p = color;
        }
    }
}

/* Draw a text string at position (x, y) */
void dlna_draw_text(dlna_buffer_t *buf, int x, int y, const char *text, uint32_t color)
{
    if (!text) return;
    
    while (*text)
    {
        if (x + 8 > (int)buf->width) break;
        dlna_draw_char(buf, x, y, *text, color);
        x += 8;
        text++;
    }
}

/* Draw text with maximum width (truncates with "..." if too long) */
void dlna_draw_text_truncated(dlna_buffer_t *buf, int x, int y, const char *text, 
                               uint32_t color, int max_width)
{
    if (!text) return;
    
    int max_chars = max_width / 8;
    int text_len = (int)strlen(text);
    
    if (text_len <= max_chars)
    {
        dlna_draw_text(buf, x, y, text, color);
        return;
    }
    
    int draw_chars = max_chars - 3;
    if (draw_chars < 1) draw_chars = 1;
    
    const char *p = text;
    for (int i = 0; i < draw_chars && *p; i++, p++)
    {
        dlna_draw_char(buf, x, y, *p, color);
        x += 8;
    }
    
    dlna_draw_char(buf, x, y, '.', color); x += 8;
    dlna_draw_char(buf, x, y, '.', color); x += 8;
    dlna_draw_char(buf, x, y, '.', color);
}

/* Draw text centered horizontally on screen */
void dlna_draw_text_centered(dlna_buffer_t *buf, int y, const char *text, uint32_t color)
{
    if (!text) return;
    
    int text_width = dlna_text_width(text);
    int x = ((int)buf->width - text_width) / 2;
    if (x < 0) x = 0;
    
    dlna_draw_text(buf, x, y, text, color);
}
