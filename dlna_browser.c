/*
 * DLNA Browser - UI Implementation
 * 
 * Browser state machine and rendering for navigating DLNA servers.
 * Inspired by DOSBox Pure's menu system.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#include "include/dlna_browser.h"
#include "include/dlna_draw.h"
#include "include/dlna_client.h"
#include "include/dlna_ssdp.h"
#include <stdio.h>
#include <string.h>

/* Log callback */
extern void (*log_cb)(int level, const char *fmt, ...);

/* Discovery timeout (frames at 60fps) */
#define DISCOVERY_TIMEOUT_FRAMES (60 * 5)  /* 5 seconds */

static int discovery_timer = 0;

/* Initialize browser */
void dlna_browser_init(dlna_browser_t *browser)
{
    memset(browser, 0, sizeof(*browser));
    browser->state = BROWSER_STATE_INIT;
    browser->ui_width = 320;
    browser->ui_height = 240;
    browser->line_height = 10;
    browser->visible_rows = 20;
    dlna_discovery_init(&browser->discovery);
    dlna_client_init(&browser->client);
    strcpy(browser->current_title, "DLNA Browser");
}

/* Set resolution */
void dlna_browser_set_resolution(dlna_browser_t *browser, int width, int height)
{
    browser->ui_width = width;
    browser->ui_height = height;
    
    if (height >= 480) {
        browser->line_height = 14;
        browser->visible_rows = (height - 50) / browser->line_height;
    } else {
        browser->line_height = 10;
        browser->visible_rows = (height - 36) / browser->line_height;
    }
}

/* Start auto-discovery */
void dlna_browser_start_discovery(dlna_browser_t *browser)
{
    browser->state = BROWSER_STATE_DISCOVERING;
    strcpy(browser->current_title, "Searching...");
    discovery_timer = 0;
    
    if (dlna_discovery_start(&browser->discovery)) {
        if (log_cb) log_cb(1, "[DLNA] Started discovery\n");
    } else {
        browser->state = BROWSER_STATE_ERROR;
        strcpy(browser->error_msg, "Failed to start discovery");
    }
}

/* Add manual server */
void dlna_browser_add_manual_server(dlna_browser_t *browser, const char *url)
{
    if (dlna_discovery_add_manual(&browser->discovery, url)) {
        if (log_cb) log_cb(1, "[DLNA] Added manual server: %s\n", url);
    }
}

/* Reset browser */
void dlna_browser_reset(dlna_browser_t *browser)
{
    dlna_discovery_stop(&browser->discovery);
    dlna_client_disconnect(&browser->client);
    
    browser->state = BROWSER_STATE_INIT;
    browser->item_count = 0;
    browser->selection = 0;
    browser->scroll = 0;
    browser->nav_depth = 0;
    browser->play_requested = false;
    browser->selected_url[0] = '\0';
}

/* Update scroll position */
static void update_scroll(dlna_browser_t *browser)
{
    int margin = 2;
    
    if (browser->selection < browser->scroll + margin) {
        browser->scroll = browser->selection - margin;
        if (browser->scroll < 0) browser->scroll = 0;
    }
    
    if (browser->selection >= browser->scroll + browser->visible_rows - margin) {
        browser->scroll = browser->selection - browser->visible_rows + margin + 1;
    }
    
    int max_scroll = browser->item_count - browser->visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (browser->scroll > max_scroll) browser->scroll = max_scroll;
}

/* Build server list as items */
static void build_server_list(dlna_browser_t *browser)
{
    int count;
    dlna_server_t *servers = dlna_discovery_get_servers(&browser->discovery, &count);
    
    browser->item_count = 0;
    
    for (int i = 0; i < count && browser->item_count < DLNA_MAX_ITEMS; i++) {
        if (!servers[i].valid) continue;
        
        dlna_item_t *item = &browser->items[browser->item_count];
        memset(item, 0, sizeof(*item));
        
        item->type = DLNA_ITEM_SERVER;
        strncpy(item->title, servers[i].name, sizeof(item->title) - 1);
        snprintf(item->id, sizeof(item->id), "%d", i);  /* Store index */
        
        browser->item_count++;
    }
}

