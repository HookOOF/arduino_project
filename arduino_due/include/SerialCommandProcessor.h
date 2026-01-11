#ifndef SERIAL_COMMAND_PROCESSOR_H
#define SERIAL_COMMAND_PROCESSOR_H

#include "types.h"

class CommandDictionary;
class Logger;
class SoftRTC;

/**
 * Процессор команд из Serial Monitor
 * Обрабатывает команды для отладки и настройки
 */
class SerialCommandProcessor {
public:
    /**
     * Инициализация процессора
     */
    void begin(CommandDictionary* dict, Logger* logger, SoftRTC* rtc);
    
    /**
     * Обработка команд из Serial (неблокирующее)
     */
    void process();
    
    // Указатели на конфигурационные переменные
    bool* serialLoggingEnabled;
    uint32_t* defaultStepDurationMs;

private:
    CommandDictionary* commandDict;
    Logger* logger;
    SoftRTC* rtc;
    
    static const size_t LINE_BUFFER_SIZE = 256;
    char lineBuffer[LINE_BUFFER_SIZE];
    size_t lineBufferPos;
    
    bool readLine(char* buffer, size_t bufferSize);
    void parseCommand(const char* line);
    void printHelp();
};

#endif // SERIAL_COMMAND_PROCESSOR_H

