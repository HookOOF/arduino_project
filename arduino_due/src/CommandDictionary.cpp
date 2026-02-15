#include "../include/CommandDictionary.h"
#include "../include/types.h"
#include <cstring>

bool CommandDictionary::begin() {
    if (!loadFromFlash()) {
        Serial.println("CommandDictionary: Invalid flash data, initializing defaults");
        initDefaultCommands();
        saveToFlash();
    } else {
        Serial.println("CommandDictionary: Loaded from flash");
    }
    
    printAllToSerial();
    return true;
}

bool CommandDictionary::getConfig(const char* name, CommandConfig& outCfg) const {
    int index = findCommandIndex(name);
    if (index >= 0 && index < (int)storage.count) {
        outCfg = storage.commands[index];
        return true;
    }
    return false;
}

bool CommandDictionary::updateConfig(const CommandConfig& cfg) {
    int index = findCommandIndex(cfg.name);
    
    if (index >= 0) {
        storage.commands[index] = cfg;
    } else {
        if (storage.count >= DEFAULT_COMMAND_COUNT) {
            Serial.println("CommandDictionary: Cannot add, dictionary full");
            return false;
        }
        storage.commands[storage.count] = cfg;
        storage.count++;
    }
    
    saveToFlash();
    return true;
}

void CommandDictionary::printAllToSerial() const {
    Serial.println("=== Command Dictionary ===");
    for (size_t i = 0; i < storage.count; i++) {
        const CommandConfig& cmd = storage.commands[i];
        Serial.print("  ");
        Serial.print(cmd.name);
        Serial.print(": L=");
        Serial.print(cmd.leftSpeed);
        Serial.print(" R=");
        Serial.print(cmd.rightSpeed);
        Serial.print(" dur=");
        Serial.println(cmd.baseDurationMs);
    }
    Serial.println("==========================");
}

void CommandDictionary::initDefaultCommands() {
    storage.magic = MAGIC;
    storage.count = DEFAULT_COMMAND_COUNT;
    
    // FORWARD: оба мотора вперед
    strncpy(storage.commands[0].name, "FORWARD", 15);
    storage.commands[0].leftSpeed = 1;
    storage.commands[0].rightSpeed = 1;
    storage.commands[0].baseDurationMs = 3000;
    
    strncpy(storage.commands[1].name, "BACKWARD", 15);
    storage.commands[1].leftSpeed = -1;
    storage.commands[1].rightSpeed = -1;
    storage.commands[1].baseDurationMs = 3000;
    
    // LEFT: левый стоит, правый вперед (pivot turn) - для поворота ВЛЕВО правая сторона должна двигаться
    strncpy(storage.commands[2].name, "LEFT", 15);
    storage.commands[2].leftSpeed = 0;
    storage.commands[2].rightSpeed = 1;
    storage.commands[2].baseDurationMs = 3000;
    
    // RIGHT: левый вперед, правый стоит (pivot turn) - для поворота ВПРАВО левая сторона должна двигаться
    strncpy(storage.commands[3].name, "RIGHT", 15);
    storage.commands[3].leftSpeed = 1;
    storage.commands[3].rightSpeed = 0;
    storage.commands[3].baseDurationMs = 3000;
    
    // STOP: оба стоят
    strncpy(storage.commands[4].name, "STOP", 15);
    storage.commands[4].leftSpeed = 0;
    storage.commands[4].rightSpeed = 0;
    storage.commands[4].baseDurationMs = 3000;
}

bool CommandDictionary::loadFromFlash() {
    byte* flashData = (byte*)flashStorage.readAddress(0);
    
    if (flashData == nullptr) {
        return false;
    }
    
    memcpy(&storage, flashData, sizeof(FlashCommandStorage));
    
    if (storage.magic != MAGIC) {
        return false;
    }
    
    if (storage.count == 0 || storage.count > DEFAULT_COMMAND_COUNT) {
        return false;
    }
    
    return true;
}

void CommandDictionary::saveToFlash() {
    flashStorage.write(0, (byte*)&storage, sizeof(FlashCommandStorage));
}

int CommandDictionary::findCommandIndex(const char* name) const {
    for (size_t i = 0; i < storage.count; i++) {
        if (strcmp(storage.commands[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}


