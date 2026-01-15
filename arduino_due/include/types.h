#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// Аппаратные константы
namespace Hardware {
    // HC-SR04 (NewPing библиотека)
    // Подключение: TRIG=D8, ECHO=D9
    const uint8_t TRIG_PIN = 8;
    const uint8_t ECHO_PIN = 9;
    const uint16_t MAX_DISTANCE_CM = 400;  // Максимальное расстояние для NewPing
    
    // Фоторезистор
    const uint8_t LIGHT_PIN = A0;
    const int LIGHT_THRESHOLD = 500; // lightRaw < LIGHT_THRESHOLD → темно
    
    // MPU6050 (I2C Wire1 - SDA1, SCL1)
    // Arduino DUE: SDA1 = 70, SCL1 = 71
    const uint8_t I2C_SDA1 = 70;
    const uint8_t I2C_SCL1 = 71;
    
    // Моторы - простой драйвер без PWM
    // Левый мотор: IN1, IN2
    // Правый мотор: IN3, IN4
    const uint8_t MOTOR_IN1 = 6;   // Левый мотор - вперед
    const uint8_t MOTOR_IN2 = 7;   // Левый мотор - назад
    const uint8_t MOTOR_IN3 = 4;   // Правый мотор - вперед
    const uint8_t MOTOR_IN4 = 5;   // Правый мотор - назад
    
    // NodeMCU ESP8266 (Serial1 - TX1, RX1)
    // Arduino DUE: TX1 = 18, RX1 = 19
    const uint32_t SERIAL1_BAUD = 9600;
    
    // OV7670 + AL422B FIFO Camera pins
    const uint8_t CAM_VSYNC = 40;
    const uint8_t CAM_RST = 22;
    const uint8_t CAM_WR = 38;
    const uint8_t CAM_WRST = 37;
    const uint8_t CAM_RRST = 35;
    const uint8_t CAM_OE = 39;
    const uint8_t CAM_RCK = 36;
    // Data pins D0-D7
    const uint8_t CAM_D0 = 51;
    const uint8_t CAM_D1 = 50;
    const uint8_t CAM_D2 = 49;
    const uint8_t CAM_D3 = 48;
    const uint8_t CAM_D4 = 47;
    const uint8_t CAM_D5 = 46;
    const uint8_t CAM_D6 = 45;
    const uint8_t CAM_D7 = 44;
    
    // Разрешение камеры
    const uint16_t CAM_WIDTH = 80;   // QQVGA/2
    const uint16_t CAM_HEIGHT = 60;
}

// Структура для хранения даты и времени
struct DateTime {
    uint8_t dd;    // день (1-31)
    uint8_t MM;    // месяц (1-12)
    uint16_t yyyy; // год
    uint8_t hh;    // час (0-23)
    uint8_t mm;    // минута (0-59)
    uint8_t ss;    // секунда (0-59)
};

// Снимок данных с датчиков
struct SensorSnapshot {
    float distanceCm;  // расстояние от HC-SR04 (см)
    int lightRaw;      // сырое значение фоторезистора
    bool isDark;       // темно (lightRaw < LIGHT_THRESHOLD)
    float ax, ay, az;  // ускорение от MPU6050
    float gx, gy, gz;  // угловая скорость от MPU6050
};

// Снимок изображения с камеры
struct ImageSnapshot {
    bool available;           // доступно ли изображение
    uint16_t width;           // ширина
    uint16_t height;          // высота
    uint8_t* buffer;          // указатель на буфер данных (grayscale)
    size_t bufferSize;        // размер буфера
};

// Конфигурация команды движения
struct CommandConfig {
    char name[16];            // "FORWARD", "BACKWARD" и т.д.
    int16_t leftSpeed;        // -1, 0, 1 (направление без PWM)
    int16_t rightSpeed;       // -1, 0, 1 (направление без PWM)
    uint32_t baseDurationMs;  // базовая длительность в мс
};

// Команда от сервера
struct Command {
    char name[16];            // имя команды от сервера
    uint32_t durationMs;      // 0 = использовать baseDurationMs из словаря
};

// Запись в лог
struct LogEntry {
    DateTime ts;              // временная метка
    char commandName[16];     // имя выполненной команды
    uint32_t durationMs;      // длительность выполнения
    float distanceCm;         // расстояние
    int lightRaw;             // освещенность
    bool isDark;              // темно ли
    bool imageSent;           // отправлено ли изображение
};

#endif // TYPES_H


