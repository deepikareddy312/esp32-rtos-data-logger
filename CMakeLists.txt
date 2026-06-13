idf_component_register(SRCS "main.c" "sensor_task.c" "logger_task.c" 
                        INCLUDE_DIRS "."
                        REQUIRES driver esp_wifi nvs_flash spiffs esp_event esp_netif esp_timer)
