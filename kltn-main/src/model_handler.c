/* src/model_handler.c */
#include "model_handler.h"
#include "chat_cli.h"
#include "dsdv_router.h"
#include "neighbor_mgr.h"
#include <zephyr/shell/shell.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/hci.h>
#include <stdlib.h> 

/* FIX 1: Thêm header cho Config và Health Server */
#include <zephyr/bluetooth/mesh/cfg_srv.h>
#include <zephyr/bluetooth/mesh/health_srv.h>

LOG_MODULE_DECLARE(chat_cli);

#ifndef BT_MESH_CID_NORDIC
#define BT_MESH_CID_NORDIC 0x0059 
#endif

/* FIX 2: Tự định nghĩa Model ID (Sử dụng ID giả lập cho SIG Model) */
#define BT_MESH_MODEL_ID_VENDOR_EXAMPLE  0x1234

static struct bt_mesh_chat_cli chat;

/* FIX 3: Khai báo các struct cần thiết cho Health Server */
static struct bt_mesh_health_srv health_srv = {};
BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

/* Định nghĩa mảng Models */
static struct bt_mesh_model root_models[] = {
    BT_MESH_MODEL_CFG_SRV,
    
    /* FIX 4: Truyền tham số cho macro Health Server */
    BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
    
    BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_VENDOR_EXAMPLE, 
                     _bt_mesh_chat_cli_op, 
                     &chat.pub, 
                     &chat, 
                     &_bt_mesh_chat_cli_cb)
};

/* Định nghĩa Elements thủ công */
static struct bt_mesh_elem elements[] = {
    {
        .loc = 0,
        .model_count = ARRAY_SIZE(root_models),
        .models = root_models,
        .vnd_model_count = 0,
        .vnd_models = NULL,
    }
};

static const struct bt_mesh_comp comp = {
    .cid = BT_MESH_CID_NORDIC,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements),
};

/* --- LED Logic --- */
static struct k_work_delayable blink_work;
static int blink_counter = 0;

static void blink_timeout(struct k_work *work)
{
    if (blink_counter > 0) {
        dk_set_led(DK_LED1, blink_counter % 2);
        blink_counter--;
        k_work_reschedule(&blink_work, K_MSEC(200));
    } else {
        dk_set_led_off(DK_LED1);
    }
}

void mesh_led_blink(int count)
{
    blink_counter = count * 2;
    k_work_reschedule(&blink_work, K_NO_WAIT);
}

/* --- Shell Commands --- */

static int cmd_show_routes(const struct shell *shell, size_t argc, char *argv[])
{
    shell_print(shell, "Dest\tNext\tHops\tSeq\tGrad: %d", g_dsdv_my_gradient);
    for (int i = 0; i < DSDV_ROUTE_TABLE_SIZE; i++) {
        if (g_dsdv_routes[i].dest != 0) {
            shell_print(shell, "0x%04x\t0x%04x\t%d\t%d",
                        g_dsdv_routes[i].dest,
                        g_dsdv_routes[i].next_hop,
                        g_dsdv_routes[i].hop_count,
                        g_dsdv_routes[i].seq_num);
        }
    }
    return 0;
}

static int cmd_show_neighbors(const struct shell *shell, size_t argc, char *argv[])
{
    uint16_t addrs[MAX_NEIGHBORS];
    int8_t rssi[MAX_NEIGHBORS];
    int count = neighbor_mgr_get_all(addrs, rssi, MAX_NEIGHBORS);

    shell_print(shell, "Neighbors (%d):", count);
    for (int i = 0; i < count; i++) {
        shell_print(shell, " - Addr: 0x%04x, RSSI: %d", addrs[i], rssi[i]);
    }
    return 0;
}

static int cmd_send_data(const struct shell *shell, size_t argc, char *argv[])
{
    if (argc < 2) {
        shell_error(shell, "Usage: send <val>");
        return -1;
    }
    uint16_t val = (uint16_t)atoi(argv[1]);
    int err = bt_mesh_chat_cli_data_send(&chat, val);
    if (err) {
        shell_error(shell, "Send failed: %d", err);
    } else {
        shell_print(shell, "Sent data: %d", val);
    }
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(chat_cmds,
    SHELL_CMD(routes, NULL, "Show Routes", cmd_show_routes),
    SHELL_CMD(neighbors, NULL, "Show Neighbors", cmd_show_neighbors),
    SHELL_CMD(send, NULL, "Send Data <val>", cmd_send_data),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(chat, &chat_cmds, "DSDV Chat", NULL);

/* --- Init --- */

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    if (has_changed & button_state & DK_BTN1_MSK) {
        static uint16_t btn_cnt = 0;
        bt_mesh_chat_cli_data_send(&chat, ++btn_cnt);
    }
}

const struct bt_mesh_comp *model_handler_init(void)
{
    dk_buttons_init(button_handler);
    k_work_init_delayable(&blink_work, blink_timeout);
    return &comp;
}