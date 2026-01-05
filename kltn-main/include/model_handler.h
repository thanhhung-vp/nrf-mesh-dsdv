#ifndef MODEL_HANDLER_H
#define MODEL_HANDLER_H

#include <zephyr/bluetooth/mesh.h>

#ifdef __cplusplus
extern "C" {
#endif

const struct bt_mesh_comp *model_handler_init(void);
void mesh_led_blink(int count);

#ifdef __cplusplus
}
#endif
#endif