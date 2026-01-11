#include "../include/SerialCommandProcessor.h"
#include "../include/CommandDictionary.h"
#include "../include/Logger.h"
#include "../include/SoftRTC.h"
#include <cstring>

void SerialCommandProcessor::begin(CommandDictionary* dict, Logger* log, SoftRTC* clock) {
    commandDict = dict;
    logger = log;
    rtc = clock;
    lineBufferPos = 0;
    
    Serial.println("SerialCommandProcessor: Ready");
    Serial.println("  Type 'help' for available commands");
}

void SerialCommandProcessor::process() {
    char buffer[LINE_BUFFER_SIZE];
    if (readLine(buffer, sizeof(buffer))) {
        parseCommand(buffer);
    }
}

bool SerialCommandProcessor::readLine(char* buffer, size_t bufferSize) {
    while (Serial.available() > 0) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (lineBufferPos > 0) {
                lineBuffer[lineBufferPos] = '\0';
                strncpy(buffer, lineBuffer, bufferSize - 1);
                buffer[bufferSize - 1] = '\0';
                lineBufferPos = 0;
                return true;
            }
        } else if (lineBufferPos < LINE_BUFFER_SIZE - 1) {
            lineBuffer[lineBufferPos++] = c;
        }
    }
    return false;
}

void SerialCommandProcessor::parseCommand(const char* line) {
    if (strcmp(line, "help") == 0) {
        printHelp();
    }
    else if (strcmp(line, "status") == 0) {
        Serial.println("=== System Status ===");
        DateTime now = rtc->now();
        Serial.print("Time: ");
        Serial.print(now.dd);
        Serial.print(":");
        Serial.print(now.MM);
        Serial.print(":");
        Serial.print(now.yyyy);
        Serial.print(" ");
        Serial.print(now.hh);
        Serial.print(":");
        Serial.print(now.mm);
        Serial.print(":");
        Serial.println(now.ss);
        Serial.print("Serial logging: ");
        Serial.println(*serialLoggingEnabled ? "ON" : "OFF");
        Serial.print("Step duration: ");
        Serial.print(*defaultStepDurationMs);
        Serial.println(" ms");
    }
    else if (strcmp(line, "log") == 0) {
        logger->printAllToSerial();
    }
    else if (strcmp(line, "log clear") == 0) {
        logger->clear();
        Serial.println("Log cleared");
    }
    else if (strcmp(line, "dict") == 0) {
        commandDict->printAllToSerial();
    }
    else if (strcmp(line, "serial on") == 0) {
        *serialLoggingEnabled = true;
        Serial.println("Serial logging enabled");
    }
    else if (strcmp(line, "serial off") == 0) {
        *serialLoggingEnabled = false;
        Serial.println("Serial logging disabled");
    }
    else if (strncmp(line, "time ", 5) == 0) {
        // Формат: time dd:MM:yyyy hh:mm:ss
        rtc->setFromString(line + 5);
    }
    else if (strncmp(line, "duration ", 9) == 0) {
        *defaultStepDurationMs = atol(line + 9);
        Serial.print("Step duration set to ");
        Serial.print(*defaultStepDurationMs);
        Serial.println(" ms");
    }
    else if (strlen(line) > 0) {
        Serial.print("Unknown command: ");
        Serial.println(line);
        Serial.println("Type 'help' for available commands");
    }
}

void SerialCommandProcessor::printHelp() {
    Serial.println("=== Available Commands ===");
    Serial.println("  help              - Show this help");
    Serial.println("  status            - Show system status");
    Serial.println("  log               - Print command log");
    Serial.println("  log clear         - Clear command log");
    Serial.println("  dict              - Print command dictionary");
    Serial.println("  serial on         - Enable serial logging");
    Serial.println("  serial off        - Disable serial logging");
    Serial.println("  time dd:MM:yyyy hh:mm:ss - Set time");
    Serial.println("  duration <ms>     - Set step duration");
    Serial.println("==========================");
}


