#include <Arduino.h>
#include <FS.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Button2.h>
#include <ezLED.h>
#include <TaskScheduler.h>

//******************************** Configulation ****************************//
#define _DEBUG_  // Comment this line if you don't want to debug
#include "Debug.h"

// #define CUSTOM_IP  // Uncomment this line if you want to use DHCP

//******************************** Variables & Objects **********************//
//----------------- TaskScheduler ------------------//
Scheduler ts;

#define deviceName "MyESP32"

//----------------- esLED ---------------------//
#define led      LED_BUILTIN
#define lightPin 4

const char* stateFile  = "/state.txt";
bool        lightState = false;

ezLED statusLed(led);
// ezLED light(lightPin);

//----------------- Reset WiFi Button ---------//
#define resetWifiBtPin 0
Button2 resetWifiBt;

#define lightBtPin 33
Button2 lightBt;

//----------------- WiFi Manager --------------//
const char* filename = "/config.txt";  // Config file name

#ifdef CUSTOM_IP
// default custom static IP
char static_ip[16]  = "192.168.0.191";
char static_gw[16]  = "192.168.0.1";
char static_sn[16]  = "255.255.255.0";
char static_dns[16] = "1.1.1.1";
#endif
char mqttBroker[16] = "192.168.0.10";
char mqttPort[6]    = "1883";
char mqttUser[10];
char mqttPass[10];

bool mqttParameter;

WiFiManager wifiManager;

WiFiManagerParameter customMqttBroker("broker", "mqtt server", mqttBroker, 16);
WiFiManagerParameter customMqttPort("port", "mqtt port", mqttPort, 6);
WiFiManagerParameter customMqttUser("user", "mqtt user", mqttUser, 10);
WiFiManagerParameter customMqttPass("pass", "mqtt pass", mqttPass, 10);

//----------------- MQTT ----------------------//
WiFiClient   espClient;
PubSubClient mqtt(espClient);

#define subLightCommand "esp32/switch/light/command"
#define pubLightState   "esp32/switch/light/state"

//******************************** Tasks ************************************//
void connectMqtt();
void reconnectMqtt();
Task tWifiManager(TASK_IMMEDIATE, TASK_FOREVER, []() { wifiManager.process(); }, &ts, true);  // Insert the process() into the task scheduler
Task tConnectMqtt(TASK_IMMEDIATE, TASK_FOREVER, &connectMqtt, &ts, true);
Task tReconnectMqtt(3000, TASK_FOREVER, &reconnectMqtt, &ts, false);

//******************************** Functions ********************************//
//----------------- LittleFS ------------------//
// Loads the configuration from a file
void loadConfiguration(fs::FS& fs, const char* filename) {
    _delnF("Loading configuration");
    // Open file for reading
    File file = fs.open(filename, "r");
    if (!file) {
        _delnF("Failed to open data file");
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error) { _delnF("Failed to read file, using default configuration"); }
    // Copy values from the JsonDocument to the Config
    // strlcpy(Destination_Variable, doc["Source_Variable"] /*| "Default_Value"*/, sizeof(Destination_Name));
    strlcpy(mqttBroker, doc["mqttBroker"], sizeof(mqttBroker));
    strlcpy(mqttPort, doc["mqttPort"], sizeof(mqttPort));
    strlcpy(mqttUser, doc["mqttUser"], sizeof(mqttUser));
    strlcpy(mqttPass, doc["mqttPass"], sizeof(mqttPass));
    mqttParameter = doc["mqttParameter"];

#ifdef CUSTOM_IP
    if (doc["ip"]) {
        strlcpy(static_ip, doc["ip"], sizeof(static_ip));
        strlcpy(static_gw, doc["gateway"], sizeof(static_gw));
        strlcpy(static_sn, doc["subnet"], sizeof(static_sn));
        strlcpy(static_dns, doc["dns"], sizeof(static_dns));
    } else {
        _delnF("No custom IP in config file");
    }
#endif

    file.close();
}

