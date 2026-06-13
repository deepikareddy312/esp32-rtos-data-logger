#ifndef PROJECT_COMMON_H
#define PROJECT_COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Structure that holds one set of sensor readings
typedef struct {
    int64_t  timestamp_ms;
    float    temperature;   // from DHT22
    float    humidity;      // from DHT22
    float    accel_x;       // from MPU6050
    float    accel_y;
    float    accel_z;
} sensor_data_t;

// Queue used to send sensor readings from Sensor_Read_Task
// to both SD_Log_Task and WiFi_Publish_Task
extern QueueHandle_t sd_log_queue;
extern QueueHandle_t mqtt_queue;

// Mutex protecting I2C bus access (shared by DHT22 + MPU6050 drivers)
extern SemaphoreHandle_t i2c_mutex;

// Task function prototypes
void sensor_read_task(void *pvParameters);
void sd_log_task(void *pvParameters);
void mqtt_publish_task(void *pvParameters);

#endif // PROJECT_COMMON_H
