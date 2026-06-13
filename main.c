#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "common.h"

static const char *TAG = "MAIN";

// Global objects shared across tasks (declared extern in common.h)
QueueHandle_t sd_log_queue   = NULL;
QueueHandle_t mqtt_queue     = NULL;
SemaphoreHandle_t i2c_mutex  = NULL;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting RTOS Multi-Sensor Data Logger");

    // Each queue holds up to 10 samples (10 seconds of buffering at 1Hz)
    sd_log_queue = xQueueCreate(10, sizeof(sensor_data_t));
    mqtt_queue   = xQueueCreate(10, sizeof(sensor_data_t));

    // Mutex protecting shared I2C bus between DHT22 and MPU6050 drivers
    i2c_mutex = xSemaphoreCreateMutex();

    if (!sd_log_queue || !mqtt_queue || !i2c_mutex) {
        ESP_LOGE(TAG, "Failed to create queues/mutex, halting");
        return;
    }

    // Task creation: (function, name, stack size, params, priority, handle)
    // Higher number = higher priority in FreeRTOS

    xTaskCreate(sensor_read_task,  "Sensor_Read_Task",  4096, NULL, 3, NULL);
    xTaskCreate(sd_log_task,       "SD_Log_Task",       4096, NULL, 2, NULL);
    //xTaskCreate(mqtt_publish_task, "WiFi_Publish_Task", 6144, NULL, 1, NULL);

    ESP_LOGI(TAG, "All tasks created successfully");
}
