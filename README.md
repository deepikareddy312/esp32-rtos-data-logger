# RTOS-based Multi-Sensor Data Logger with IoT Connectivity

A FreeRTOS application for ESP32 that reads data from multiple sensors (DHT22 temperature/humidity, MPU6050 accelerometer), logs it to flash storage as CSV, and publishes it to a cloud MQTT broker in real time. Developed and validated using the Wokwi ESP32 simulator.

## Architecture

```
                 ┌─────────────────────┐
                 │  Sensor_Read_Task   │  Priority 3
                 │  (1 Hz, I2C mutex)  │
                 └─────────┬───────────┘
                            │
              ┌─────────────┴─────────────┐
              ▼                            ▼
     ┌─────────────────┐         ┌─────────────────────┐
     │  sd_log_queue    │         │   mqtt_queue         │
     └────────┬─────────┘         └──────────┬───────────┘
              ▼                                ▼
     ┌─────────────────┐         ┌─────────────────────┐
     │  SD_Log_Task     │         │  WiFi_Publish_Task   │
     │  (SPIFFS, CSV)   │         │  (Wi-Fi + MQTT/JSON) │
     │  Priority 2      │         │  Priority 1          │
     └─────────────────┘         └─────────────────────┘
```

## RTOS Concepts Demonstrated
- **Tasks & Priorities**: Sensor reading is highest priority; logging and network publishing are lower priority consumers.
- **Queues**: Decouple sensor sampling rate from (potentially slower) storage and network operations. Non-blocking sends prevent the sensor task from ever stalling.
- **Mutex**: Protects shared I2C bus access between the DHT22 and MPU6050 drivers.
- **vTaskDelayUntil**: Maintains a precise 1 Hz sampling period regardless of jitter in task execution time.

## Hardware (Simulated)
- ESP32 DevKit
- DHT22 (temperature & humidity)
- MPU6050 (accelerometer/gyroscope)
- Flash storage (SPIFFS) as software-equivalent of SD card logging

All hardware is simulated in [Wokwi](https://wokwi.com) — see `diagram.json`. Architecture is directly portable to physical ESP32 hardware by replacing the simulated sensor read functions in `sensor_task.c` with real I2C/GPIO driver calls.

## Cloud Connectivity
- Connects to Wi-Fi (Wokwi-GUEST simulated network, which has real internet access)
- Publishes JSON sensor data to a public MQTT broker (HiveMQ)
- Can be visualized live using a free MQTT dashboard (e.g., HiveMQ WebSocket client, Node-RED, or ThingsBoard)

## How to Run
1. Open this project folder in VS Code with the ESP-IDF and Wokwi simulator extensions installed.
2. Build the project: `idf.py build`
3. Launch the Wokwi simulation (F1 → "Wokwi: Start Simulator")
4. Observe sensor readings, CSV logging, and MQTT publish messages in the serial monitor.
5. Subscribe to the topic `ece_student/embedded_logger/data` on a public MQTT client to see live data.

## Files
- `main/main.c` — task creation, queue/mutex setup
- `main/sensor_task.c` — Sensor_Read_Task (sensor sampling)
- `main/logger_task.c` — SD_Log_Task (SPIFFS CSV logging)
- `main/mqtt_task.c` — WiFi_Publish_Task (Wi-Fi + MQTT)
- `main/common.h` — shared data structures and RTOS object declarations
- `diagram.json` — Wokwi virtual circuit definition

## Next Steps / Improvements
- Replace simulated sensor reads with real DHT22/MPU6050 driver code
- Add deep sleep between samples for power optimization
- Add a watchdog task for system health monitoring
- Add OTA firmware update capability
