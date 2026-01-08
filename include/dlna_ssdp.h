/*
 * DLNA Browser - SSDP Discovery Header
 * 
 * UPnP Simple Service Discovery Protocol for finding DLNA media servers.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#ifndef DLNA_SSDP_H
#define DLNA_SSDP_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum number of servers to discover */
#define DLNA_MAX_SERVERS 16

/* Server info */
typedef struct {
    char name[128];           /* Friendly name */
    char location[256];       /* Device description URL */
    char control_url[256];    /* ContentDirectory control URL */
    char base_url[128];       /* Base URL for media */
    bool valid;               /* Successfully parsed */
} dlna_server_t;

/* Discovery state */
typedef struct {
    dlna_server_t servers[DLNA_MAX_SERVERS];
    int server_count;
    bool discovery_complete;
    int socket_fd;            /* UDP socket for SSDP */
} dlna_discovery_t;

/* Initialize discovery */
void dlna_discovery_init(dlna_discovery_t *disc);

/* Start SSDP discovery (sends M-SEARCH) */
bool dlna_discovery_start(dlna_discovery_t *disc);

/* Process any incoming responses (non-blocking) */
void dlna_discovery_update(dlna_discovery_t *disc);

/* Stop discovery and close socket */
void dlna_discovery_stop(dlna_discovery_t *disc);

/* Add a server manually from URL */
bool dlna_discovery_add_manual(dlna_discovery_t *disc, const char *location_url);

/* Get server list */
dlna_server_t *dlna_discovery_get_servers(dlna_discovery_t *disc, int *count);

#endif /* DLNA_SSDP_H */
