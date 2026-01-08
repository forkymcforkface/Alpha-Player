/*
 * DLNA Browser - UI State Header
 * 
 * Browser state machine and UI for navigating DLNA servers.
 * Inspired by DOSBox Pure's menu system.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#ifndef DLNA_BROWSER_H
#define DLNA_BROWSER_H

#include "dlna_client.h"
#include "dlna_ssdp.h"
#include "dlna_draw.h"
#include <stdbool.h>

/* Maximum navigation depth */
#define DLNA_NAV_STACK_DEPTH 16

/* Browser state */
typedef enum {
    BROWSER_STATE_INIT,          /* Initial state */
    BROWSER_STATE_DISCOVERING,   /* Discovering servers */
    BROWSER_STATE_SERVERS,       /* Showing server list */
    BROWSER_STATE_CONNECTING,    /* Connecting to server */
    BROWSER_STATE_BROWSING,      /* Browsing content */
    BROWSER_STATE_LOADING,       /* Loading content list */
    BROWSER_STATE_PLAYING,       /* Media is playing */
    BROWSER_STATE_ERROR,         /* Error occurred */
} dlna_browser_state_t;

/* Navigation stack entry */
typedef struct {
    char id[DLNA_MAX_ID_LEN];
    char title[64];
} dlna_nav_entry_t;

/* Main browser structure */
typedef struct {
    /* State */
    dlna_browser_state_t state;
    
    /* Discovery */
    dlna_discovery_t discovery;
    
    /* Client for current server */
    dlna_client_t client;
    
    /* Current view items (unified list) */
    dlna_item_t items[DLNA_MAX_ITEMS];
    int item_count;
    
    /* Selection */
    int selection;
    int scroll;
    
    /* Navigation stack */
    dlna_nav_entry_t nav_stack[DLNA_NAV_STACK_DEPTH];
    int nav_depth;
    
    /* Current title */
    char current_title[64];
    
    /* UI settings */
    int ui_width;
    int ui_height;
    int line_height;
    int visible_rows;
    
    /* Error message */
    char error_msg[128];
    
    /* Playback request */
    char selected_url[DLNA_MAX_URL_LEN];
    bool play_requested;
    
} dlna_browser_t;

/* Lifecycle */
void dlna_browser_init(dlna_browser_t *browser);
void dlna_browser_set_resolution(dlna_browser_t *browser, int width, int height);
void dlna_browser_start_discovery(dlna_browser_t *browser);
void dlna_browser_add_manual_server(dlna_browser_t *browser, const char *url);
void dlna_browser_reset(dlna_browser_t *browser);

/* Input handling */
void dlna_browser_input_up(dlna_browser_t *browser);
void dlna_browser_input_down(dlna_browser_t *browser);
void dlna_browser_input_select(dlna_browser_t *browser);
void dlna_browser_input_back(dlna_browser_t *browser);
void dlna_browser_input_pageup(dlna_browser_t *browser);
void dlna_browser_input_pagedown(dlna_browser_t *browser);

/* Frame update */
void dlna_browser_update(dlna_browser_t *browser);

/* Rendering */
void dlna_browser_render(dlna_browser_t *browser, uint32_t *framebuffer);

/* State queries */
static inline bool dlna_browser_is_playing(dlna_browser_t *browser)
{
    return browser->state == BROWSER_STATE_PLAYING;
}

static inline bool dlna_browser_wants_to_play(dlna_browser_t *browser)
{
    return browser->play_requested;
}

static inline const char *dlna_browser_get_selected_url(dlna_browser_t *browser)
{
    return browser->selected_url;
}

static inline void dlna_browser_playback_started(dlna_browser_t *browser)
{
    browser->play_requested = false;
    browser->state = BROWSER_STATE_PLAYING;
}

static inline void dlna_browser_playback_ended(dlna_browser_t *browser)
{
    browser->state = BROWSER_STATE_BROWSING;
}

#endif /* DLNA_BROWSER_H */
