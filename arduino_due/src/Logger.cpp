#include "../include/Logger.h"
#include "../include/types.h"
#include <cstring>

void Logger::begin() {
    clear();
    Serial.println("Logger: Initialized (256 entries ring buffer)");
}

void Logger::add(const LogEntry& e) {
    logEntries[currentIndex] = e;
    currentIndex = (currentIndex + 1) % MAX_LOG_ENTRIES;
    
    if (logCount < MAX_LOG_ENTRIES) {
        logCount++;
    }
}

void Logger::printAllToSerial() const {
    Serial.println("=== Command Log ===");
    Serial.print("Total entries: ");
    Serial.println(logCount);
    
    size_t startIndex = 0;
    size_t entriesToPrint = logCount;
    
    if (logCount >= MAX_LOG_ENTRIES) {
        startIndex = currentIndex;
        entriesToPrint = MAX_LOG_ENTRIES;
    }
    
    for (size_t i = 0; i < entriesToPrint; i++) {
        size_t idx = (startIndex + i) % MAX_LOG_ENTRIES;
        const LogEntry& entry = logEntries[idx];
        
        char timestampStr[32];
        formatTimestamp(entry.ts, timestampStr, sizeof(timestampStr));
        
        Serial.print(timestampStr);
        Serial.print(" ");
        Serial.print(entry.commandName);
        Serial.print(" dist=");
        Serial.print(entry.distanceCm, 1);
        Serial.print(" light=");
        Serial.print(entry.lightRaw);
        Serial.print(" dark=");
        Serial.print(entry.isDark ? 1 : 0);
        Serial.print(" obst=");
        Serial.print(entry.obstacle ? 1 : 0);
        Serial.print(" img=");
        Serial.print(entry.imageSent ? 1 : 0);
        Serial.print(" dur=");
        Serial.println(entry.durationMs);
    }
    
    Serial.println("===================");
}

void Logger::clear() {
    logCount = 0;
    currentIndex = 0;
    memset(logEntries, 0, sizeof(logEntries));
}

bool Logger::getEntry(size_t index, LogEntry& outEntry) const {
    if (index >= logCount) {
        return false;
    }
    
    size_t startIndex = 0;
    if (logCount >= MAX_LOG_ENTRIES) {
        startIndex = currentIndex;
    }
    
    size_t realIndex = (startIndex + index) % MAX_LOG_ENTRIES;
    outEntry = logEntries[realIndex];
    return true;
}

void Logger::formatTimestamp(const DateTime& ts, char* buffer, size_t bufferSize) const {
    if (buffer == nullptr || bufferSize < 20) {
        return;
    }
    snprintf(buffer, bufferSize, "%02d:%02d:%04d %02d:%02d:%02d",
             ts.dd, ts.MM, ts.yyyy, ts.hh, ts.mm, ts.ss);
}


