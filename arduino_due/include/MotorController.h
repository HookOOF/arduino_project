#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "types.h"

/**
 * Контроллер моторов без PWM
 * Использует простой драйвер с пинами IN1-IN4
 * Скорость регулируется только временем работы
 */
class MotorController {
public:
    /**
     * Инициализация драйвера моторов
     */
    void begin();
    
    /**
     * Остановка обоих моторов
     */
    void stop();
    
    /**
     * Применение команды движения
     * @param cfg конфигурация команды с параметрами направления
     */
    void applyCommand(const CommandConfig& cfg);
    
    /**
     * Движение вперед
     */
    void forward();
    
    /**
     * Движение назад
     */
    void backward();
    
    /**
     * Поворот влево (левый стоит, правый вперед)
     */
    void turnLeft();
    
    /**
     * Поворот вправо (левый вперед, правый стоит)
     */
    void turnRight();

private:
    /**
     * Установка состояния левого мотора
     * @param direction 1=вперед, -1=назад, 0=стоп
     */
    void setLeftMotor(int8_t direction);
    
    /**
     * Установка состояния правого мотора
     * @param direction 1=вперед, -1=назад, 0=стоп
     */
    void setRightMotor(int8_t direction);
};

#endif // MOTOR_CONTROLLER_H


