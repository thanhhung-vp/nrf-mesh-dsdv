/* src/chat_cli.c */
#include "chat_cli.h"
#include "dsdv_router.h"
#include "neighbor_mgr.h"
#include "model_handler.h" 
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/bluetooth/mesh/access.h>

LOG_MODULE_REGISTER(chat_cli, CONFIG_LOG_DEFAULT_LEVEL);

static struct bt_mesh_chat_cli *g_chat;
static struct k_work_delayable dsdv_hello_work;
static struct k_work_delayable dsdv_update_work;

/* Sink Node Check */
#ifdef CONFIG_BT_MESH_DSDV_SINK_NODE
    #define IS_SINK_NODE true
#else
    #define IS_SINK_NODE false
#endif

/* --- Handlers --- */

static int handle_dsdv_hello(const struct bt_mesh_model *model,
                             struct bt_mesh_msg_ctx *ctx,
                             struct net_buf_simple *buf)
{
    struct dsdv_hello hello;
    memcpy(&hello, net_buf_simple_pull_mem(buf, sizeof(hello)), sizeof(hello));

    // 1. Update Neighbor
    neighbor_mgr_update(ctx->addr, ctx->recv_rssi);

    // 2. Update Route to Sender (Direct connection -> hop 1)
    dsdv_router_update(ctx->addr, ctx->addr, 1, hello.seq_num, hello.is_sink);

    // 3. If sender has route to sink, update gradient info
    if (hello.gradient != 255) {
        dsdv_router_update(SINK_NODE_ADDR, ctx->addr, hello.gradient + 1, 
                           hello.seq_num, hello.is_sink);
    }

    dsdv_router_recalc_gradient(IS_SINK_NODE);
    return 0;
}

static int handle_dsdv_update(const struct bt_mesh_model *model,
                              struct bt_mesh_msg_ctx *ctx,
                              struct net_buf_simple *buf)
{
    struct dsdv_update_header hdr;
    memcpy(&hdr, net_buf_simple_pull_mem(buf, sizeof(hdr)), sizeof(hdr));

    neighbor_mgr_update(ctx->addr, ctx->recv_rssi);

    for (int i = 0; i < hdr.num_entries; i++) {
        struct dsdv_update_entry entry;
        memcpy(&entry, net_buf_simple_pull_mem(buf, sizeof(entry)), sizeof(entry));

        // Add 1 hop to the metric received
        uint8_t new_hops = (entry.hop_count == 255) ? 255 : entry.hop_count + 1;
        
        dsdv_router_update(entry.dest, ctx->addr, new_hops, entry.seq_num, entry.is_sink);
    }
    
    dsdv_router_recalc_gradient(IS_SINK_NODE);
    return 0;
}

static int handle_dsdv_data(const struct bt_mesh_model *model,
                            struct bt_mesh_msg_ctx *ctx,
                            struct net_buf_simple *buf)
{
    struct dsdv_data_packet pkt;
    memcpy(&pkt, net_buf_simple_pull_mem(buf, sizeof(pkt)), sizeof(pkt));

    if (dsdv_router_is_duplicate(pkt.src, pkt.seq_num)) {
        return 0;
    }

    /* FIX: Use bt_mesh_primary_addr() instead of elem->addr */
    uint16_t my_addr = bt_mesh_primary_addr();
    
    LOG_INF("DATA: Src 0x%04x Dest 0x%04x Payload %d (Hops: %d)", 
            pkt.src, pkt.dest, pkt.payload, pkt.hop_count);

    if (pkt.dest == my_addr) {
        LOG_INF(">>> DATA REACHED DESTINATION! <<<");
        mesh_led_blink(2); 
        return 0;
    }

    // Forwarding
    struct dsdv_route_entry *route = dsdv_router_find(pkt.dest);
    if (route && route->hop_count < 255) {
        pkt.hop_count++;
        
        struct bt_mesh_msg_ctx new_ctx = {
            .app_idx = ctx->app_idx,
            .addr = route->next_hop, // Unicast to next hop
            .send_ttl = BT_MESH_TTL_DEFAULT,
        };
        
        BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_CHAT_CLI_OP_DSDV_DATA, sizeof(pkt));
        bt_mesh_model_msg_init(&msg, BT_MESH_CHAT_CLI_OP_DSDV_DATA);
        
        /* FIX: Pass address of msg (&msg) */
        net_buf_simple_add_mem(&msg, &pkt, sizeof(pkt));

        /* FIX: Pass address of msg (&msg) */
        if (bt_mesh_model_send(model, &new_ctx, &msg, NULL, NULL) == 0) {
            LOG_INF("Forwarded to 0x%04x", route->next_hop);
        }
    } else {
        LOG_WRN("No route to 0x%04x, drop.", pkt.dest);
    }

    return 0;
}

const struct bt_mesh_model_op _bt_mesh_chat_cli_op[] = {
    { BT_MESH_CHAT_CLI_OP_DSDV_HELLO,  0, handle_dsdv_hello },
    { BT_MESH_CHAT_CLI_OP_DSDV_UPDATE, 0, handle_dsdv_update },
    { BT_MESH_CHAT_CLI_OP_DSDV_DATA,   0, handle_dsdv_data },
    BT_MESH_MODEL_OP_END,
};

/* --- Workers --- */

