#include "../include/WifiLink.h"
#include "../include/types.h"
#include "../include/base64.h"
#include <Arduino.h>
#include <cstring>

// Используем ArduinoJson если доступна
#if __has_include(<ArduinoJson.h>)
#include <ArduinoJson.h>
#define HAS_ARDUINO_JSON 1
#else
#define HAS_ARDUINO_JSON 0
#endif

void WifiLink::begin() {
    // Инициализация Serial1 для связи с NodeMCU ESP8266
    // TX1 = pin 18, RX1 = pin 19
    Serial1.begin(Hardware::SERIAL1_BAUD);
    lineBufferPos = 0;
    
    Serial.println("WifiLink: Serial1 initialized for NodeMCU communication");
    Serial.print("WifiLink: Baud rate = ");
    Serial.println(Hardware::SERIAL1_BAUD);
}

void WifiLink::sendData(uint32_t sessionId, uint32_t stepId,
                        const DateTime& ts,
                        const SensorSnapshot& sensors,
                        const ImageSnapshot& image) {
    
    // Формирование временной метки
    char timestampStr[32];
    formatTimestamp(ts, timestampStr, sizeof(timestampStr));
    
#if HAS_ARDUINO_JSON
    // Используем ArduinoJson для формирования JSON
    StaticJsonDocument<1024> doc;
    
    doc["session_id"] = sessionId;
    doc["step"] = stepId;
    doc["timestamp"] = timestampStr;
    
    JsonObject sensorsObj = doc.createNestedObject("sensors");
    sensorsObj["distance_cm"] = sensors.distanceCm;
    sensorsObj["obstacle"] = sensors.obstacle;
    sensorsObj["light_raw"] = sensors.lightRaw;
    sensorsObj["light_dark"] = sensors.isDark;
    
    JsonObject mpuObj = sensorsObj.createNestedObject("mpu6050");
    mpuObj["ax"] = sensors.ax;
    mpuObj["ay"] = sensors.ay;
    mpuObj["az"] = sensors.az;
    mpuObj["gx"] = sensors.gx;
    mpuObj["gy"] = sensors.gy;
    mpuObj["gz"] = sensors.gz;
    
    JsonObject imageObj = doc.createNestedObject("image");
    imageObj["available"] = image.available;
    imageObj["width"] = image.width;
    imageObj["height"] = image.height;
    imageObj["format"] = "GRAY8";
    
    // Отправляем JSON (без изображения в base64 - слишком большое)
    Serial1.print("DATA ");
    serializeJson(doc, Serial1);
    Serial1.println();
    
#else
    // Формируем JSON вручную без библиотеки
    Serial1.print("DATA {");
    Serial1.print("\"session_id\":");
    Serial1.print(sessionId);
    Serial1.print(",\"step\":");
    Serial1.print(stepId);
    Serial1.print(",\"timestamp\":\"");
    Serial1.print(timestampStr);
    Serial1.print("\",\"sensors\":{");
    Serial1.print("\"distance_cm\":");
    Serial1.print(sensors.distanceCm, 1);
    Serial1.print(",\"obstacle\":");
    Serial1.print(sensors.obstacle ? "true" : "false");
    Serial1.print(",\"light_raw\":");
    Serial1.print(sensors.lightRaw);
    Serial1.print(",\"light_dark\":");
    Serial1.print(sensors.isDark ? "true" : "false");
    Serial1.print(",\"mpu6050\":{");
    Serial1.print("\"ax\":");
    Serial1.print(sensors.ax, 2);
    Serial1.print(",\"ay\":");
    Serial1.print(sensors.ay, 2);
    Serial1.print(",\"az\":");
    Serial1.print(sensors.az, 2);
    Serial1.print(",\"gx\":");
    Serial1.print(sensors.gx, 2);
    Serial1.print(",\"gy\":");
    Serial1.print(sensors.gy, 2);
    Serial1.print(",\"gz\":");
    Serial1.print(sensors.gz, 2);
    Serial1.print("}},\"image\":{");
    Serial1.print("\"available\":");
    Serial1.print(image.available ? "true" : "false");
    Serial1.print(",\"width\":");
    Serial1.print(image.width);
    Serial1.print(",\"height\":");
    Serial1.print(image.height);
    Serial1.print(",\"format\":\"GRAY8\"");
    Serial1.println("}}");
#endif

    // Если изображение доступно, отправляем отдельным пакетом
    if (image.available && image.buffer != nullptr && image.bufferSize > 0) {
        Serial1.print("IMAGE ");
        Serial1.print(image.width);
        Serial1.print(" ");
        Serial1.print(image.height);
        Serial1.print(" ");
        sendImageBase64(image.buffer, image.bufferSize);
        Serial1.println();
    }
}

