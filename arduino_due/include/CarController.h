#ifndef CAR_CONTROLLER_H
#define CAR_CONTROLLER_H

#include "types.h"
#include "Sensors.h"
#include "MotorController.h"
#include "CameraModule.h"
#include "WifiLink.h"
#include "CommandDictionary.h"
#include "Logger.h"
#include "SerialCommandProcessor.h"
#include "SoftRTC.h"

/**
 * Главный контроллер автомобиля
 * Управляет всеми модулями и реализует конечный автомат
 */
class CarController {
public:
    /**
     * Инициализация всех модулей
     */
    void begin();
    
    /**
     * Основной цикл обработки (вызывать из loop())
     */
    void tick();

private:
    // Модули
    Sensors sensors;
    MotorController motorController;
    CameraModule cameraModule;
    WifiLink wifiLink;
    CommandDictionary commandDict;
    Logger logger;
    SerialCommandProcessor serialProcessor;
    SoftRTC rtc;
    
    // Состояния конечного автомата
    enum State {
        STATE_INIT,
        STATE_COLLECT_SENSORS,
        STATE_SEND_TO_SERVER,
        STATE_WAIT_COMMAND,
        STATE_EXECUTE_COMMAND
    };
    
    State currentState;
    
    // Счетчики
    uint32_t sessionId;
    uint32_t stepId;
    
    // Таймеры
    uint32_t stateStartMillis;
    uint32_t commandWaitStartMillis;
    uint32_t commandExecStartMillis;
    
    // Текущие данные шага
    SensorSnapshot currentSensorSnapshot;
    ImageSnapshot currentImageSnapshot;
    DateTime currentStepTimestamp;
    Command currentCommand;
    CommandConfig currentCommandConfig;
    uint32_t currentCommandDuration;
    
    // Конфигурация
    bool serialLoggingEnabled;
    uint32_t defaultStepDurationMs;
    static const uint32_t COMMAND_WAIT_TIMEOUT_MS = 5000;
    
    // Обработчики состояний
    void handleStateInit();
    void handleStateCollectSensors();
    void handleStateSendToServer();
    void handleStateWaitCommand();
    void handleStateExecuteCommand();
    
    void changeState(State newState);
    void logCurrentStep();
};

#endif // CAR_CONTROLLER_H


