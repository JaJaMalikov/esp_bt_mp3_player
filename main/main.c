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
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include <dirent.h>
#include "path_config.h"

#define WIFI_AP_SSID "mp3-player"
#define WIFI_AP_PASS "12345678"
#define MOUNT_POINT MP3_DIR


static const char *TAG = "main";
static httpd_handle_t http_server = NULL;

void init_wifi_ap() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t ap_conf = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = 1,
            .password = WIFI_AP_PASS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = 4
        }
    };
    if (strlen(WIFI_AP_PASS) == 0) {
        ap_conf.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_conf));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP démarré SSID:%s", WIFI_AP_SSID);
}

esp_err_t list_handler(httpd_req_t *req) {
    DIR *dir = opendir(MOUNT_POINT);
    if (!dir) return ESP_FAIL;
    struct dirent *de;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "[");
    bool first=true;
    while((de=readdir(dir))) {
        if(de->d_type==DT_REG) {
            if(!first) httpd_resp_sendstr_chunk(req, ",");
            httpd_resp_sendstr_chunk(req, "\"");
            httpd_resp_sendstr_chunk(req, de->d_name);
            httpd_resp_sendstr_chunk(req, "\"");
            first=false;
        }
    }
    closedir(dir);
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t play_handler(httpd_req_t *req) {
    char buf[128];
    size_t len = httpd_req_get_url_query_len(req)+1;
    if (len > sizeof(buf)) return ESP_FAIL;
    if (httpd_req_get_url_query_str(req, buf, len)==ESP_OK) {
        char file[64];
        if (httpd_query_key_value(buf, "file", file, sizeof(file))==ESP_OK) {
            char path[128];
            snprintf(path, sizeof(path), "%s/%s", MP3_DIR, file);
            playlist_manager_set_current_by_name(file);
            audio_manager_play(path);
            httpd_resp_sendstr(req, "OK");
            return ESP_OK;
        }
    }
    httpd_resp_sendstr(req, "BAD REQUEST");
    return ESP_FAIL;
}

esp_err_t ctl_handler(httpd_req_t *req) {
    if (strcmp(req->uri, "/pause")==0) {
        audio_manager_pause();
    } else if (strcmp(req->uri, "/resume")==0) {
        audio_manager_resume();
    } else if (strcmp(req->uri, "/next")==0) {
        audio_manager_next();
    } else if (strcmp(req->uri, "/previous")==0) {
        audio_manager_prev();
    }
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

esp_err_t current_handler(httpd_req_t *req) {
    const char *path = playlist_manager_get_current_track();
    if (!path) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"track\":\"%s\",\"index\":%d,\"total\":%d}",
             name,
             (int)playlist_manager_get_current_index(),
             (int)playlist_manager_get_track_count());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

void start_httpd() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&http_server, &config);
    httpd_uri_t list_uri = {"/list", HTTP_GET, list_handler, NULL};
    httpd_uri_t play_uri = {"/play", HTTP_GET, play_handler, NULL};
    httpd_uri_t pause_uri = {"/pause", HTTP_GET, ctl_handler, NULL};
    httpd_uri_t resume_uri = {"/resume", HTTP_GET, ctl_handler, NULL};
    httpd_uri_t next_uri = {"/next", HTTP_GET, ctl_handler, NULL};
    httpd_uri_t prev_uri = {"/previous", HTTP_GET, ctl_handler, NULL};
    httpd_uri_t current_uri = {"/current", HTTP_GET, current_handler, NULL};
    httpd_register_uri_handler(http_server, &list_uri);
    httpd_register_uri_handler(http_server, &play_uri);
    httpd_register_uri_handler(http_server, &pause_uri);
    httpd_register_uri_handler(http_server, &resume_uri);
    httpd_register_uri_handler(http_server, &next_uri);
    httpd_register_uri_handler(http_server, &prev_uri);
    httpd_register_uri_handler(http_server, &current_uri);
}


void app_main(void) {
    ESP_LOGI(TAG, "Init NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_wifi_ap();


    ESP_LOGI(TAG, "Init playlist manager");
    if (playlist_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Playlist manager failed");
        return;
    }

        start_httpd();

    ESP_ERROR_CHECK(bt_control_init());

    ESP_LOGI(TAG, "Start audio playback");
    if (audio_manager_start() != ESP_OK) {
        ESP_LOGE(TAG, "Audio pipeline failed to start");
        return;
    }



}

