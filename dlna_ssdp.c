/*
 * DLNA Browser - SSDP Discovery Implementation
 * 
 * UPnP Simple Service Discovery Protocol for finding DLNA media servers.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#include "include/dlna_ssdp.h"

#include <libavformat/avformat.h>
#include <libavformat/avio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

/* SSDP multicast address and port */
#define SSDP_MULTICAST_ADDR "239.255.255.250"
#define SSDP_PORT 1900

/* SSDP M-SEARCH request for MediaServer devices */
static const char *SSDP_MSEARCH = 
    "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 3\r\n"
    "ST: urn:schemas-upnp-org:device:MediaServer:1\r\n"
    "\r\n";

/* Log callback */
extern void (*log_cb)(int level, const char *fmt, ...);

/* HTTP buffer */
#define HTTP_BUFFER_SIZE (64 * 1024)

/* Simple XML helper to extract value between tags */
static bool xml_get_value(const char *xml, const char *tag, char *out, int max_len)
{
    char open_tag[64], close_tag[64];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);
    
    const char *start = strstr(xml, open_tag);
    if (!start) return false;
    start += strlen(open_tag);
    
    const char *end = strstr(start, close_tag);
    if (!end) return false;
    
    int len = (int)(end - start);
    if (len >= max_len) len = max_len - 1;
    
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

/* Extract base URL from location URL */
static void extract_base_url(const char *location, char *base_url, int max_len)
{
    /* Find the third slash (after http://host:port/) */
    const char *p = location;
    int slashes = 0;
    while (*p && slashes < 3) {
        if (*p == '/') slashes++;
        if (slashes < 3) p++;
    }
    
    int len = (int)(p - location);
    if (len >= max_len) len = max_len - 1;
    strncpy(base_url, location, len);
    base_url[len] = '\0';
}

/* Fetch device description and parse it */
static bool parse_device_description(dlna_server_t *server)
{
    AVIOContext *avio = NULL;
    char *buffer = NULL;
    int ret;
    
    ret = avio_open(&avio, server->location, AVIO_FLAG_READ);
    if (ret < 0) {
        if (log_cb) log_cb(2, "[DLNA] Failed to fetch device description: %s\n", server->location);
        return false;
    }
    
    buffer = (char *)av_malloc(HTTP_BUFFER_SIZE);
    if (!buffer) {
        avio_closep(&avio);
        return false;
    }
    
    int total = 0;
    int bytes;
    while (total < HTTP_BUFFER_SIZE - 1) {
        bytes = avio_read(avio, (unsigned char*)(buffer + total), HTTP_BUFFER_SIZE - 1 - total);
        if (bytes <= 0) break;
        total += bytes;
    }
    buffer[total] = '\0';
    avio_closep(&avio);
    
    /* Parse XML to extract device info */
    xml_get_value(buffer, "friendlyName", server->name, sizeof(server->name));
    
    /* Find ContentDirectory service control URL */
    const char *cd_service = strstr(buffer, "ContentDirectory");
    if (cd_service) {
        /* Look for controlURL after this point */
        char control_path[256] = {0};
        const char *control_start = strstr(cd_service, "<controlURL>");
        if (control_start) {
            control_start += 12;
            const char *control_end = strstr(control_start, "</controlURL>");
            if (control_end) {
                int len = (int)(control_end - control_start);
                if (len > 0 && len < 256) {
                    strncpy(control_path, control_start, len);
                    control_path[len] = '\0';
                }
            }
        }
        
        if (control_path[0]) {
            /* Build full control URL */
            extract_base_url(server->location, server->base_url, sizeof(server->base_url));
            snprintf(server->control_url, sizeof(server->control_url), 
                     "%s%s", server->base_url, control_path);
        }
    }
    
    av_free(buffer);
    
    if (strlen(server->name) == 0) {
        strcpy(server->name, "Unknown Server");
    }
    
    if (strlen(server->control_url) == 0) {
        if (log_cb) log_cb(2, "[DLNA] No ContentDirectory service found\n");
        return false;
    }
    
    server->valid = true;
    if (log_cb) log_cb(1, "[DLNA] Found server: %s\n", server->name);
    
    return true;
}

