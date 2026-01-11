#ifndef COMMAND_DICTIONARY_H
#define COMMAND_DICTIONARY_H

#include "types.h"
#include <DueFlashStorage.h>

// Структура для хранения в энергонезависимой памяти
struct FlashCommandStorage {
    uint32_t magic;              // сигнатура для проверки валидности
    size_t count;                // количество команд
    CommandConfig commands[5];   // массив команд
};

/**
 * Справочник команд в энергонезависимой памяти
 * Хранит конфигурации команд: FORWARD, BACKWARD, LEFT, RIGHT, STOP
 */
class CommandDictionary {
public:
    /**
     * Инициализация словаря команд
     * @return true если успешно
     */
    bool begin();
    
    /**
     * Получение конфигурации команды по имени
     * @param name имя команды
     * @param outCfg структура для записи конфигурации
     * @return true если команда найдена
     */
    bool getConfig(const char* name, CommandConfig& outCfg) const;
    
    /**
     * Обновление или добавление конфигурации команды
     * @param cfg конфигурация команды
     * @return true если успешно
     */
    bool updateConfig(const CommandConfig& cfg);
    
    /**
     * Вывод всех команд в Serial (для отладки)
     */
    void printAllToSerial() const;

private:
    static const uint32_t MAGIC = 0xCAFECAFE;
    static const size_t DEFAULT_COMMAND_COUNT = 5;
    
    DueFlashStorage flashStorage;
    FlashCommandStorage storage;
    
    void initDefaultCommands();
    bool loadFromFlash();
    void saveToFlash();
    int findCommandIndex(const char* name) const;
};

#endif // COMMAND_DICTIONARY_H