/* Connect to a server and browse root */
static void connect_to_server(dlna_browser_t *browser, int server_index)
{
    int count;
    dlna_server_t *servers = dlna_discovery_get_servers(&browser->discovery, &count);
    
    if (server_index < 0 || server_index >= count) return;
    
    browser->state = BROWSER_STATE_CONNECTING;
    
    if (dlna_client_connect(&browser->client, &servers[server_index])) {
        browser->state = BROWSER_STATE_LOADING;
        strncpy(browser->current_title, servers[server_index].name, 
                sizeof(browser->current_title) - 1);
        
        /* Browse root container */
        if (dlna_client_browse(&browser->client, "0")) {
            browser->state = BROWSER_STATE_BROWSING;
            browser->nav_depth = 0;
            browser->selection = 0;
            browser->scroll = 0;
            
            /* Copy items from client */
            browser->item_count = browser->client.item_count;
            memcpy(browser->items, browser->client.items, 
                   browser->item_count * sizeof(dlna_item_t));
        } else {
            browser->state = BROWSER_STATE_ERROR;
            strcpy(browser->error_msg, "Failed to browse server");
        }
    } else {
        browser->state = BROWSER_STATE_ERROR;
        strcpy(browser->error_msg, "Failed to connect");
    }
}

/* Browse into a container */
static void browse_container(dlna_browser_t *browser, dlna_item_t *item)
{
    /* Push current to nav stack */
    if (browser->nav_depth < DLNA_NAV_STACK_DEPTH) {
        strncpy(browser->nav_stack[browser->nav_depth].id, 
                browser->client.current_id,
                sizeof(browser->nav_stack[0].id) - 1);
        strncpy(browser->nav_stack[browser->nav_depth].title,
                browser->current_title,
                sizeof(browser->nav_stack[0].title) - 1);
        browser->nav_depth++;
    }
    
    browser->state = BROWSER_STATE_LOADING;
    
    if (dlna_client_browse(&browser->client, item->id)) {
        browser->state = BROWSER_STATE_BROWSING;
        strncpy(browser->current_title, item->title, sizeof(browser->current_title) - 1);
        browser->selection = 0;
        browser->scroll = 0;
        
        /* Copy items from client */
        browser->item_count = browser->client.item_count;
        memcpy(browser->items, browser->client.items,
               browser->item_count * sizeof(dlna_item_t));
    } else {
        browser->state = BROWSER_STATE_ERROR;
        strcpy(browser->error_msg, "Failed to browse");
        browser->nav_depth--;
    }
}

/* Play a media item */
static void play_item(dlna_browser_t *browser, dlna_item_t *item)
{
    if (dlna_client_get_url(&browser->client, item, 
                            browser->selected_url, sizeof(browser->selected_url))) {
        browser->play_requested = true;
        if (log_cb) log_cb(1, "[DLNA] Playing: %s\n", item->title);
    } else {
        browser->state = BROWSER_STATE_ERROR;
        strcpy(browser->error_msg, "No playable URL");
    }
}

/* Input: Up */
void dlna_browser_input_up(dlna_browser_t *browser)
{
    if (browser->item_count > 0) {
        if (browser->selection > 0)
            browser->selection--;
        else
            browser->selection = browser->item_count - 1;
        update_scroll(browser);
    }
}

/* Input: Down */
void dlna_browser_input_down(dlna_browser_t *browser)
{
    if (browser->item_count > 0) {
        if (browser->selection < browser->item_count - 1)
            browser->selection++;
        else
            browser->selection = 0;
        update_scroll(browser);
    }
}

/* Input: Select (A button) */
void dlna_browser_input_select(dlna_browser_t *browser)
{
    if (browser->state == BROWSER_STATE_ERROR) {
        /* Clear error */
        if (browser->nav_depth > 0) {
            browser->nav_depth--;
            browser->state = BROWSER_STATE_BROWSING;
        } else if (browser->client.state == DLNA_CLIENT_CONNECTED) {
            browser->state = BROWSER_STATE_BROWSING;
        } else {
            browser->state = BROWSER_STATE_SERVERS;
            build_server_list(browser);
        }
        return;
    }
    
    if (browser->state == BROWSER_STATE_SERVERS && browser->item_count > 0) {
        /* Select server */
        int server_idx = atoi(browser->items[browser->selection].id);
        connect_to_server(browser, server_idx);
        return;
    }
    
    if (browser->state == BROWSER_STATE_BROWSING && browser->item_count > 0) {
        dlna_item_t *item = &browser->items[browser->selection];
        
        if (dlna_item_is_playable(item->type)) {
            play_item(browser, item);
        } else if (dlna_item_is_browsable(item->type)) {
            browse_container(browser, item);
        }
    }
}

