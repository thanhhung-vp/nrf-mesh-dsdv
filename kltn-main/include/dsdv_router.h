#ifndef DSDV_ROUTER_H
#define DSDV_ROUTER_H

#include "dsdv_types.h"
#include <stdbool.h>

/* Export global variables solely for Shell Commands visibility */
extern struct dsdv_route_entry g_dsdv_routes[DSDV_ROUTE_TABLE_SIZE];
extern uint8_t g_dsdv_my_gradient;

/* Core API */
void dsdv_router_init(bool is_sink_node);
void dsdv_router_update(uint16_t dest, uint16_t next_hop, uint8_t hop_count, 
                       uint32_t seq_num, uint8_t is_sink);
struct dsdv_route_entry* dsdv_router_find(uint16_t dest);
void dsdv_router_cleanup(void); // Remove stale routes
void dsdv_router_recalc_gradient(bool is_sink_node);

/* Helper API */
bool dsdv_router_is_duplicate(uint16_t src, uint32_t seq);
uint32_t dsdv_router_get_seq(void);
void dsdv_router_inc_seq(void);

#endif