void loadState(const char* fileName) {
    File file = LittleFS.open(fileName, "r");
    if (!file) {
        _deln("Failed to open " + String(fileName));
        return;
    }

    JsonDocument         doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) _deln("Failed to read " + String(fileName) + ", using default configuration");
    lightState = doc["lightState"];

    file.close();
}

void saveState(const char* fileName) {
    File file = LittleFS.open(fileName, "w");
    if (!file) {
        _deln("Failed to open " + String(fileName) + " for writing");
        return;
    }

    JsonDocument doc;
    doc["lightState"] = lightState;

    if (serializeJson(doc, file) == 0) _deln("Failed to write to " + String(fileName));

    file.close();
}

void deviceInit() {
    digitalWrite(lightPin, lightState);
}

void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (String(topic) == subLightCommand) {
        digitalWrite(lightPin, lightState = message == "ON" ? true : false);
        saveState(stateFile);
    }
}

void mqttInit() {
    _deF("MQTT parameters are ");
    if (mqttParameter) {
        _delnF("available");
        mqtt.setCallback(handleMqttMessage);
        mqtt.setServer(mqttBroker, atoi(mqttPort));
        // tConnectMqtt.start();
    } else {
        _delnF("not available.");
    }
}

void saveParamsCallback() {
    // Pull the values from WiFi portal form.
    strcpy(mqttBroker, customMqttBroker.getValue());
    strcpy(mqttPort, customMqttPort.getValue());
    strcpy(mqttUser, customMqttUser.getValue());
    strcpy(mqttPass, customMqttPass.getValue());

    _delnF("The values are updated.");

    // Delete existing file, otherwise the configuration is appended to the file
    // LittleFS.remove(filename);
    File file = LittleFS.open(filename, "w");
    if (!file) {
        _delnF("Failed to open config file for writing");
        return;
    }

    // Allocate a temporary JsonDocument
    JsonDocument doc;
    // Set the values in the document
    doc["mqttBroker"] = mqttBroker;
    doc["mqttPort"]   = mqttPort;
    doc["mqttUser"]   = mqttUser;
    doc["mqttPass"]   = mqttPass;

    if (doc["mqttBroker"] != "") {
        doc["mqttParameter"] = true;
        mqttParameter        = doc["mqttParameter"];
    }
#ifdef CUSTOM_IP
    doc["ip"]      = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"]  = WiFi.subnetMask().toString();
    doc["dns"]     = WiFi.dnsIP().toString();
#endif
    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) {
        _delnF("Failed to write to file");
    } else {
        _delnF("Configuration saved successfully");
    }

    file.close();  // Close the file

    mqttInit();
}

void printFile(fs::FS& fs, const char* filename) {
    _delnF("Print config file...");
    File file = fs.open(filename, "r");
    if (!file) {
        _delnF("Failed to open data file");
        return;
    }

    JsonDocument         doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) _delnF("Failed to read file");

    char buffer[512];
    serializeJsonPretty(doc, buffer);
    _delnF(buffer);

    file.close();
}

void deleteFile(fs::FS& fs, const char* path) {
    _deVarln("Delete file: ", path);
    if (fs.remove(path)) {
        _delnF("- file deleted");
    } else {
        _delnF("- delete failed");
    }
}

void wifiManagerSetup() {
    loadConfiguration(LittleFS, filename);
#ifdef _DEBUG_
    printFile(LittleFS, filename);
#endif

    // reset settings - wipe credentials for testing
    // wifiManager.resetSettings();

#ifdef CUSTOM_IP
    // set static ip
    IPAddress _ip, _gw, _sn, _dns;
    _ip.fromString(static_ip);
    _gw.fromString(static_gw);
    _sn.fromString(static_sn);
    _dns.fromString(static_dns);
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns);
#endif

    wifiManager.addParameter(&customMqttBroker);
    wifiManager.addParameter(&customMqttPort);
    wifiManager.addParameter(&customMqttUser);
    wifiManager.addParameter(&customMqttPass);

    wifiManager.setDarkMode(true);
