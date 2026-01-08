/*
 * DLNA Browser - Client Implementation
 * 
 * UPnP ContentDirectory service client for browsing DLNA servers.
 * 
 * Copyright (C) 2024
 * License: GPL-2.0
 */

#include "include/dlna_client.h"

#include <libavformat/avformat.h>
#include <libavformat/avio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Log callback */
extern void (*log_cb)(int level, const char *fmt, ...);

/* HTTP buffer size */
#define HTTP_BUFFER_SIZE (256 * 1024)

/* SOAP envelope for Browse request */
static const char *SOAP_BROWSE_TEMPLATE = 
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
    "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
    "<s:Body>"
    "<u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
    "<ObjectID>%s</ObjectID>"
    "<BrowseFlag>BrowseDirectChildren</BrowseFlag>"
    "<Filter>*</Filter>"
    "<StartingIndex>0</StartingIndex>"
    "<RequestedCount>200</RequestedCount>"
    "<SortCriteria></SortCriteria>"
    "</u:Browse>"
    "</s:Body>"
    "</s:Envelope>";

/* Simple XML helpers */
static const char *xml_find_tag(const char *xml, const char *tag)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "<%s", tag);
    return strstr(xml, pattern);
}

static bool xml_get_attr(const char *xml, const char *attr, char *out, int max_len)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    const char *start = strstr(xml, pattern);
    if (!start) return false;
    start += strlen(pattern);
    
    const char *end = strchr(start, '"');
    if (!end) return false;
    
    int len = (int)(end - start);
    if (len >= max_len) len = max_len - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool xml_get_content(const char *start, const char *end_tag, char *out, int max_len)
{
    const char *gt = strchr(start, '>');
    if (!gt) return false;
    gt++;
    
    const char *end = strstr(gt, end_tag);
    if (!end) return false;
    
    int len = (int)(end - gt);
    if (len >= max_len) len = max_len - 1;
    
    /* Decode XML entities */
    int j = 0;
    for (int i = 0; i < len && j < max_len - 1; i++) {
        if (gt[i] == '&') {
            if (strncmp(&gt[i], "&amp;", 5) == 0) { out[j++] = '&'; i += 4; }
            else if (strncmp(&gt[i], "&lt;", 4) == 0) { out[j++] = '<'; i += 3; }
            else if (strncmp(&gt[i], "&gt;", 4) == 0) { out[j++] = '>'; i += 3; }
            else if (strncmp(&gt[i], "&quot;", 6) == 0) { out[j++] = '"'; i += 5; }
            else if (strncmp(&gt[i], "&apos;", 6) == 0) { out[j++] = '\''; i += 5; }
            else out[j++] = gt[i];
        } else {
            out[j++] = gt[i];
        }
    }
    out[j] = '\0';
    return true;
}

/* Parse duration string "HH:MM:SS" to milliseconds */
static int parse_duration(const char *dur)
{
    int h = 0, m = 0, s = 0;
    if (sscanf(dur, "%d:%d:%d", &h, &m, &s) >= 2) {
        return (h * 3600 + m * 60 + s) * 1000;
    }
    return 0;
}

/* Initialize client */
void dlna_client_init(dlna_client_t *client)
{
    memset(client, 0, sizeof(*client));
    client->state = DLNA_CLIENT_DISCONNECTED;
    strcpy(client->current_id, "0");
}

/* Connect to server */
bool dlna_client_connect(dlna_client_t *client, dlna_server_t *server)
{
    if (!server || !server->valid) {
        strcpy(client->error_msg, "Invalid server");
        client->state = DLNA_CLIENT_ERROR;
        return false;
    }
    
    client->server = server;
    client->state = DLNA_CLIENT_CONNECTED;
    strcpy(client->current_id, "0");
    strncpy(client->current_title, server->name, sizeof(client->current_title) - 1);
    
    if (log_cb) log_cb(1, "[DLNA] Connected to: %s\n", server->name);
    
    return true;
}

