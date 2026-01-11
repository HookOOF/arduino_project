#include "../include/MotorController.h"
#include "../include/types.h"

void MotorController::begin() {
    // Настройка пинов моторов как выходы
    pinMode(Hardware::MOTOR_IN1, OUTPUT);
    pinMode(Hardware::MOTOR_IN2, OUTPUT);
    pinMode(Hardware::MOTOR_IN3, OUTPUT);
    pinMode(Hardware::MOTOR_IN4, OUTPUT);
    
    // Остановка обоих моторов при инициализации
    stop();
    
    Serial.println("MotorController: Initialized (no PWM mode)");
}

void MotorController::stop() {
    // Все пины в LOW = остановка
    digitalWrite(Hardware::MOTOR_IN1, LOW);
    digitalWrite(Hardware::MOTOR_IN2, LOW);
    digitalWrite(Hardware::MOTOR_IN3, LOW);
    digitalWrite(Hardware::MOTOR_IN4, LOW);
}

void MotorController::applyCommand(const CommandConfig& cfg) {
    // Применяем направление для каждого мотора
    // leftSpeed и rightSpeed: положительное = вперед, отрицательное = назад, 0 = стоп
    
    if (cfg.leftSpeed > 0) {
        setLeftMotor(1);   // Вперед
    } else if (cfg.leftSpeed < 0) {
        setLeftMotor(-1);  // Назад
    } else {
        setLeftMotor(0);   // Стоп
    }
    
    if (cfg.rightSpeed > 0) {
        setRightMotor(1);  // Вперед
    } else if (cfg.rightSpeed < 0) {
        setRightMotor(-1); // Назад
    } else {
        setRightMotor(0);  // Стоп
    }
}

void MotorController::forward() {
    setLeftMotor(1);
    setRightMotor(1);
}

void MotorController::backward() {
    setLeftMotor(-1);
    setRightMotor(-1);
}

void MotorController::turnLeft() {
    setLeftMotor(-1);  // Левый назад
    setRightMotor(1);  // Правый вперед
}

void MotorController::turnRight() {
    setLeftMotor(1);   // Левый вперед
    setRightMotor(-1); // Правый назад
}

void MotorController::setLeftMotor(int8_t direction) {
    if (direction > 0) {
        // Вперед: IN1=HIGH, IN2=LOW
        digitalWrite(Hardware::MOTOR_IN1, HIGH);
        digitalWrite(Hardware::MOTOR_IN2, LOW);
    } else if (direction < 0) {
        // Назад: IN1=LOW, IN2=HIGH
        digitalWrite(Hardware::MOTOR_IN1, LOW);
        digitalWrite(Hardware::MOTOR_IN2, HIGH);
    } else {
        // Стоп: оба LOW
        digitalWrite(Hardware::MOTOR_IN1, LOW);
        digitalWrite(Hardware::MOTOR_IN2, LOW);
    }
}

void MotorController::setRightMotor(int8_t direction) {
    if (direction > 0) {
        // Вперед: IN3=HIGH, IN4=LOW
        digitalWrite(Hardware::MOTOR_IN3, HIGH);
        digitalWrite(Hardware::MOTOR_IN4, LOW);
    } else if (direction < 0) {
        // Назад: IN3=LOW, IN4=HIGH
        digitalWrite(Hardware::MOTOR_IN3, LOW);
        digitalWrite(Hardware::MOTOR_IN4, HIGH);
    } else {
        // Стоп: оба LOW
        digitalWrite(Hardware::MOTOR_IN3, LOW);
        digitalWrite(Hardware::MOTOR_IN4, LOW);
    }
}


