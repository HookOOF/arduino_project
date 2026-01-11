#include "../include/SoftRTC.h"
#include "../include/types.h"
#include <Arduino.h>
#include <cstdlib>

void SoftRTC::begin() {
    // Установка времени по умолчанию
    baseTime.dd = 1;
    baseTime.MM = 1;
    baseTime.yyyy = 2026;
    baseTime.hh = 0;
    baseTime.mm = 0;
    baseTime.ss = 0;
    baseMillis = millis();
    
    Serial.println("SoftRTC: Initialized with default time");
}

void SoftRTC::setFromString(const char* dateTimeStr) {
    // Формат: "dd:MM:yyyy hh:mm:ss"
    if (dateTimeStr == nullptr || strlen(dateTimeStr) < 19) {
        Serial.println("SoftRTC: Invalid time string");
        return;
    }
    
    // Парсинг строки
    char buffer[20];
    strncpy(buffer, dateTimeStr, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // dd:MM:yyyy hh:mm:ss
    baseTime.dd = atoi(buffer);
    baseTime.MM = atoi(buffer + 3);
    baseTime.yyyy = atoi(buffer + 6);
    baseTime.hh = atoi(buffer + 11);
    baseTime.mm = atoi(buffer + 14);
    baseTime.ss = atoi(buffer + 17);
    
    baseMillis = millis();
    
    Serial.print("SoftRTC: Time set to ");
    Serial.print(baseTime.dd);
    Serial.print(":");
    Serial.print(baseTime.MM);
    Serial.print(":");
    Serial.print(baseTime.yyyy);
    Serial.print(" ");
    Serial.print(baseTime.hh);
    Serial.print(":");
    Serial.print(baseTime.mm);
    Serial.print(":");
    Serial.println(baseTime.ss);
}

DateTime SoftRTC::now() {
    update();
    return baseTime;
}

void SoftRTC::update() {
    uint32_t currentMillis = millis();
    uint32_t elapsedMs = currentMillis - baseMillis;
    
    // Обновляем только если прошла хотя бы секунда
    if (elapsedMs >= 1000) {
        uint32_t elapsedSeconds = elapsedMs / 1000;
        baseMillis += elapsedSeconds * 1000;
        addSeconds(elapsedSeconds);
    }
}

void SoftRTC::addSeconds(uint32_t seconds) {
    baseTime.ss += seconds;
    
    while (baseTime.ss >= 60) {
        baseTime.ss -= 60;
        baseTime.mm++;
    }
    
    while (baseTime.mm >= 60) {
        baseTime.mm -= 60;
        baseTime.hh++;
    }
    
    while (baseTime.hh >= 24) {
        baseTime.hh -= 24;
        baseTime.dd++;
    }
    
    // Простая проверка дней в месяце
    uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Високосный год
    if ((baseTime.yyyy % 4 == 0 && baseTime.yyyy % 100 != 0) || (baseTime.yyyy % 400 == 0)) {
        daysInMonth[1] = 29;
    }
    
    while (baseTime.dd > daysInMonth[baseTime.MM - 1]) {
        baseTime.dd -= daysInMonth[baseTime.MM - 1];
        baseTime.MM++;
        
        if (baseTime.MM > 12) {
            baseTime.MM = 1;
            baseTime.yyyy++;
        }
    }
}