/* Initialize discovery */
void dlna_discovery_init(dlna_discovery_t *disc)
{
    memset(disc, 0, sizeof(*disc));
    disc->socket_fd = -1;
}

/* Start SSDP discovery */
bool dlna_discovery_start(dlna_discovery_t *disc)
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    
    /* Create UDP socket */
    disc->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (disc->socket_fd < 0) {
        if (log_cb) log_cb(2, "[DLNA] Failed to create socket\n");
        return false;
    }
    
    /* Set non-blocking */
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(disc->socket_fd, FIONBIO, &mode);
#else
    int flags = fcntl(disc->socket_fd, F_GETFL, 0);
    fcntl(disc->socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif
    
    /* Set socket options */
    int reuse = 1;
    setsockopt(disc->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    
    /* Set up multicast destination */
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(SSDP_PORT);
    inet_pton(AF_INET, SSDP_MULTICAST_ADDR, &dest.sin_addr);
    
    /* Send M-SEARCH */
    int sent = sendto(disc->socket_fd, SSDP_MSEARCH, strlen(SSDP_MSEARCH), 0,
                      (struct sockaddr*)&dest, sizeof(dest));
    
    if (sent < 0) {
        if (log_cb) log_cb(2, "[DLNA] Failed to send M-SEARCH\n");
        return false;
    }
    
    if (log_cb) log_cb(1, "[DLNA] Sent SSDP M-SEARCH\n");
    disc->discovery_complete = false;
    
    return true;
}

/* Check if we already have this server */
static bool has_server(dlna_discovery_t *disc, const char *location)
{
    for (int i = 0; i < disc->server_count; i++) {
        if (strcmp(disc->servers[i].location, location) == 0)
            return true;
    }
    return false;
}

/* Process incoming SSDP responses */
void dlna_discovery_update(dlna_discovery_t *disc)
{
    if (disc->socket_fd < 0) return;
    
    char buffer[2048];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    
    /* Try to receive (non-blocking) */
    int received = recvfrom(disc->socket_fd, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr*)&from, &from_len);
    
    if (received <= 0) return;
    
    buffer[received] = '\0';
    
    /* Check if it's a valid response */
    if (strstr(buffer, "HTTP/1.1 200 OK") == NULL) return;
    if (strstr(buffer, "MediaServer") == NULL) return;
    
    /* Extract LOCATION header */
    const char *loc = strstr(buffer, "LOCATION:");
    if (!loc) loc = strstr(buffer, "Location:");
    if (!loc) return;
    
    loc += 9;
    while (*loc == ' ') loc++;
    
    char location[256];
    int i = 0;
    while (*loc && *loc != '\r' && *loc != '\n' && i < 255) {
        location[i++] = *loc++;
    }
    location[i] = '\0';
    
    /* Skip if already have this server */
    if (has_server(disc, location)) return;
    
    /* Add new server */
    if (disc->server_count < DLNA_MAX_SERVERS) {
        dlna_server_t *server = &disc->servers[disc->server_count];
        memset(server, 0, sizeof(*server));
        strncpy(server->location, location, sizeof(server->location) - 1);
        
        /* Fetch and parse device description */
        if (parse_device_description(server)) {
            disc->server_count++;
            if (log_cb) log_cb(1, "[DLNA] Added server: %s (%s)\n", server->name, server->location);
        }
    }
}

/* Stop discovery */
void dlna_discovery_stop(dlna_discovery_t *disc)
{
    if (disc->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(disc->socket_fd);
#else
        close(disc->socket_fd);
#endif
        disc->socket_fd = -1;
    }
    disc->discovery_complete = true;
}

/* Add server manually from URL */
bool dlna_discovery_add_manual(dlna_discovery_t *disc, const char *location_url)
{
    if (disc->server_count >= DLNA_MAX_SERVERS) return false;
    if (has_server(disc, location_url)) return true;
    
    dlna_server_t *server = &disc->servers[disc->server_count];
    memset(server, 0, sizeof(*server));
    strncpy(server->location, location_url, sizeof(server->location) - 1);
    
    if (parse_device_description(server)) {
        disc->server_count++;
        return true;
    }
    
    return false;
}

/* Get server list */
dlna_server_t *dlna_discovery_get_servers(dlna_discovery_t *disc, int *count)
{
    *count = disc->server_count;
    return disc->servers;
}
