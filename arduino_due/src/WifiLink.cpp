#include "../include/WifiLink.h"
#include "../include/types.h"
#include "../include/base64.h"
#include <Arduino.h>
#include <cstring>

#if __has_include(<ArduinoJson.h>)
#include <ArduinoJson.h>
#define HAS_ARDUINO_JSON 1
#else
#define HAS_ARDUINO_JSON 0
#endif

void WifiLink::begin() {
    Serial1.begin(Hardware::SERIAL1_BAUD);
    lineBufferPos = 0;
    
    Serial.println("WifiLink: Serial1 initialized for NodeMCU communication");
    Serial.print("WifiLink: Baud rate = ");
    Serial.println(Hardware::SERIAL1_BAUD);
}

uint16_t WifiLink::crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

void WifiLink::sendData(uint32_t sessionId, uint32_t stepId,
                        const DateTime& ts,
                        const SensorSnapshot& sensors,
                        const ImageSnapshot& image) {
    
    bool imageTransferSuccess = false;
    
    if (image.available && image.buffer != nullptr && image.bufferSize > 0) {
        imageTransferSuccess = sendImageChunked(image.buffer, image.bufferSize, image.width, image.height);
        if (!imageTransferSuccess) {
            Serial.println("WifiLink: Image transfer failed");
        } else {
            delay(50);
        }
    }
    
    char timestampStr[32];
    formatTimestamp(ts, timestampStr, sizeof(timestampStr));
    
#if HAS_ARDUINO_JSON
    StaticJsonDocument<1024> doc;
    
    doc["session_id"] = sessionId;
    doc["step"] = stepId;
    doc["timestamp"] = timestampStr;
    
    JsonObject sensorsObj = doc.createNestedObject("sensors");
    sensorsObj["distance_cm"] = sensors.distanceCm;
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
    imageObj["available"] = imageTransferSuccess;
    imageObj["width"] = imageTransferSuccess ? image.width : 0;
    imageObj["height"] = imageTransferSuccess ? image.height : 0;
    imageObj["format"] = "GRAY8";
    
    Serial1.print("DATA ");
    serializeJson(doc, Serial1);
    Serial1.println();
    
#else
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
    Serial1.print(imageTransferSuccess ? "true" : "false");
    Serial1.print(",\"width\":");
    Serial1.print(imageTransferSuccess ? image.width : 0);
    Serial1.print(",\"height\":");
    Serial1.print(imageTransferSuccess ? image.height : 0);
    Serial1.print(",\"format\":\"GRAY8\"");
    Serial1.println("}}");
#endif
}

bool WifiLink::waitForCommand(Command& outCmd, uint32_t timeoutMs) {
    memset(outCmd.name, 0, sizeof(outCmd.name));
    outCmd.durationMs = 0;
        
    char buffer[256];
    if (!readLine(buffer, sizeof(buffer), timeoutMs)) {
        return false;
    }
    
    if (strncmp(buffer, "CMD ", 4) != 0) {
        return false;
    }
    
    const char* jsonStr = buffer + 4;
    
#if HAS_ARDUINO_JSON
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
    const char* cmdStart = strstr(jsonStr, "\"command\":\"");
    if (cmdStart == nullptr) {
        return false;
    }
    cmdStart += 11;
    
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
                continue;
            } else if (pos < bufferSize - 1) {
                buffer[pos++] = c;
            }
        }
    }
    
    buffer[pos] = '\0';
    return false;
}

bool WifiLink::sendImageChunked(const uint8_t* data, size_t size, uint16_t width, uint16_t height) {
    while (Serial1.available()) {
        Serial1.read();
    }
    
    uint16_t crc = crc16_ccitt(data, size);
    
    uint16_t totalChunks = (size + CHUNK_RAW_SIZE - 1) / CHUNK_RAW_SIZE;
    
    Serial.print("WifiLink: Sending image ");
    Serial.print(width);
    Serial.print("x");
    Serial.print(height);
    Serial.print(" in ");
    Serial.print(totalChunks);
    Serial.println(" chunks");
    
    Serial1.print("IMG_START ");
    Serial1.print(width);
    Serial1.print(" ");
    Serial1.print(height);
    Serial1.print(" ");
    Serial1.print(totalChunks);
    Serial1.print(" 0x");
    Serial1.println(crc, HEX);
    Serial1.flush();
    
    char readyBuf[32];
    if (!readLine(readyBuf, sizeof(readyBuf), ACK_TIMEOUT_MS)) {
        Serial.println("WifiLink: No IMG_READY received");
        return false;
    }
    if (strncmp(readyBuf, "IMG_READY", 9) != 0) {
        Serial.print("WifiLink: Expected IMG_READY, got: ");
        Serial.println(readyBuf);
        return false;
    }
    
    char base64Chunk[CHUNK_BASE64_SIZE + 1];
    
    for (uint16_t chunkIdx = 0; chunkIdx < totalChunks; chunkIdx++) {
        size_t offset = chunkIdx * CHUNK_RAW_SIZE;
        size_t chunkLen = (offset + CHUNK_RAW_SIZE <= size) ? CHUNK_RAW_SIZE : (size - offset);
        
        size_t encoded = base64_encode(data + offset, chunkLen, base64Chunk, sizeof(base64Chunk));
        if (encoded == 0) {
            Serial.println("WifiLink: Base64 encoding failed");
            return false;
        }
        
        if (!sendChunkWithAck(chunkIdx, base64Chunk)) {
            Serial.print("WifiLink: Failed to send chunk ");
            Serial.println(chunkIdx);
            Serial1.println("IMG_ABORT");
            return false;
        }
    }
    
    Serial1.println("IMG_END");
    Serial1.flush();
    
    Serial.println("WifiLink: Image transfer complete");
    return true;
}

bool WifiLink::sendChunkWithAck(uint16_t chunkIdx, const char* base64Data) {
    for (uint8_t retry = 0; retry < MAX_RETRIES; retry++) {
        while (Serial1.available()) {
            Serial1.read();
        }
        
        Serial1.print("IMG_CHUNK ");
        Serial1.print(chunkIdx);
        Serial1.print(" ");
        Serial1.println(base64Data);
        Serial1.flush();
        
        if (waitForAck(chunkIdx)) {
            return true;
        }
        
        Serial.print("WifiLink: Retry chunk ");
        Serial.print(chunkIdx);
        Serial.print(" attempt ");
        Serial.println(retry + 2);
        
        delay(50);
    }
    
    return false;
}

bool WifiLink::waitForAck(uint16_t expectedChunkIdx) {
    char buffer[32];
    uint32_t startTime = millis();
    size_t pos = 0;
    
    while ((millis() - startTime) < ACK_TIMEOUT_MS) {
        if (Serial1.available() > 0) {
            char c = Serial1.read();
            
            if (c == '\n') {
                buffer[pos] = '\0';
                
                if (strncmp(buffer, "ACK ", 4) == 0) {
                    uint16_t ackIdx = atoi(buffer + 4);
                    if (ackIdx == expectedChunkIdx) {
                        return true;
                    }
                } else if (strncmp(buffer, "NAK ", 4) == 0) {
                    return false;
                }
                
                pos = 0;
            } else if (c != '\r' && pos < sizeof(buffer) - 1) {
                buffer[pos++] = c;
            }
        }
    }
    
    return false;
}