/* Perform SOAP Browse request */
bool dlna_client_browse(dlna_client_t *client, const char *container_id)
{
    if (client->state != DLNA_CLIENT_CONNECTED) return false;
    
    /* Build SOAP request body */
    char soap_body[2048];
    snprintf(soap_body, sizeof(soap_body), SOAP_BROWSE_TEMPLATE, container_id);
    
    /* Build full URL with POST headers */
    AVIOContext *avio = NULL;
    AVDictionary *opts = NULL;
    
    /* Set HTTP method and headers */
    av_dict_set(&opts, "method", "POST", 0);
    av_dict_set(&opts, "content_type", "text/xml; charset=\"utf-8\"", 0);
    
    char soap_action[256];
    snprintf(soap_action, sizeof(soap_action), 
             "SOAPACTION: \"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\"\r\n"
             "Content-Length: %d", (int)strlen(soap_body));
    av_dict_set(&opts, "headers", soap_action, 0);
    av_dict_set(&opts, "post_data", soap_body, 0);
    
    int ret = avio_open2(&avio, client->server->control_url, AVIO_FLAG_READ, NULL, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        if (log_cb) log_cb(2, "[DLNA] Browse request failed: %d\n", ret);
        strcpy(client->error_msg, "Browse request failed");
        return false;
    }
    
    /* Read response */
    char *buffer = (char *)av_malloc(HTTP_BUFFER_SIZE);
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
    
    /* Parse DIDL-Lite response */
    client->item_count = 0;
    strncpy(client->current_id, container_id, sizeof(client->current_id) - 1);
    
    /* Find Result element (may be HTML-encoded) */
    const char *result = strstr(buffer, "<Result>");
    if (!result) result = strstr(buffer, "&lt;DIDL-Lite");
    
    if (!result) {
        av_free(buffer);
        if (log_cb) log_cb(2, "[DLNA] No Result in response\n");
        return false;
    }
    
    /* Parse containers and items */
    const char *p = result;
    
    while (client->item_count < DLNA_MAX_ITEMS) {
        /* Look for container or item */
        const char *container = xml_find_tag(p, "container");
        const char *item = xml_find_tag(p, "item");
        
        const char *next = NULL;
        bool is_container = false;
        
        if (container && (!item || container < item)) {
            next = container;
            is_container = true;
        } else if (item) {
            next = item;
            is_container = false;
        } else {
            break;
        }
        
        dlna_item_t *di = &client->items[client->item_count];
        memset(di, 0, sizeof(*di));
        
        /* Get ID */
        xml_get_attr(next, "id", di->id, sizeof(di->id));
        xml_get_attr(next, "parentID", di->parent_id, sizeof(di->parent_id));
        
        /* Get title */
        const char *title = strstr(next, "<dc:title>");
        if (title) {
            xml_get_content(title, "</dc:title>", di->title, sizeof(di->title));
        }
        
        if (is_container) {
            di->type = DLNA_ITEM_CONTAINER;
        } else {
            /* Determine type from upnp:class */
            const char *upnp_class = strstr(next, "<upnp:class>");
            if (upnp_class) {
                if (strstr(upnp_class, "videoItem")) {
                    di->type = DLNA_ITEM_VIDEO;
                } else if (strstr(upnp_class, "audioItem") || strstr(upnp_class, "musicTrack")) {
                    di->type = DLNA_ITEM_AUDIO;
                } else if (strstr(upnp_class, "imageItem") || strstr(upnp_class, "photo")) {
                    di->type = DLNA_ITEM_IMAGE;
                } else {
                    di->type = DLNA_ITEM_VIDEO;  /* Default to video */
                }
            } else {
                di->type = DLNA_ITEM_VIDEO;
            }
            
            /* Get resource URL */
            const char *res = strstr(next, "<res");
            if (res) {
                /* Get duration attribute */
                char duration_str[32] = {0};
                if (xml_get_attr(res, "duration", duration_str, sizeof(duration_str))) {
                    di->duration_ms = parse_duration(duration_str);
                }
                
                /* Get size attribute */
                char size_str[32] = {0};
                if (xml_get_attr(res, "size", size_str, sizeof(size_str))) {
                    di->size_bytes = strtoll(size_str, NULL, 10);
                }
                
                /* Get URL (content of res element) */
                xml_get_content(res, "</res>", di->url, sizeof(di->url));
            }
        }
        
        if (strlen(di->id) > 0 && strlen(di->title) > 0) {
            client->item_count++;
        }
        
        p = next + 1;
    }
    
    av_free(buffer);
    
    if (log_cb) log_cb(1, "[DLNA] Browse returned %d items\n", client->item_count);
    
    return true;
}

/* Get stream URL for item */
bool dlna_client_get_url(dlna_client_t *client, dlna_item_t *item, char *url_out, int max_len)
{
    if (!dlna_item_is_playable(item->type)) return false;
    if (strlen(item->url) == 0) return false;
    
    strncpy(url_out, item->url, max_len - 1);
    url_out[max_len - 1] = '\0';
    
    return true;
}

/* Disconnect */
void dlna_client_disconnect(dlna_client_t *client)
{
    client->server = NULL;
    client->state = DLNA_CLIENT_DISCONNECTED;
    client->item_count = 0;
}

/* Format duration */
void dlna_format_duration(int duration_ms, char *out, int out_len)
{
    if (duration_ms <= 0) {
        snprintf(out, out_len, "--:--");
        return;
    }
    
    int total_sec = duration_ms / 1000;
    int hours = total_sec / 3600;
    int mins = (total_sec % 3600) / 60;
    int secs = total_sec % 60;
    
    if (hours > 0)
        snprintf(out, out_len, "%d:%02d:%02d", hours, mins, secs);
    else
        snprintf(out, out_len, "%d:%02d", mins, secs);
}
