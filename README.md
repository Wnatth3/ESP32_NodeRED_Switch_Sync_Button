# ESP32 Node-RED Switch Sync Button

This sketch enables an ESP32 to control and synchronize a light switch via MQTT, with configuration through a captive WiFi portal. It supports state persistence, MQTT communication, and physical button control.

## Prerequisites

- **Hardware:**
    - ESP32 development board
    - Push buttons (for reset and light control)
    - LED (status indicator)
    - Light relay or output device on GPIO 4

- **Libraries:**
    - [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
    - [WiFiManager](https://github.com/tzapu/WiFiManager)
    - LittleFS
    - [PubSubClient](https://github.com/knolleary/pubsubclient)
    - [Button2](https://github.com/LennartHennigs/Button2)
    - [ezLED](https://github.com/raphaelbs/ezLED)
    - [TaskScheduler](https://github.com/arkhipenko/TaskScheduler)

- **Platform:**
    - Arduino IDE or PlatformIO with ESP32 board support

## Features

- **WiFi Configuration Portal:**  
    Automatic captive portal for WiFi and MQTT credentials using WiFiManager.

- **MQTT Integration:**  
    Publishes light state and subscribes to light control commands.

- **State Persistence:**  
    Saves and loads light state and configuration using LittleFS.

- **Physical Button Control:**  
    - Long press (5s) on reset button: clears WiFi/MQTT settings and restarts.
    - Tap on light button: toggles light and syncs state via MQTT.

- **Status LED:**  
    Indicates connection and operation status.

- **Task Scheduling:**  
    Uses TaskScheduler for non-blocking WiFi and MQTT management.

- **Debugging:**  
    Optional debug output for troubleshooting.

## Usage

1. Flash the sketch to your ESP32.
2. On first boot or after reset, connect to the WiFi AP named `MyESP32` and configure WiFi and MQTT settings.
3. Use the physical buttons to control and reset the device.
4. Integrate with Node-RED or any MQTT broker using the topics:
     - Command: `esp32/switch/light/command`
     - State:   `esp32/switch/light/state`