/* Input: Back (B) */
void dlna_browser_input_back(dlna_browser_t *browser)
{
    if (browser->state == BROWSER_STATE_ERROR) {
        dlna_browser_input_select(browser);
        return;
    }
    
    if (browser->state == BROWSER_STATE_BROWSING && browser->nav_depth > 0) {
        /* Go back to parent container */
        browser->nav_depth--;
        dlna_nav_entry_t *entry = &browser->nav_stack[browser->nav_depth];
        
        browser->state = BROWSER_STATE_LOADING;
        
        if (dlna_client_browse(&browser->client, entry->id)) {
            browser->state = BROWSER_STATE_BROWSING;
            strncpy(browser->current_title, entry->title, sizeof(browser->current_title) - 1);
            browser->selection = 0;
            browser->scroll = 0;
            
            browser->item_count = browser->client.item_count;
            memcpy(browser->items, browser->client.items,
                   browser->item_count * sizeof(dlna_item_t));
        } else {
            browser->state = BROWSER_STATE_ERROR;
            strcpy(browser->error_msg, "Failed to go back");
        }
        return;
    }
    
    if (browser->state == BROWSER_STATE_BROWSING && browser->nav_depth == 0) {
        /* Go back to server list */
        dlna_client_disconnect(&browser->client);
        browser->state = BROWSER_STATE_SERVERS;
        strcpy(browser->current_title, "DLNA Servers");
        build_server_list(browser);
        browser->selection = 0;
        browser->scroll = 0;
        return;
    }
}

/* Input: Page Up (L) */
void dlna_browser_input_pageup(dlna_browser_t *browser)
{
    if (browser->item_count > 0) {
        browser->selection -= browser->visible_rows;
        if (browser->selection < 0) browser->selection = 0;
        update_scroll(browser);
    }
}

/* Input: Page Down (R) */
void dlna_browser_input_pagedown(dlna_browser_t *browser)
{
    if (browser->item_count > 0) {
        browser->selection += browser->visible_rows;
        if (browser->selection >= browser->item_count)
            browser->selection = browser->item_count - 1;
        update_scroll(browser);
    }
}

/* Frame update */
void dlna_browser_update(dlna_browser_t *browser)
{
    if (browser->state == BROWSER_STATE_DISCOVERING) {
        /* Process discovery responses */
        dlna_discovery_update(&browser->discovery);
        discovery_timer++;
        
        /* Check for timeout or found servers */
        if (discovery_timer >= DISCOVERY_TIMEOUT_FRAMES) {
            dlna_discovery_stop(&browser->discovery);
            browser->state = BROWSER_STATE_SERVERS;
            strcpy(browser->current_title, "DLNA Servers");
            build_server_list(browser);
            
            if (browser->item_count == 0) {
                browser->state = BROWSER_STATE_ERROR;
                strcpy(browser->error_msg, "No servers found");
            }
        }
    }
}

/* Get icon for item type */
static const char *get_item_prefix(dlna_item_type_t type)
{
    switch (type) {
        case DLNA_ITEM_BACK:       return "< ";
        case DLNA_ITEM_SERVER:     return "[S] ";
        case DLNA_ITEM_CONTAINER:  return "[+] ";
        case DLNA_ITEM_VIDEO:      return "    ";
        case DLNA_ITEM_AUDIO:      return " ~  ";
        case DLNA_ITEM_IMAGE:      return "[I] ";
        case DLNA_ITEM_LOADING:    return "... ";
        case DLNA_ITEM_ERROR:      return "!   ";
        default:                   return "    ";
    }
}

