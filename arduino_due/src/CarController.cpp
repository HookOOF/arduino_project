#include "../include/CarController.h"
#include <cstring>

void CarController::begin() {
    // Инициализация Serial для отладки
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        ; // Ожидание подключения Serial (максимум 3 секунды)
    }
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("   LLM Car Controller - Arduino Due");
    Serial.println("========================================");
    Serial.println();
    
    // Инициализация всех модулей
    Serial.println("Initializing modules...");
    
    rtc.begin();
    sensors.begin();
    motorController.begin();
    
    // Камера - опционально
    if (!cameraModule.begin()) {
        Serial.println("WARNING: Camera not available, continuing without it");
    }
    
    wifiLink.begin();
    commandDict.begin();
    logger.begin();
    
    // Настройка процессора команд
    serialProcessor.begin(&commandDict, &logger, &rtc);
    serialProcessor.serialLoggingEnabled = &serialLoggingEnabled;
    serialProcessor.defaultStepDurationMs = &defaultStepDurationMs;
    
    // Настройки по умолчанию
    serialLoggingEnabled = true;
    defaultStepDurationMs = 3000;
    
    // Инициализация состояния
    sessionId = 1;
    stepId = 0;
    currentState = STATE_INIT;
    stateStartMillis = millis();
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("   Initialization Complete!");
    Serial.println("   Waiting for NodeMCU connection...");
    Serial.println("========================================");
    Serial.println();
}

void CarController::tick() {
    // Всегда обрабатываем команды из Serial Monitor
    serialProcessor.process();
    
    // Обновляем время
    rtc.update();
    
    // Конечный автомат
    switch (currentState) {
        case STATE_INIT:
            handleStateInit();
            break;
        case STATE_COLLECT_SENSORS:
            handleStateCollectSensors();
            break;
        case STATE_SEND_TO_SERVER:
            handleStateSendToServer();
            break;
        case STATE_WAIT_COMMAND:
            handleStateWaitCommand();
            break;
        case STATE_EXECUTE_COMMAND:
            handleStateExecuteCommand();
            break;
    }
}

void CarController::handleStateInit() {
    // Небольшая задержка перед стартом
    if (millis() - stateStartMillis > 2000) {
        Serial.println("Starting main loop...");
        changeState(STATE_COLLECT_SENSORS);
    }
}

void CarController::handleStateCollectSensors() {
    // Увеличиваем счетчик шага
    stepId++;
    
    // Получаем текущее время
    currentStepTimestamp = rtc.now();
    
    // Читаем данные с датчиков
    currentSensorSnapshot = sensors.readSnapshot();
    
    // Захватываем изображение если достаточно света
    if (cameraModule.isInitialized() && !currentSensorSnapshot.isDark) {
        currentImageSnapshot = cameraModule.capture();
    } else {
        currentImageSnapshot.available = false;
        currentImageSnapshot.width = 0;
        currentImageSnapshot.height = 0;
        currentImageSnapshot.buffer = nullptr;
        currentImageSnapshot.bufferSize = 0;
    }
    
    if (serialLoggingEnabled) {
        Serial.print("Step ");
        Serial.print(stepId);
        Serial.print(": dist=");
        Serial.print(currentSensorSnapshot.distanceCm, 1);
        Serial.print("cm obst=");
        Serial.print(currentSensorSnapshot.obstacle ? "Y" : "N");
        Serial.print(" dark=");
        Serial.print(currentSensorSnapshot.isDark ? "Y" : "N");
        Serial.print(" cam=");
        Serial.println(currentImageSnapshot.available ? "Y" : "N");
    }
    
    changeState(STATE_SEND_TO_SERVER);
}

void CarController::handleStateSendToServer() {
    // Отправляем данные на сервер через NodeMCU
    wifiLink.sendData(sessionId, stepId, currentStepTimestamp,
                      currentSensorSnapshot, currentImageSnapshot);
    
    commandWaitStartMillis = millis();
    changeState(STATE_WAIT_COMMAND);
}

void CarController::handleStateWaitCommand() {
    // Пытаемся получить команду
    Command cmd;
    if (wifiLink.waitForCommand(cmd, 100)) {
        // Команда получена
        currentCommand = cmd;
        
        // Ищем конфигурацию в словаре
        if (!commandDict.getConfig(cmd.name, currentCommandConfig)) {
            Serial.print("Unknown command: ");
            Serial.print(cmd.name);
            Serial.println(", using STOP");
            commandDict.getConfig("STOP", currentCommandConfig);
            strncpy(currentCommand.name, "STOP", sizeof(currentCommand.name) - 1);
        }
        
        // Определяем длительность
        if (cmd.durationMs == 0) {
            currentCommandDuration = currentCommandConfig.baseDurationMs;
        } else {
            currentCommandDuration = cmd.durationMs;
        }
        
        if (serialLoggingEnabled) {
            Serial.print("Received command: ");
            Serial.print(currentCommand.name);
            Serial.print(" for ");
            Serial.print(currentCommandDuration);
            Serial.println(" ms");
        }
        
        changeState(STATE_EXECUTE_COMMAND);
        return;
    }
    
    // Проверка таймаута
    if (millis() - commandWaitStartMillis >= COMMAND_WAIT_TIMEOUT_MS) {
        Serial.println("Command timeout, using STOP");
        
        strncpy(currentCommand.name, "STOP", sizeof(currentCommand.name) - 1);
        currentCommand.durationMs = defaultStepDurationMs;
        commandDict.getConfig("STOP", currentCommandConfig);
        currentCommandDuration = defaultStepDurationMs;
        
        changeState(STATE_EXECUTE_COMMAND);
    }
}

void CarController::handleStateExecuteCommand() {
    // Первый вход в состояние - запускаем команду
    if (millis() - stateStartMillis < 10) {
        motorController.applyCommand(currentCommandConfig);
        commandExecStartMillis = millis();
    }
    
    // Проверяем окончание команды
    if (millis() - commandExecStartMillis >= currentCommandDuration) {
        // Останавливаем моторы
        motorController.stop();
        
        // Логируем команду
        logCurrentStep();
        
        // Переходим к следующему шагу
        changeState(STATE_COLLECT_SENSORS);
    }
}

void CarController::changeState(State newState) {
    currentState = newState;
    stateStartMillis = millis();
}

void CarController::logCurrentStep() {
    LogEntry entry;
    entry.ts = currentStepTimestamp;
    strncpy(entry.commandName, currentCommand.name, sizeof(entry.commandName) - 1);
    entry.commandName[sizeof(entry.commandName) - 1] = '\0';
    entry.durationMs = currentCommandDuration;
    entry.distanceCm = currentSensorSnapshot.distanceCm;
    entry.lightRaw = currentSensorSnapshot.lightRaw;
    entry.isDark = currentSensorSnapshot.isDark;
    entry.obstacle = currentSensorSnapshot.obstacle;
    entry.imageSent = currentImageSnapshot.available;
    
    logger.add(entry);
    
    if (serialLoggingEnabled) {
        Serial.print("Executed: ");
        Serial.print(entry.commandName);
        Serial.print(" (");
        Serial.print(entry.durationMs);
        Serial.println(" ms)");
    }
}