static void dsdv_hello_worker(struct k_work *work)
{
    if (!g_chat) return;

    /* FIX: Use bt_mesh_primary_addr() */
    struct dsdv_hello hello_pkt = {
        .src = bt_mesh_primary_addr(),
        .seq_num = dsdv_router_get_seq(),
        .is_sink = IS_SINK_NODE,
        .gradient = g_dsdv_my_gradient
    };
    
    dsdv_router_inc_seq();

    BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_CHAT_CLI_OP_DSDV_HELLO, sizeof(hello_pkt));
    bt_mesh_model_msg_init(&msg, BT_MESH_CHAT_CLI_OP_DSDV_HELLO);
    
    /* FIX: Pass address of msg (&msg) */
    net_buf_simple_add_mem(&msg, &hello_pkt, sizeof(hello_pkt));

    // Broadcast Hello
    struct bt_mesh_msg_ctx ctx = {
        .app_idx = 0, 
        .addr = BROADCAST_ADDR,
        .send_ttl = 1, // Hello only goes to neighbors
    };

    /* FIX: Pass address of msg (&msg) */
    bt_mesh_model_send(g_chat->model, &ctx, &msg, NULL, NULL);

    k_work_reschedule(&dsdv_hello_work, K_MSEC(HELLO_INTERVAL_MS));
}

static void dsdv_update_worker(struct k_work *work)
{
    if (!g_chat) return;

    dsdv_router_cleanup(); // Clean old routes first

    // Create Update Packet
    BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_CHAT_CLI_OP_DSDV_UPDATE, 64); // Approx size
    bt_mesh_model_msg_init(&msg, BT_MESH_CHAT_CLI_OP_DSDV_UPDATE);
    
    // Placeholder for Header
    /* FIX: Pass address of msg (&msg) */
    struct dsdv_update_header *hdr = net_buf_simple_add(&msg, sizeof(struct dsdv_update_header));
    
    /* FIX: Use bt_mesh_primary_addr() */
    hdr->src = bt_mesh_primary_addr();
    hdr->num_entries = 0;

    int entries = 0;
    for (int i = 0; i < DSDV_ROUTE_TABLE_SIZE; i++) {
        if (g_dsdv_routes[i].dest != 0 && g_dsdv_routes[i].changed) {
            
            /* FIX: Pass address of msg (&msg) */
            struct dsdv_update_entry *entry = net_buf_simple_add(&msg, sizeof(struct dsdv_update_entry));
            entry->dest = g_dsdv_routes[i].dest;
            entry->hop_count = g_dsdv_routes[i].hop_count;
            entry->seq_num = g_dsdv_routes[i].seq_num;
            entry->is_sink = g_dsdv_routes[i].is_sink;
            
            g_dsdv_routes[i].changed = 0; // Clear flag
            entries++;
            
            if (entries >= MAX_UPDATE_ENTRIES) break;
        }
    }
    hdr->num_entries = entries;

    if (entries > 0) {
        struct bt_mesh_msg_ctx ctx = {
            .app_idx = 0,
            .addr = BROADCAST_ADDR,
            .send_ttl = 1,
        };
        /* FIX: Pass address of msg (&msg) */
        bt_mesh_model_send(g_chat->model, &ctx, &msg, NULL, NULL);
    }

    k_work_reschedule(&dsdv_update_work, K_MSEC(UPDATE_INTERVAL_MS));
}

/* --- Public Functions --- */

int bt_mesh_chat_cli_data_send(struct bt_mesh_chat_cli *chat, uint16_t data_payload)
{
    struct dsdv_route_entry *r = dsdv_router_find(SINK_NODE_ADDR);
    
    if (!r || r->hop_count == 255) {
        LOG_ERR("No route to sink!");
        return -EHOSTUNREACH;
    }

    struct dsdv_data_packet pkt = {
        /* FIX: Use bt_mesh_primary_addr() */
        .src = bt_mesh_primary_addr(),
        .dest = SINK_NODE_ADDR,
        .seq_num = sys_rand32_get(), // Unique ID for packet
        .hop_count = 0,
        .payload = data_payload
    };

    BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_CHAT_CLI_OP_DSDV_DATA, sizeof(pkt));
    bt_mesh_model_msg_init(&msg, BT_MESH_CHAT_CLI_OP_DSDV_DATA);
    
    /* FIX: Pass address of msg (&msg) */
    net_buf_simple_add_mem(&msg, &pkt, sizeof(pkt));

    struct bt_mesh_msg_ctx ctx = {
        .app_idx = 0,
        .addr = r->next_hop,
        .send_ttl = BT_MESH_TTL_DEFAULT,
    };

    /* FIX: Pass address of msg (&msg) */
    return bt_mesh_model_send(chat->model, &ctx, &msg, NULL, NULL);
}

static int bt_mesh_chat_cli_init(const struct bt_mesh_model *model)
{
    struct bt_mesh_chat_cli *chat = model->rt->user_data;
    chat->model = model;
    g_chat = chat;

    neighbor_mgr_init();
    dsdv_router_init(IS_SINK_NODE);

    k_work_init_delayable(&dsdv_hello_work, dsdv_hello_worker);
    k_work_init_delayable(&dsdv_update_work, dsdv_update_worker);

    return 0;
}

static int bt_mesh_chat_cli_start(const struct bt_mesh_model *model)
{
    k_work_schedule(&dsdv_hello_work, K_MSEC(1000));
    k_work_schedule(&dsdv_update_work, K_MSEC(5000));
    return 0;
}

const struct bt_mesh_model_cb _bt_mesh_chat_cli_cb = {
    .init = bt_mesh_chat_cli_init,
    .start = bt_mesh_chat_cli_start,
};