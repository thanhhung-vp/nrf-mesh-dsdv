#ifndef NEIGHBOR_MGR_H
#define NEIGHBOR_MGR_H

#include "dsdv_types.h"
#include <stdbool.h>

void neighbor_mgr_init(void);
void neighbor_mgr_update(uint16_t addr, int8_t rssi);
bool neighbor_mgr_is_valid(uint16_t addr);
int neighbor_mgr_get_all(uint16_t *addrs, int8_t *rssi, int max_count);

#endif