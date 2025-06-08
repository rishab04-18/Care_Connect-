#include "config.h"  // Adafruit IO Config 
#include <DHT.h>
#include <WiFi.h>
#include <time.h>

// Pin Definitions
#define DHTPIN 4
#define DHTTYPE DHT22
#define SOIL_SENSOR 34
#define LDR_SENSOR 35
#define PLANT_LIGHT 26
#define UV_LIGHT 27
#define WATER_PUMP 25
#define FAN 33

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // 19800 for IST (GMT+5:30)
const int   daylightOffset_sec = 0;

// Adafruit IO Feeds
AdafruitIO_Feed *soilMoistureFeed = io.feed("soil-moisture");
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed = io.feed("humidity");
AdafruitIO_Feed *waterPumpFeed = io.feed("water-pump");
AdafruitIO_Feed *fanFeed = io.feed("fan");
AdafruitIO_Feed *lightFeed = io.feed("light-intensity");

// Manual Control Feeds
AdafruitIO_Feed *plantLightControl = io.feed("plant-light-control");
AdafruitIO_Feed *uvLightControl = io.feed("uv-light-control");
AdafruitIO_Feed *pumpControl = io.feed("pump-control");
AdafruitIO_Feed *fanControl = io.feed("fan-control");

DHT dht(DHTPIN, DHTTYPE);

// Manual Override States
bool plantLightManual = false;
bool uvLightManual = false;
bool pumpManual = false;
bool fanManual = false;

// Callback Functions for Manual Overrides
void handlePlantLight(AdafruitIO_Data *data) { plantLightManual = data->toInt(); }
void handleUVLight(AdafruitIO_Data *data) { uvLightManual = data->toInt(); }
void handlePump(AdafruitIO_Data *data) { pumpManual = data->toInt(); }
void handleFan(AdafruitIO_Data *data) { fanManual = data->toInt(); }

void setup() {
    Serial.begin(115200);
    dht.begin();
    
    // Setup pins
    pinMode(SOIL_SENSOR, INPUT);
    pinMode(LDR_SENSOR, INPUT);
    pinMode(PLANT_LIGHT, OUTPUT);
    pinMode(UV_LIGHT, OUTPUT);
    pinMode(WATER_PUMP, OUTPUT);
    pinMode(FAN, OUTPUT);

    // Ensure relays are OFF at startup
    digitalWrite(PLANT_LIGHT, HIGH);
    digitalWrite(UV_LIGHT, HIGH);
    digitalWrite(WATER_PUMP, HIGH);
    digitalWrite(FAN, HIGH);

    // Connect to Adafruit IO
    io.connect();
    
    // Attach callbacks
    plantLightControl->onMessage(handlePlantLight);
    uvLightControl->onMessage(handleUVLight);
    pumpControl->onMessage(handlePump);
    fanControl->onMessage(handleFan);

    // Subscribe to feeds
    plantLightControl->get();
    uvLightControl->get();
    pumpControl->get();
    fanControl->get();
    
    // Set time using NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
    io.run();
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    int currentHour = timeinfo.tm_hour;
    
    int moisture = analogRead(SOIL_SENSOR);
    int soilMoisture = (100 - ((moisture / 4095.00) * 100));
    int ldr = analogRead(LDR_SENSOR);
    int ldrValue = (100.0 * ldr) / 4095.0;
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("DHT Sensor Read Failed!");
        return;
    }

    Serial.printf("Date & Time: %04d-%02d-%02d %02d:%02d:%02d | Temp: %.1f°C | Humidity: %.1f%%\n", 
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                  temperature, humidity);
    Serial.printf("Soil Moisture: %d | LDR: %d\n", soilMoisture, ldrValue);

    // Time-Based Light Control (Day Cycle: 6 AM - 6 PM)
    bool isDaytime = (currentHour >= 6 && currentHour < 18);

    // Adaptive Light Control (ON/OFF based on LDR)
    bool lightState = (ldrValue < 1500); // LDR < 1500 means it's dark
    digitalWrite(PLANT_LIGHT, (plantLightManual ? LOW : (isDaytime && lightState ? LOW : HIGH)));

    // UV Light Control (Humidity > 70%)
    bool uvLightState = (humidity > 70);
    digitalWrite(UV_LIGHT, uvLightManual ? LOW : (uvLightState ? LOW : HIGH));

    // Water Pump Control (Soil Moisture)
    bool pumpState = (soilMoisture > 70);
    digitalWrite(WATER_PUMP, pumpManual ? LOW : (pumpState ? LOW : HIGH));

    // Temperature-Based Fan Control
    bool fanState = (temperature > 30);
    digitalWrite(FAN, fanManual ? LOW : (fanState ? LOW : HIGH));

    // Publish Data to Adafruit IO
    soilMoistureFeed->save(soilMoisture);
    lightFeed->save(ldrValue);
    temperatureFeed->save(temperature);
    humidityFeed->save(humidity);
    waterPumpFeed->save(pumpState ? "ON" : "OFF");
    fanFeed->save(fanState ? "ON" : "OFF");

    delay(15000);  // Update every 15 seconds
}
