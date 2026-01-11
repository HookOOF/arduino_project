#ifndef SENSORS_H
#define SENSORS_H

#include "types.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// NewPing library для HC-SR04
#if __has_include(<NewPing.h>)
#include <NewPing.h>
#define HAS_NEWPING 1
#else
#define HAS_NEWPING 0
// Stub класс если библиотека не установлена
class NewPing {
public:
    NewPing(uint8_t trigger, uint8_t echo, int max_cm) {}
    unsigned int ping_cm() { return 0; }
};
#endif

/**
 * Модуль датчиков
 * Работает с HC-SR04, фоторезистором, ИК-датчиком препятствий и MPU6050
 */
class Sensors {
public:
    /**
     * Инициализация всех датчиков
     * @return true если успешно, false в противном случае
     */
    bool begin();
    
    /**
     * Чтение снимка данных со всех датчиков
     * @return структура SensorSnapshot с текущими значениями
     */
    SensorSnapshot readSnapshot();
    
    /**
     * Проверка, темно ли сейчас
     * @return true если темно
     */
    bool isDark();

private:
    Adafruit_MPU6050 mpu;
    bool mpuInitialized;
    NewPing* sonar;
    
    /**
     * Чтение расстояния от HC-SR04
     * @return расстояние в см
     */
    float readHCSR04();
    
    /**
     * Чтение состояния ИК-датчика препятствий
     * @return true если препятствие
     */
    bool readObstacleSensor();
    
    /**
     * Чтение фоторезистора
     * @param lightRaw выходной параметр - сырое значение
     * @return true если темно
     */
    bool readLightSensor(int& lightRaw);
    
    /**
     * Чтение данных с MPU6050
     */
    bool readMPU6050(float& ax, float& ay, float& az, float& gx, float& gy, float& gz);
};

#endif // SENSORS_H


