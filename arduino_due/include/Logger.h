#ifndef LOGGER_H
#define LOGGER_H

#include "types.h"

/**
 * Журнал команд
 * Кольцевой буфер на 256 записей
 */
class Logger {
public:
    /**
     * Инициализация логгера
     */
    void begin();
    
    /**
     * Добавление записи в лог
     * @param e запись для добавления
     */
    void add(const LogEntry& e);
    
    /**
     * Вывод всех записей в Serial
     */
    void printAllToSerial() const;
    
    /**
     * Получение количества записей
     */
    size_t getCount() const { return logCount; }
    
    /**
     * Очистка лога
     */
    void clear();
    
    /**
     * Получение записи по индексу (0 = самая старая)
     */
    bool getEntry(size_t index, LogEntry& outEntry) const;

private:
    static const size_t MAX_LOG_ENTRIES = 256;
    LogEntry logEntries[MAX_LOG_ENTRIES];
    size_t logCount;
    size_t currentIndex;
    
    void formatTimestamp(const DateTime& ts, char* buffer, size_t bufferSize) const;
};

#endif // LOGGER_H


