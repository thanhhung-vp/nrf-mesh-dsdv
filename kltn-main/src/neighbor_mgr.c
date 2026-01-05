#include "neighbor_mgr.h"
#include <zephyr/kernel.h>
#include <string.h>

struct neighbor_entry {
    uint16_t addr;
    int8_t rssi;
    uint32_t last_seen;
};

static struct neighbor_entry neighbors[MAX_NEIGHBORS];

void neighbor_mgr_init(void)
{
    memset(neighbors, 0, sizeof(neighbors));
}

void neighbor_mgr_update(uint16_t addr, int8_t rssi)
{
    uint32_t now = k_uptime_get_32();
    int oldest_idx = 0;
    uint32_t oldest_time = 0xFFFFFFFF;

    // 1. Try to find existing neighbor
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].addr == addr) {
            neighbors[i].rssi = rssi;
            neighbors[i].last_seen = now;
            return;
        }
        
        // Track empty slot or oldest slot
        if (neighbors[i].addr == 0) {
            oldest_idx = i;
            oldest_time = 0; // Priority to fill empty
        } else if (neighbors[i].last_seen < oldest_time && oldest_time != 0) {
            oldest_time = neighbors[i].last_seen;
            oldest_idx = i;
        }
    }

    // 2. Add new or replace oldest
    neighbors[oldest_idx].addr = addr;
    neighbors[oldest_idx].rssi = rssi;
    neighbors[oldest_idx].last_seen = now;
}

bool neighbor_mgr_is_valid(uint16_t addr)
{
    uint32_t now = k_uptime_get_32();
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].addr == addr) {
            return (now - neighbors[i].last_seen) < DSDV_NEIGHBOR_TIMEOUT_MS;
        }
    }
    return false;
}

int neighbor_mgr_get_all(uint16_t *addrs, int8_t *rssi, int max_count)
{
    int count = 0;
    uint32_t now = k_uptime_get_32();

    for (int i = 0; i < MAX_NEIGHBORS && count < max_count; i++) {
        if (neighbors[i].addr != 0 && (now - neighbors[i].last_seen) < DSDV_NEIGHBOR_TIMEOUT_MS) {
            addrs[count] = neighbors[i].addr;
            if (rssi) rssi[count] = neighbors[i].rssi;
            count++;
        }
    }
    return count;
}