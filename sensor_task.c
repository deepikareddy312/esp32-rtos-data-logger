#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "common.h"

static const char *TAG = "SENSOR_TASK";

/*
 * In a real build, these would call into actual driver code
 * (DHT22 single-wire read, MPU6050 I2C register reads).
 * For now they return simulated values so the RTOS architecture
 * can be developed and tested independently of sensor drivers.
 * Replace the bodies of these two functions with real I2C/GPIO
 * calls once the driver code (sensor_drivers.c) is ready.
 */
static void read_dht22(float *temperature, float *humidity)
{
    // TODO: replace with real DHT22 single-wire protocol read
    *temperature = 25.0f + (float)(esp_random() % 100) / 20.0f; // ~25-30 C
    *humidity    = 50.0f + (float)(esp_random() % 200) / 10.0f; // ~50-70 %
}

static void read_mpu6050(float *ax, float *ay, float *az)
{
    // TODO: replace with real MPU6050 I2C register read (0x3B-0x40)
    *ax = (float)(esp_random() % 200 - 100) / 100.0f; // -1.0 to 1.0 g
    *ay = (float)(esp_random() % 200 - 100) / 100.0f;
    *az = 1.0f + (float)(esp_random() % 50) / 100.0f; // ~1g (gravity)
}

/*
 * Sensor_Read_Task
 * Priority: 2 (higher than logging/publish tasks)
 * Period:   1000 ms
 *
 * Responsibilities:
 *  - Take I2C mutex before talking to sensors (DHT22 + MPU6050
 *    may share the bus on real hardware)
 *  - Build a sensor_data_t sample
 *  - Send a COPY of the sample to both sd_log_queue and mqtt_queue
 *    so the two consumer tasks operate independently
 */
void sensor_read_task(void *pvParameters)
{
    sensor_data_t sample;
    const TickType_t period = pdMS_TO_TICKS(1000);
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        // Protect shared I2C bus access between sensor reads
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            read_dht22(&sample.temperature, &sample.humidity);
            read_mpu6050(&sample.accel_x, &sample.accel_y, &sample.accel_z);
            xSemaphoreGive(i2c_mutex);
        } else {
            ESP_LOGW(TAG, "Could not acquire I2C mutex, skipping cycle");
            vTaskDelayUntil(&last_wake_time, period);
            continue;
        }

        sample.timestamp_ms = esp_timer_get_time() / 1000;

        ESP_LOGI(TAG, "T=%.2fC H=%.2f%% Ax=%.2f Ay=%.2f Az=%.2f",
                 sample.temperature, sample.humidity,
                 sample.accel_x, sample.accel_y, sample.accel_z);

        // Non-blocking sends: if a queue is full, drop the sample
        // rather than blocking the high-priority sensor task
        if (xQueueSend(sd_log_queue, &sample, 0) != pdTRUE) {
            ESP_LOGW(TAG, "sd_log_queue full, dropping sample");
        }
        //if (xQueueSend(mqtt_queue, &sample, 0) != pdTRUE) {
           // ESP_LOGW(TAG, "mqtt_queue full, dropping sample");
       //}

        // Maintain a precise 1-second period regardless of how
        // long the work above took
        vTaskDelayUntil(&last_wake_time, period);
    }
}
