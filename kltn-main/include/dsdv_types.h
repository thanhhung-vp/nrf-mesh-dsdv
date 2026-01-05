/* dsdv_types.h */
#ifndef DSDV_TYPES_H
#define DSDV_TYPES_H

#include <zephyr/types.h>
#include <zephyr/toolchain.h>

/* Configuration Constants */
#define DSDV_ROUTE_TABLE_SIZE      32
#define MAX_NEIGHBORS              16
#define MAX_UPDATE_ENTRIES         10  /* Số lượng route tối đa trong 1 gói Update */
#define DUP_CACHE_SIZE             16

/* Timeouts */
#define HELLO_INTERVAL_MS          2000
#define UPDATE_INTERVAL_MS         10000
#define DSDV_NEIGHBOR_TIMEOUT_MS   15000 /* 3x Hello interval + buffer */
#define DSDV_ROUTE_TIMEOUT_MS      45000 

/* Addressing */
#define SINK_NODE_ADDR             0x0001
#define BROADCAST_ADDR             0xFFFF

/* --- Network Packet Structures (Over-the-air) --- */

/* HELLO Packet */
struct dsdv_hello {
    uint16_t src;
    uint32_t seq_num;
    uint8_t is_sink;
    uint8_t gradient;
} __packed;

/* UPDATE Packet Headers */
struct dsdv_update_header {
    uint16_t src;
    uint8_t num_entries;
} __packed;

struct dsdv_update_entry {
    uint16_t dest;
    uint8_t hop_count;
    uint32_t seq_num;
    uint8_t is_sink;
} __packed;

/* DATA Packet */
struct dsdv_data_packet {
    uint16_t src;
    uint16_t dest;
    uint32_t seq_num;
    uint8_t hop_count;
    uint16_t payload;
} __packed;

/* --- Internal Structures --- */

/* Routing Table Entry */
struct dsdv_route_entry {
    uint16_t dest;             /* Destination Address */
    uint16_t next_hop;         /* Next Hop Address */
    uint8_t hop_count;         /* Metric */
    uint32_t seq_num;          /* Destination Sequence Number */
    uint32_t last_update_time; /* Timestamp for timeout */
    uint8_t changed:1;         /* Flag: Need to send in next update? */
    uint8_t is_sink:1;         /* Flag: Is destination a sink? */
};

#endif /* DSDV_TYPES_H */