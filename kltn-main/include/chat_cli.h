#ifndef CHAT_CLI_H
#define CHAT_CLI_H

#include <zephyr/bluetooth/mesh.h>
#include "dsdv_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OpCodes */
#define BT_MESH_CHAT_CLI_OP_DSDV_HELLO    BT_MESH_MODEL_OP_2(0x00, 0x01)
#define BT_MESH_CHAT_CLI_OP_DSDV_UPDATE   BT_MESH_MODEL_OP_2(0x00, 0x02)
#define BT_MESH_CHAT_CLI_OP_DSDV_DATA     BT_MESH_MODEL_OP_2(0x00, 0x03)

struct bt_mesh_chat_cli {
    const struct bt_mesh_model *model;
    struct bt_mesh_model_pub pub;
    struct net_buf_simple pub_msg;
    uint8_t buf[BT_MESH_MODEL_BUF_LEN(BT_MESH_CHAT_CLI_OP_DSDV_DATA, 20)];
};

/* API for App Layer */
int bt_mesh_chat_cli_data_send(struct bt_mesh_chat_cli *chat, uint16_t data);

/* Model Definitions */
extern const struct bt_mesh_model_cb _bt_mesh_chat_cli_cb;
extern const struct bt_mesh_model_op _bt_mesh_chat_cli_op[];

#ifdef __cplusplus
}
#endif
#endif