/* Render browser */
void dlna_browser_render(dlna_browser_t *browser, uint32_t *framebuffer)
{
    dlna_buffer_t buf;
    dlna_draw_init(&buf, framebuffer, browser->ui_width, browser->ui_height);
    
    /* Clear background */
    dlna_draw_clear(&buf, DLNA_COL_BG_MENU);
    
    int lh = browser->line_height;
    int header_h = lh + 8;
    int content_y = header_h + 4;
    int footer_y = browser->ui_height - lh - 4;
    
    /* Header */
    dlna_draw_box(&buf, 4, 2, browser->ui_width - 8, header_h,
                  DLNA_COL_BG_HEADER, DLNA_COL_BORDER);
    dlna_draw_text_centered(&buf, 6, browser->current_title, DLNA_COL_TEXT_TITLE);
    
    /* State-specific rendering */
    if (browser->state == BROWSER_STATE_INIT) {
        dlna_draw_text_centered(&buf, browser->ui_height / 2, "Press A to discover", DLNA_COL_TEXT_DIM);
    }
    else if (browser->state == BROWSER_STATE_DISCOVERING || browser->state == BROWSER_STATE_LOADING) {
        dlna_draw_text_centered(&buf, browser->ui_height / 2, "Loading...", DLNA_COL_TEXT_NORMAL);
    }
    else if (browser->state == BROWSER_STATE_ERROR) {
        dlna_draw_text_centered(&buf, browser->ui_height / 2 - lh, "Error", DLNA_COL_TEXT_WARN);
        dlna_draw_text_centered(&buf, browser->ui_height / 2 + lh, browser->error_msg, DLNA_COL_TEXT_DIM);
    }
    else if (browser->state == BROWSER_STATE_SERVERS || browser->state == BROWSER_STATE_BROWSING) {
        /* Draw items */
        int y = content_y;
        int max_text_width = browser->ui_width - 24;
        
        for (int i = browser->scroll; 
             i < browser->item_count && i < browser->scroll + browser->visible_rows;
             i++) {
            dlna_item_t *item = &browser->items[i];
            
            /* Selection highlight */
            if (i == browser->selection) {
                dlna_draw_rect(&buf, 4, y - 1, browser->ui_width - 16, lh + 2,
                              DLNA_COL_BG_SELECTION);
            }
            
            /* Build display string */
            char line[160];
            const char *prefix = get_item_prefix(item->type);
            snprintf(line, sizeof(line), "%s%s", prefix, item->title);
            
            uint32_t col = (i == browser->selection) ? DLNA_COL_TEXT_WHITE : DLNA_COL_TEXT_NORMAL;
            dlna_draw_text_truncated(&buf, 8, y, line, col, max_text_width);
            
            /* Duration for playable */
            if (dlna_item_is_playable(item->type) && item->duration_ms > 0) {
                char dur[16];
                dlna_format_duration(item->duration_ms, dur, sizeof(dur));
                int dur_x = browser->ui_width - 8 - dlna_text_width(dur) - 12;
                dlna_draw_text(&buf, dur_x, y, dur, DLNA_COL_TEXT_DIM);
            }
            
            y += lh;
        }
        
        /* Scrollbar */
        if (browser->item_count > browser->visible_rows) {
            int bar_x = browser->ui_width - 8;
            int bar_y = content_y;
            int bar_h = footer_y - content_y - 4;
            
            dlna_draw_rect(&buf, bar_x, bar_y, 4, bar_h, DLNA_COL_BG_SCROLL);
            
            int thumb_h = bar_h * browser->visible_rows / browser->item_count;
            if (thumb_h < 8) thumb_h = 8;
            int thumb_y = bar_y + (bar_h - thumb_h) * browser->scroll / 
                          (browser->item_count - browser->visible_rows);
            dlna_draw_rect(&buf, bar_x, thumb_y, 4, thumb_h, DLNA_COL_BG_SELECTION);
        }
    }
    
    /* Footer */
    const char *hint;
    if (browser->state == BROWSER_STATE_INIT)
        hint = "A:Discover";
    else if (browser->state == BROWSER_STATE_ERROR)
        hint = "A/B:Back";
    else if (browser->nav_depth > 0 || browser->state == BROWSER_STATE_BROWSING)
        hint = "A:Select B:Back L/R:Page";
    else
        hint = "A:Select L/R:Page";
    
    dlna_draw_text(&buf, 8, footer_y, hint, DLNA_COL_TEXT_DIM);
    
    /* Item count */
    if (browser->item_count > 0) {
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%d/%d",
                 browser->selection + 1, browser->item_count);
        int count_x = browser->ui_width - 8 - dlna_text_width(count_str);
        dlna_draw_text(&buf, count_x, footer_y, count_str, DLNA_COL_TEXT_DIM);
    }
}
