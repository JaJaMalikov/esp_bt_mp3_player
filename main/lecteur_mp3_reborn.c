// main.c - Test minimal lecteur MP3 Bluetooth
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "playlist_manager.h"
#include "audio_manager.h"
#include "bt_control.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"

static const char *TAG = "main";
static audio_element_handle_t fatfs_stream_reader, mp3_decoder, bt_stream_writer;
static audio_pipeline_handle_t pipeline;

void app_main(void) {
    ESP_LOGI(TAG, "Init NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Init playlist manager");
    if (playlist_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Playlist manager failed");
        return;
    }
    ESP_ERROR_CHECK(bt_control_init());

    ESP_LOGI(TAG, "Start audio playback");
    if (audio_manager_start() != ESP_OK) {
        ESP_LOGE(TAG, "Audio pipeline failed to start");
        return;
    }



}