bool WifiLink::waitForCommand(Command& outCmd, uint32_t timeoutMs) {
    // Инициализация структуры команды
    memset(outCmd.name, 0, sizeof(outCmd.name));
    outCmd.durationMs = 0;
    
    // Чтение строки из Serial1
    char buffer[256];
    if (!readLine(buffer, sizeof(buffer), timeoutMs)) {
        return false; // Таймаут или ошибка чтения
    }
    
    // Проверка формата команды: должна начинаться с "CMD "
    if (strncmp(buffer, "CMD ", 4) != 0) {
        return false; // Неверный формат
    }
    
    // Извлечение JSON части (после "CMD ")
    const char* jsonStr = buffer + 4;
    
#if HAS_ARDUINO_JSON
    // Парсинг JSON с ArduinoJson
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.print("WifiLink: JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    if (!doc.containsKey("command")) {
        return false;
    }
    
    const char* commandName = doc["command"];
    if (commandName == nullptr || strlen(commandName) == 0) {
        return false;
    }
    
    strncpy(outCmd.name, commandName, sizeof(outCmd.name) - 1);
    outCmd.name[sizeof(outCmd.name) - 1] = '\0';
    
    if (doc.containsKey("duration_ms")) {
        outCmd.durationMs = doc["duration_ms"];
    }
#else
    // Простой парсинг без ArduinoJson
    // Ищем "command":"VALUE"
    const char* cmdStart = strstr(jsonStr, "\"command\":\"");
    if (cmdStart == nullptr) {
        return false;
    }
    cmdStart += 11; // Пропускаем "command":"
    
    const char* cmdEnd = strchr(cmdStart, '"');
    if (cmdEnd == nullptr) {
        return false;
    }
    
    size_t cmdLen = cmdEnd - cmdStart;
    if (cmdLen >= sizeof(outCmd.name)) {
        cmdLen = sizeof(outCmd.name) - 1;
    }
    
    strncpy(outCmd.name, cmdStart, cmdLen);
    outCmd.name[cmdLen] = '\0';
    
    // Ищем duration_ms
    const char* durStart = strstr(jsonStr, "\"duration_ms\":");
    if (durStart != nullptr) {
        durStart += 14;
        outCmd.durationMs = atol(durStart);
    }
#endif

    return true;
}

void WifiLink::formatTimestamp(const DateTime& ts, char* buffer, size_t bufferSize) {
    if (buffer == nullptr || bufferSize < 20) {
        return;
    }
    
    // Формат: "dd:MM:yyyy hh:mm:ss"
    snprintf(buffer, bufferSize, "%02d:%02d:%04d %02d:%02d:%02d",
             ts.dd, ts.MM, ts.yyyy, ts.hh, ts.mm, ts.ss);
}

bool WifiLink::readLine(char* buffer, size_t bufferSize, uint32_t timeoutMs) {
    if (buffer == nullptr || bufferSize == 0) {
        return false;
    }
    
    uint32_t startTime = millis();
    size_t pos = 0;
    
    while ((millis() - startTime) < timeoutMs) {
        if (Serial1.available() > 0) {
            char c = Serial1.read();
            
            if (c == '\n') {
                buffer[pos] = '\0';
                return pos > 0;
            } else if (c == '\r') {
                continue; // Игнорируем \r
            } else if (pos < bufferSize - 1) {
                buffer[pos++] = c;
            }
        }
    }
    
    // Таймаут
    buffer[pos] = '\0';
    return false;
}

void WifiLink::sendImageBase64(const uint8_t* data, size_t size) {
    // Отправляем изображение чанками по 48 байт (= 64 символа base64)
    const size_t CHUNK_SIZE = 48;
    char base64Chunk[65]; // 64 + null terminator
    
    for (size_t i = 0; i < size; i += CHUNK_SIZE) {
        size_t chunkLen = (i + CHUNK_SIZE <= size) ? CHUNK_SIZE : (size - i);
        size_t encoded = base64_encode(data + i, chunkLen, base64Chunk, sizeof(base64Chunk));
        if (encoded > 0) {
            Serial1.print(base64Chunk);
        }
    }
}


