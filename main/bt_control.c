#include <string.h>
#include <inttypes.h>
#include "bt_control.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "audio_manager.h"

static const char *TAG = "bt_control";

static uint8_t remote_bt_device_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
static esp_bd_addr_t remote_bd_addr = {0};
static bool a2dp_connected = false;
static bool device_found = false;

bool bt_control_is_connected(void) {
    return a2dp_connected;
}

void bt_control_start_discovery(void) {
    device_found = false;
    ESP_LOGI(TAG, "Starting device discovery...");
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 5, 0);
}

static char *bda2str(const esp_bd_addr_t bda, char *str, size_t size) {
    if (!bda || !str || size < 18) return NULL;
    snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
        bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}

static bool get_name_from_eir(uint8_t *eir, uint8_t *bdname) {
    uint8_t *rmt_bdname = NULL;
    uint8_t rmt_bdname_len = 0;
    if (!eir) return false;
    rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &rmt_bdname_len);
    if (!rmt_bdname)
        rmt_bdname = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &rmt_bdname_len);
    if (rmt_bdname) {
        if (rmt_bdname_len > ESP_BT_GAP_MAX_BDNAME_LEN) rmt_bdname_len = ESP_BT_GAP_MAX_BDNAME_LEN;
        memcpy(bdname, rmt_bdname, rmt_bdname_len);
        bdname[rmt_bdname_len] = '\0';
        return true;
    }
    return false;
}

static void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param) {
    char bda_str[18];
    uint8_t *eir = NULL;
    uint8_t peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
    esp_bt_gap_dev_prop_t *p;
    ESP_LOGI(TAG, "Scanned device: %s", bda2str(param->disc_res.bda, bda_str, sizeof(bda_str)));
    for (int i = 0; i < param->disc_res.num_prop; i++) {
        p = param->disc_res.prop + i;
        if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
            eir = (uint8_t *)(p->val);
            get_name_from_eir(eir, peer_bdname);
            ESP_LOGI(TAG, "--Name: %s", peer_bdname);
        }
    }
    ESP_LOGI(TAG, "Looking for: %s", remote_bt_device_name);
    if (eir) {
        get_name_from_eir(eir, peer_bdname);
        if (strcmp((char *)peer_bdname, (char *)remote_bt_device_name) != 0)
            return;
        ESP_LOGI(TAG, "Found target! address %s, name %s", bda_str, peer_bdname);
        device_found = true;
        memcpy(remote_bd_addr, param->disc_res.bda, ESP_BD_ADDR_LEN);
        ESP_LOGI(TAG, "Cancel device discovery ...");
        esp_bt_gap_cancel_discovery();
    }
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            filter_inquiry_scan_result(param);
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                if (device_found && !a2dp_connected) {
                    ESP_LOGI(TAG, "Discovery stopped, connecting A2DP...");
                    esp_a2d_source_connect(remote_bd_addr);
                } else if (!a2dp_connected) {
                    ESP_LOGI(TAG, "Discovery failed, retrying...");
                    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 5, 0);
                }
            }
            break;
        default:
            break;
    }
}

static void bt_app_a2dp_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "A2DP CONNECTED to %s", remote_bt_device_name);
                a2dp_connected = true;
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGW(TAG, "A2DP DISCONNECTED from %s", remote_bt_device_name);
                a2dp_connected = false;
                device_found = false;
                ESP_LOGI(TAG, "Re-launching device discovery...");
                esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 5, 0);
            }
            break;
        default:
            break;
    }
}

void bt_app_avrc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
    switch(event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
            ESP_LOGI(TAG, "AVRCP Connection State: %d", param->conn_stat.connected);
            break;
        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
            switch(param->psth_rsp.key_code) {
                case ESP_AVRC_PT_CMD_PLAY:
                    audio_manager_start();
                    break;
                case ESP_AVRC_PT_CMD_STOP:
                case ESP_AVRC_PT_CMD_PAUSE:
                    audio_manager_stop();
                    break;
                case ESP_AVRC_PT_CMD_FORWARD:
                    audio_manager_next();
                    break;
                case ESP_AVRC_PT_CMD_BACKWARD:
                    audio_manager_prev();
                    break;
                default:
                    break;
            }
            break;
        default: break;
    }
}

static void bt_app_avrc_tg_cb(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param) {
    if (event == ESP_AVRC_TG_PASSTHROUGH_CMD_EVT &&
        param->psth_cmd.key_state == ESP_AVRC_PT_CMD_STATE_PRESSED) {
        switch (param->psth_cmd.key_code) {
            case ESP_AVRC_PT_CMD_PLAY:
                audio_manager_resume();
                break;
            case ESP_AVRC_PT_CMD_STOP:
            case ESP_AVRC_PT_CMD_PAUSE:
                audio_manager_pause();
                break;
            case ESP_AVRC_PT_CMD_FORWARD:
                audio_manager_next();
                break;
            case ESP_AVRC_PT_CMD_BACKWARD:
                audio_manager_prev();
                break;
            default:
                break;
        }
    }
}

esp_err_t bt_control_init(void) {
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
  esp_bt_pin_code_t pin_code = { '1', '2', '3', '4' };
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
  ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());

  ESP_ERROR_CHECK(esp_avrc_tg_init());
  esp_avrc_tg_register_callback(bt_app_avrc_tg_cb);
  ESP_ERROR_CHECK(esp_avrc_ct_init());
  esp_avrc_ct_register_callback(bt_app_avrc_ct_cb);

  esp_bt_gap_set_device_name("ESP_SOURCE_STREAM_DEMO");
  esp_bt_gap_set_pin(pin_type, 1, pin_code);
  esp_bt_gap_register_callback(bt_app_gap_cb);
  esp_a2d_register_callback(bt_app_a2dp_cb);
  esp_a2d_source_register_data_callback(NULL);
  ESP_ERROR_CHECK(esp_a2d_source_init());
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

  const char *remote_name = NULL;
  remote_name = CONFIG_BT_REMOTE_NAME;
  if (remote_name) {
    memcpy(&remote_bt_device_name, remote_name, strlen(remote_name) + 1);
  } else {
    memcpy(&remote_bt_device_name, "ESP_SINK_STREAM_DEMO", ESP_BT_GAP_MAX_BDNAME_LEN);
  }

  bt_control_start_discovery();

    ESP_LOGI(TAG, "Bluetooth A2DP source initialized");
    return ESP_OK;
}

