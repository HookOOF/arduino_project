#ifndef SOFT_RTC_H
#define SOFT_RTC_H

#include "types.h"

/**
 * Программные часы реального времени
 * Используют millis() для отсчета времени
 */
class SoftRTC {
public:
    /**
     * Инициализация часов
     */
    void begin();
    
    /**
     * Установка времени из строки формата "dd:MM:yyyy hh:mm:ss"
     * @param dateTimeStr строка с датой и временем
     */
    void setFromString(const char* dateTimeStr);
    
    /**
     * Получение текущего времени
     * @return структура DateTime с текущим временем
     */
    DateTime now();
    
    /**
     * Обновление времени (вызывать периодически)
     */
    void update();

private:
    DateTime baseTime;
    uint32_t baseMillis;
    
    void addSeconds(uint32_t seconds);
};

#endif // SOFT_RTC_H

