#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "common.h"

static const char *TAG = "LOGGER_TASK";
#define LOG_FILE "/spiffs/sensor_log.csv"

/*
 * Mount SPIFFS (flash-based filesystem). This is the "software-only"
 * stand-in for an SD card -- same CSV-writing logic applies if you
 * later switch to an SD card over SPI (just mount with sdspi instead).
 */
static esp_err_t init_storage(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    // Write CSV header if file doesn't exist yet
    FILE *f = fopen(LOG_FILE, "r");
    if (f == NULL) {
        f = fopen(LOG_FILE, "w");
        if (f) {
            fprintf(f, "timestamp_ms,temperature,humidity,accel_x,accel_y,accel_z\n");
            fclose(f);
        }
    } else {
        fclose(f);
    }

    return ESP_OK;
}

/*
 * SD_Log_Task
 * Priority: 1 (lower than sensor task -- logging can lag slightly
 *              behind sensing without consequence)
 *
 * Blocks on sd_log_queue, appends each received sample as a CSV row.
 */
void sd_log_task(void *pvParameters)
{
    sensor_data_t sample;

    if (init_storage() != ESP_OK) {
        ESP_LOGE(TAG, "Storage init failed, log task exiting");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "SPIFFS mounted, logging to %s", LOG_FILE);

    while (1) {
        // Block indefinitely until a sample arrives
        if (xQueueReceive(sd_log_queue, &sample, portMAX_DELAY) == pdTRUE) {
            FILE *f = fopen(LOG_FILE, "a");
            if (f) {
                fprintf(f, "%lld,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                        sample.timestamp_ms, sample.temperature, sample.humidity,
                        sample.accel_x, sample.accel_y, sample.accel_z);
                fclose(f);
                ESP_LOGI(TAG, "Logged sample at t=%lld ms", sample.timestamp_ms);
            } else {
                ESP_LOGE(TAG, "Failed to open log file for append");
            }
        }
    }
}
