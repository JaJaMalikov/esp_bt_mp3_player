#include "esp_peripherals.h"
#include "audio_manager.h"
#include "esp_log.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "fatfs_stream.h"
#include "mp3_decoder.h"
#include "a2dp_stream.h"
#include "playlist_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_mgr";

static audio_pipeline_handle_t pipeline = NULL;
static audio_element_handle_t fatfs_reader = NULL;
static audio_element_handle_t mp3_decoder = NULL;
static audio_element_handle_t bt_stream_writer = NULL;
static audio_event_iface_handle_t evt = NULL;

static void audio_event_task(void *param)
{
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
            msg.source == (void *)fatfs_reader &&
            msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
            (intptr_t)msg.data == AEL_STATUS_STATE_FINISHED) {
            ESP_LOGI(TAG, "Track finished. Loading next track.");
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            const char *next_uri = playlist_manager_get_next();
            audio_element_set_uri(fatfs_reader, next_uri);
            ESP_LOGI(TAG, "Next track: %s", next_uri);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_elements(pipeline);
            audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
            audio_pipeline_run(pipeline);
        }
    }
}

esp_err_t audio_manager_start(void)
{
    ESP_LOGI(TAG, "Starting audio pipeline");

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (!pipeline) {
        ESP_LOGE(TAG, "Failed to create pipeline");
        return ESP_FAIL;
    }

    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_reader = fatfs_stream_init(&fatfs_cfg);

    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    a2dp_stream_config_t a2dp_config = {
        .type = AUDIO_STREAM_WRITER,
        .user_callback = { 0 },
    };
    bt_stream_writer = a2dp_stream_init(&a2dp_config);

    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 5, 0);

    audio_pipeline_register(pipeline, fatfs_reader, "file");
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, bt_stream_writer, "bt");

    audio_pipeline_link(pipeline, (const char *[]) {"file", "mp3", "bt"}, 3);

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);
    audio_pipeline_set_listener(pipeline, evt);

    const char *uri = playlist_manager_get_next();
    audio_element_set_uri(fatfs_reader, uri);

    ESP_LOGI(TAG, "Playing: %s", uri);
    audio_pipeline_run(pipeline);

    xTaskCreatePinnedToCore(audio_event_task, "audio_evt_task", 4096, NULL, 5, NULL, 1);

    return ESP_OK;
}

static esp_err_t change_track(const char *uri)
{
    if (!pipeline) return ESP_FAIL;

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);

    audio_element_set_uri(fatfs_reader, uri);
    ESP_LOGI(TAG, "Loading: %s", uri);
    audio_pipeline_reset_ringbuffer(pipeline);
    audio_pipeline_reset_elements(pipeline);
    audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
    audio_pipeline_run(pipeline);
    return ESP_OK;
}

esp_err_t audio_manager_next(void)
{
    const char *next_uri = playlist_manager_get_next();
    return change_track(next_uri);
}

esp_err_t audio_manager_prev(void)
{
    const char *prev_uri = playlist_manager_get_prev();
    return change_track(prev_uri);
}

esp_err_t audio_manager_pause(void)
{
    if (!pipeline) return ESP_FAIL;
    ESP_LOGI(TAG, "Pausing audio pipeline");
    return audio_pipeline_pause(pipeline);
}

esp_err_t audio_manager_resume(void)
{
    if (!pipeline) return ESP_FAIL;
    ESP_LOGI(TAG, "Resuming audio pipeline");
    return audio_pipeline_resume(pipeline);
}

esp_err_t audio_manager_play(const char *path)
{
    return change_track(path);
}

esp_err_t audio_manager_stop(void)
{
    ESP_LOGI(TAG, "Stopping audio pipeline");
    if (!pipeline) return ESP_FAIL;

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, fatfs_reader);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, bt_stream_writer);

    audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_reader);
    audio_element_deinit(mp3_decoder);
    audio_element_deinit(bt_stream_writer);

    if (evt) {
        audio_event_iface_destroy(evt);
        evt = NULL;
    }

    pipeline = NULL;
    return ESP_OK;
}
