#ifndef BT_CONTROL_H
#define BT_CONTROL_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t bt_control_init(void);
void bt_control_start_discovery(void);
bool bt_control_is_connected(void);

#endif