#ifndef _DEBUG_
    wifiManager.setDebugOutput(true, WM_DEBUG_SILENT);
#endif
    // wifiManager.setDebugOutput(true, WM_DEBUG_DEV);
    // wifiManager.setMinimumSignalQuality(20); // Default: 8%
    wifiManager.setConnectTimeout(10);
    wifiManager.setConfigPortalTimeout(60);
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setSaveParamsCallback(saveParamsCallback);

    // automatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if (wifiManager.autoConnect(deviceName, "password")) {
        _delnF("WiFI is connected :D");
    } else {
        _delnF("Configportal running");
    }
}

void subscribeMqtt() {
    _delnF("Subscribing to the MQTT topics...");
    mqtt.subscribe(subLightCommand);
}

void publishMqtt() {
    _delnF("Publishing to the MQTT topics...");
    mqtt.publish(pubLightState, lightState ? "ON" : "OFF");
}

//----------------- Connect MQTT --------------//
void reconnectMqtt() {
    if (WiFi.status() == WL_CONNECTED) {
        _deVar("MQTT Broker: ", mqttBroker);
        _deVar(" | Port: ", mqttPort);
        _deVar(" | User: ", mqttUser);
        _deVarln(" | Pass: ", mqttPass);
        _deF("Connecting MQTT... ");
        if (mqtt.connect(deviceName, mqttUser, mqttPass)) {
            _delnF("Connected");
            tReconnectMqtt.disable();
            tConnectMqtt.enable();
            statusLed.blinkNumberOfTimes(200, 200, 3);  // 250ms ON, 750ms OFF, repeat 3 times, blink immediately
            subscribeMqtt();
            publishMqtt();
        } else {  //
            _deVar("failed state: ", mqtt.state());
            _deVarln(" | counter: ", tReconnectMqtt.getRunCounter());
            if (tReconnectMqtt.getRunCounter() > 3) {
                tReconnectMqtt.disable();
                tConnectMqtt.setInterval(60000L);  // Wait 60 seconds before reconnecting.
                tConnectMqtt.enableDelayed();
            }
        }
    } else {
        if (tReconnectMqtt.isFirstIteration()) {
            _delnF("WiFi is not connected");
        }
    }
}

void connectMqtt() {
    if (!mqtt.connected()) {
        tConnectMqtt.disable();
        tReconnectMqtt.enable();
    } else {
        mqtt.loop();
    }
}

//----------------- Reset WiFi Button ---------//
void resetWifiBtPressed(Button2& btn) {
    statusLed.turnON();
    _delnF("Deleting the config file and resetting WiFi.");
    deleteFile(LittleFS, filename);
    // deleteFile(LittleFS, stateFile);
    wifiManager.resetSettings();
    _deF(deviceName);
    _delnF(" is restarting.");
    delay(3000);
    ESP.restart();
}

void toggleLight(Button2& btn) {
    // light.toggle();
    lightState = !lightState;
    digitalWrite(lightPin, lightState);
    saveState(stateFile);
    mqtt.publish(pubLightState, lightState ? "ON" : "OFF");
}

void setup() {
    _serialBegin(115200);
    statusLed.turnOFF();
    pinMode(lightPin, OUTPUT);

    resetWifiBt.begin(resetWifiBtPin);
    resetWifiBt.setLongClickTime(5000);
    resetWifiBt.setLongClickDetectedHandler(resetWifiBtPressed);

    lightBt.begin(lightBtPin);
    lightBt.setTapHandler(toggleLight);

    while (!LittleFS.begin(true)) {  // true = format if failed
        _delnF("Failed to initialize LittleFS library");
        delay(1000);
    }

    loadState(stateFile);
    deviceInit();

    wifiManagerSetup();
    mqttInit();
}

void loop() {
    ts.execute();
    statusLed.loop();
    resetWifiBt.loop();
    lightBt.loop();
}
