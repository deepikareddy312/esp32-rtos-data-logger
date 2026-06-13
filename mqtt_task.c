#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "common.h"

static const char *TAG = "MQTT_TASK";

// ---- EDIT THESE before flashing ----
#define WIFI_SSID      "Wokwi-GUEST"   // Wokwi's built-in simulated Wi-Fi network
#define WIFI_PASS      ""              // Wokwi-GUEST has no password
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883" // free public broker
#define MQTT_TOPIC     "ece_student/embedded_logger/data"
// -------------------------------------

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi connected, got IP");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            mqtt_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            mqtt_connected = false;
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    mqtt_event_handler_cb((esp_mqtt_event_handle_t)event_data);
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/*
 * WiFi_Publish_Task
 * Priority: 1
 *
 * - Connects to Wi-Fi and MQTT broker once at startup
 * - Blocks on mqtt_queue, publishes each sample as JSON
 * - If Wi-Fi/MQTT is down, samples are simply dropped (queue overwrites)
 *   rather than blocking the rest of the system
 */
void mqtt_publish_task(void *pvParameters)
{
    sensor_data_t sample;
    char payload[200];

    wifi_init();
    mqtt_init();

    while (1) {
        if (xQueueReceive(mqtt_queue, &sample, portMAX_DELAY) == pdTRUE) {
            if (mqtt_connected) {
                snprintf(payload, sizeof(payload),
                    "{\"timestamp\":%lld,\"temperature\":%.2f,\"humidity\":%.2f,"
                    "\"accel_x\":%.2f,\"accel_y\":%.2f,\"accel_z\":%.2f}",
                    sample.timestamp_ms, sample.temperature, sample.humidity,
                    sample.accel_x, sample.accel_y, sample.accel_z);

                int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, payload, 0, 1, 0);
                ESP_LOGI(TAG, "Published msg_id=%d: %s", msg_id, payload);
            } else {
                ESP_LOGW(TAG, "MQTT not connected, dropping sample");
            }
        }
    }
}
