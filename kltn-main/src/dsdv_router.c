#include "dsdv_router.h"
#include "neighbor_mgr.h"
#include <zephyr/kernel.h>
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dsdv_core, CONFIG_LOG_DEFAULT_LEVEL);

/* Globals */
struct dsdv_route_entry g_dsdv_routes[DSDV_ROUTE_TABLE_SIZE];
uint8_t g_dsdv_my_gradient = 255;

/* Locals */
static uint32_t g_my_seq_num = 0;
static struct { uint16_t src; uint32_t seq; } g_dup_cache[DUP_CACHE_SIZE];
static int g_dup_idx = 0;

void dsdv_router_init(bool is_sink_node)
{
    memset(g_dsdv_routes, 0, sizeof(g_dsdv_routes));
    memset(g_dup_cache, 0, sizeof(g_dup_cache));
    g_dsdv_my_gradient = is_sink_node ? 0 : 255;
    g_my_seq_num = 1;
}

struct dsdv_route_entry* dsdv_router_find(uint16_t dest)
{
    for (int i = 0; i < DSDV_ROUTE_TABLE_SIZE; i++) {
        if (g_dsdv_routes[i].dest == dest) {
            return &g_dsdv_routes[i];
        }
    }
    return NULL;
}

/* Core DSDV Algorithm Logic */
void dsdv_router_update(uint16_t dest, uint16_t next_hop, uint8_t hop_count, 
                        uint32_t seq_num, uint8_t is_sink)
{
    struct dsdv_route_entry *r = dsdv_router_find(dest);
    uint32_t now = k_uptime_get_32();

    if (r) {
        // Rule 1: Higher sequence number -> Always update
        bool update = (seq_num > r->seq_num);
        
        // Rule 2: Same sequence number but better hop count
        if (!update && (seq_num == r->seq_num) && (hop_count < r->hop_count)) {
            update = true;
        }

        if (update) {
            r->next_hop = next_hop;
            r->hop_count = hop_count;
            r->seq_num = seq_num;
            r->last_update_time = now;
            r->is_sink = is_sink;
            r->changed = 1; 
            // LOG_DBG("Route Updated: Dest 0x%04x via 0x%04x hops %d", dest, next_hop, hop_count);
        }
    } else {
        // Add new route
        for (int i = 0; i < DSDV_ROUTE_TABLE_SIZE; i++) {
            if (g_dsdv_routes[i].dest == 0) {
                g_dsdv_routes[i].dest = dest;
                g_dsdv_routes[i].next_hop = next_hop;
                g_dsdv_routes[i].hop_count = hop_count;
                g_dsdv_routes[i].seq_num = seq_num;
                g_dsdv_routes[i].last_update_time = now;
                g_dsdv_routes[i].is_sink = is_sink;
                g_dsdv_routes[i].changed = 1;
                // LOG_INF("New Route: Dest 0x%04x via 0x%04x", dest, next_hop);
                break;
            }
        }
    }
}

void dsdv_router_cleanup(void)
{
    uint32_t now = k_uptime_get_32();
    for (int i = 0; i < DSDV_ROUTE_TABLE_SIZE; i++) {
        if (g_dsdv_routes[i].dest != 0) {
            if ((now - g_dsdv_routes[i].last_update_time) > DSDV_ROUTE_TIMEOUT_MS) {
                // LOG_INF("Route Timeout: 0x%04x", g_dsdv_routes[i].dest);
                g_dsdv_routes[i].dest = 0; // Clear
                g_dsdv_routes[i].hop_count = 255;
            }
        }
    }
}

void dsdv_router_recalc_gradient(bool is_sink_node)
{
    if (is_sink_node) {
        g_dsdv_my_gradient = 0;
        return;
    }

    uint8_t min_hops = 255;
    
    // Find route to SINK (0x0001) or nodes marked as sink
    struct dsdv_route_entry *r = dsdv_router_find(SINK_NODE_ADDR);
    
    // Check if route exists AND next hop is a valid neighbor
    if (r && neighbor_mgr_is_valid(r->next_hop)) {
        min_hops = r->hop_count;
    }

    // Also check other routes flagged as is_sink
    for (int i=0; i<DSDV_ROUTE_TABLE_SIZE; i++) {
        if (g_dsdv_routes[i].dest != 0 && g_dsdv_routes[i].is_sink) {
             if (neighbor_mgr_is_valid(g_dsdv_routes[i].next_hop)) {
                 if (g_dsdv_routes[i].hop_count < min_hops) {
                     min_hops = g_dsdv_routes[i].hop_count;
                 }
             }
        }
    }

    g_dsdv_my_gradient = min_hops;
}

bool dsdv_router_is_duplicate(uint16_t src, uint32_t seq)
{
    for (int i = 0; i < DUP_CACHE_SIZE; i++) {
        if (g_dup_cache[i].src == src && g_dup_cache[i].seq == seq) {
            return true;
        }
    }
    // Add to cache
    g_dup_cache[g_dup_idx].src = src;
    g_dup_cache[g_dup_idx].seq = seq;
    g_dup_idx = (g_dup_idx + 1) % DUP_CACHE_SIZE;
    return false;
}

uint32_t dsdv_router_get_seq(void) { return g_my_seq_num; }
void dsdv_router_inc_seq(void) { g_my_seq_num++; }