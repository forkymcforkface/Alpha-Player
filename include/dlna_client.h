/*
 * DLNA Browser - Client Header
 * 
 * UPnP ContentDirectory service client for browsing DLNA media servers.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#ifndef DLNA_CLIENT_H
#define DLNA_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "dlna_ssdp.h"

/* Item limits */
#define DLNA_MAX_ITEMS      128
#define DLNA_MAX_TITLE_LEN  128
#define DLNA_MAX_URL_LEN    512
#define DLNA_MAX_ID_LEN     64

/* DLNA item types */
typedef enum {
    DLNA_ITEM_NONE = 0,
    DLNA_ITEM_BACK,         /* ".." go back */
    DLNA_ITEM_SERVER,       /* Media server */
    DLNA_ITEM_CONTAINER,    /* Folder/container */
    DLNA_ITEM_VIDEO,        /* Video file */
    DLNA_ITEM_AUDIO,        /* Audio file */
    DLNA_ITEM_IMAGE,        /* Image file */
    DLNA_ITEM_LOADING,      /* Loading placeholder */
    DLNA_ITEM_ERROR,        /* Error message */
} dlna_item_type_t;

/* A single item from DLNA server */
typedef struct {
    dlna_item_type_t type;
    char id[DLNA_MAX_ID_LEN];           /* Object ID for browsing */
    char parent_id[DLNA_MAX_ID_LEN];    /* Parent container ID */
    char title[DLNA_MAX_TITLE_LEN];     /* Display title */
    char url[DLNA_MAX_URL_LEN];         /* Stream URL (for playable items) */
    int duration_ms;                     /* Duration in milliseconds */
    int64_t size_bytes;                  /* File size */
} dlna_item_t;

/* Client connection state */
typedef enum {
    DLNA_CLIENT_DISCONNECTED,
    DLNA_CLIENT_CONNECTING,
    DLNA_CLIENT_CONNECTED,
    DLNA_CLIENT_ERROR,
} dlna_client_state_t;

/* DLNA client */
typedef struct {
    dlna_server_t *server;              /* Currently connected server */
    dlna_client_state_t state;
    char error_msg[128];
    
    /* Current browse results */
    dlna_item_t items[DLNA_MAX_ITEMS];
    int item_count;
    int total_matches;                  /* Total items in container */
    
    /* Current container info */
    char current_id[DLNA_MAX_ID_LEN];   /* Current container ID ("0" = root) */
    char current_title[DLNA_MAX_TITLE_LEN];
} dlna_client_t;

/* Initialize client */
void dlna_client_init(dlna_client_t *client);

/* Connect to a DLNA server */
bool dlna_client_connect(dlna_client_t *client, dlna_server_t *server);

/* Browse a container (use "0" for root) */
bool dlna_client_browse(dlna_client_t *client, const char *container_id);

/* Get stream URL for an item */
bool dlna_client_get_url(dlna_client_t *client, dlna_item_t *item, char *url_out, int max_len);

/* Disconnect from server */
void dlna_client_disconnect(dlna_client_t *client);

/* Format duration as "HH:MM:SS" or "MM:SS" */
void dlna_format_duration(int duration_ms, char *out, int out_len);

/* Check if item is playable */
static inline bool dlna_item_is_playable(dlna_item_type_t type)
{
    return type == DLNA_ITEM_VIDEO || 
           type == DLNA_ITEM_AUDIO;
}

/* Check if item is browsable */
static inline bool dlna_item_is_browsable(dlna_item_type_t type)
{
    return type == DLNA_ITEM_CONTAINER ||
           type == DLNA_ITEM_SERVER ||
           type == DLNA_ITEM_BACK;
}

#endif /* DLNA_CLIENT_H */
