// playlist_manager.c
#include "playlist_manager.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_random.h"
#include "path_config.h"

#define MAX_TRACKS 512
#define NVS_NAMESPACE "playlist"
#define NVS_KEY_ORDER "shuffle_order"

static const char *TAG = "playlist_mgr";
static char *track_list[MAX_TRACKS];
static size_t shuffle_order[MAX_TRACKS];
static size_t track_count = 0;
static size_t current_index = 0;

static esp_err_t scan_directory(void) {
    DIR *dir = opendir(MP3_DIR);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open dir: %s", MP3_DIR);
        return ESP_FAIL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) && track_count < MAX_TRACKS) {
        if (strstr(entry->d_name, ".mp3")) {
            size_t len = strlen(entry->d_name) + strlen(MP3_DIR) + 2;
            track_list[track_count] = malloc(len);
            snprintf(track_list[track_count], len, "%s/%s", MP3_DIR, entry->d_name);
            track_count++;
        }
    }
    closedir(dir);
    ESP_LOGI(TAG, "Found %d tracks", track_count);
    return ESP_OK;
}

static void shuffle_tracks(void) {
    if (track_count == 0) {
        current_index = 0;
        return;
    }
    for (size_t i = 0; i < track_count; i++) {
        shuffle_order[i] = i;
    }
    for (size_t i = track_count - 1; i > 0; i--) {
        size_t j = esp_random() % (i + 1);
        size_t tmp = shuffle_order[i];
        shuffle_order[i] = shuffle_order[j];
        shuffle_order[j] = tmp;
    }
    current_index = 0;
}

static esp_err_t save_shuffle_order(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, NVS_KEY_ORDER, shuffle_order, sizeof(size_t) * track_count);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    ESP_LOGI(TAG, "Shuffle saved to NVS");
    return err;
}

static esp_err_t load_shuffle_order(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t required_size = sizeof(size_t) * track_count;
    err = nvs_get_blob(handle, NVS_KEY_ORDER, shuffle_order, &required_size);
    nvs_close(handle);
    if (err == ESP_OK && required_size == sizeof(size_t) * track_count) {
        ESP_LOGI(TAG, "Shuffle loaded from NVS");
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t playlist_manager_init(void) {
esp_err_t err;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5
    };
    sdmmc_card_t* card;
    if ((err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot, &mount_cfg, &card)) != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(err));
        return err;
    } else {
        ESP_LOGI(TAG, "Carte montée");
    }
    ESP_LOGI(TAG, "Initializing playlist manager");
    esp_err_t scan_err = scan_directory();
    if (scan_err != ESP_OK) return scan_err;

    if (track_count == 0) {
        ESP_LOGW(TAG, "No MP3 files found");
        return ESP_ERR_NOT_FOUND;
    }

    if (load_shuffle_order() != ESP_OK) {
        shuffle_tracks();
        save_shuffle_order();
    }
    return ESP_OK;
}

const char *playlist_manager_get_next(void) {
    if (track_count == 0) {
        return NULL;
    }
    if (current_index >= track_count) {
        shuffle_tracks();
        save_shuffle_order();
    }
    return track_list[shuffle_order[current_index++]];
}

void playlist_manager_reset(void) {
    current_index = 0;
}

size_t playlist_manager_get_track_count(void) {
    return track_count;
}

size_t playlist_manager_get_current_index(void) {
    return current_index;
}

const char *playlist_manager_get_current_track(void) {
    if (track_count == 0) {
        return NULL;
    }
    size_t idx = current_index > 0 ? current_index - 1 : 0;
    return track_list[shuffle_order[idx]];
}

const char *playlist_manager_get_prev(void) {
    if (track_count == 0) {
        return NULL;
    }
    if (current_index == 0) {
        current_index = track_count;
    }
    current_index--;
    return track_list[shuffle_order[current_index]];
}

esp_err_t playlist_manager_set_current_by_name(const char *filename) {
    if (!filename) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < track_count; i++) {
        const char *path = track_list[i];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        if (strcmp(name, filename) == 0) {
            for (size_t j = 0; j < track_count; j++) {
                if (shuffle_order[j] == i) {
                    current_index = j + 1;
                    return ESP_OK;
                }
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

