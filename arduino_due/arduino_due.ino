/*
 * LLM Car Controller - Arduino Due
 * 
 * Система управления моделью автомобиля на базе Arduino Due.
 * Собирает данные с датчиков, отправляет на сервер с LLM через NodeMCU ESP8266,
 * получает команды управления и выполняет их.
 * 
 * Оборудование:
 * - Arduino Due
 * - NodeMCU ESP8266 (WiFi мост, подключен к Serial1)
 * - HC-SR04 ультразвуковой датчик (TRIG=10, ECHO=11)
 * - MPU6050 акселерометр/гироскоп (I2C Wire1: SDA1=70, SCL1=71)
 * - OV7670 + AL422B FIFO камера (опционально)
 * - Фоторезистор (A0)
 * - ИК-датчик препятствий (D24)
 * - Моторы через драйвер (IN1=6, IN2=7, IN3=4, IN4=5)
 * 
 * Подключение NodeMCU:
 * - Arduino Due TX1 (pin 18) → NodeMCU RX (GPIO3)
 * - Arduino Due RX1 (pin 19) → NodeMCU TX (GPIO1)
 * - GND → GND
 * 
 * Необходимые библиотеки:
 * - DueFlashStorage
 * - Adafruit_MPU6050
 * - Adafruit_Sensor
 * - NewPing (опционально)
 * - ArduinoJson (опционально)
 * 
 * Автор: LLM Car Project
 * Дата: 2026
 */

#include <Arduino.h>
#include "include/CarController.h"

// Глобальный объект контроллера автомобиля
CarController carController;

void setup() {
    // Инициализация контроллера
    // (Serial, датчики, моторы, WiFi и т.д. инициализируются внутри)
    carController.begin();
}

void loop() {
    // Основной цикл обработки
    // (неблокирующий, управляется конечным автоматом)
    carController.tick();
}

