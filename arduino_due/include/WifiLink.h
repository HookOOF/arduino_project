#ifndef WIFI_LINK_H
#define WIFI_LINK_H

#include "types.h"

/**
 * Связь с NodeMCU ESP8266 через Serial1
 * NodeMCU выполняет роль WiFi моста к серверу
 * Arduino DUE отправляет JSON данные и получает команды
 */
class WifiLink {
public:
    /**
     * Инициализация Serial1 для связи с NodeMCU
     */
    void begin();
    
    /**
     * Отправка данных датчиков на сервер через NodeMCU
     * @param sessionId идентификатор сессии
     * @param stepId идентификатор шага
     * @param ts временная метка
     * @param sensors снимок датчиков
     * @param image снимок изображения (может быть пустым)
     */
    void sendData(uint32_t sessionId, uint32_t stepId,
                  const DateTime& ts,
                  const SensorSnapshot& sensors,
                  const ImageSnapshot& image);
    
    /**
     * Ожидание команды от сервера через NodeMCU
     * @param outCmd структура для записи полученной команды
     * @param timeoutMs таймаут ожидания в миллисекундах
     * @return true если команда получена, false при таймауте
     */
    bool waitForCommand(Command& outCmd, uint32_t timeoutMs);

private:
    // Буфер для приема данных
    static const size_t LINE_BUFFER_SIZE = 512;
    char lineBuffer[LINE_BUFFER_SIZE];
    size_t lineBufferPos;
    
    /**
     * Форматирование временной метки в строку "dd:MM:yyyy hh:mm:ss"
     */
    void formatTimestamp(const DateTime& ts, char* buffer, size_t bufferSize);
    
    /**
     * Чтение строки из Serial1 до '\n' с таймаутом
     * @param buffer буфер для строки
     * @param bufferSize размер буфера
     * @param timeoutMs таймаут в миллисекундах
     * @return true если строка прочитана, false при таймауте
     */
    bool readLine(char* buffer, size_t bufferSize, uint32_t timeoutMs);
    
    /**
     * Base64 кодирование изображения (inline)
     */
    void sendImageBase64(const uint8_t* data, size_t size);
};

#endif // WIFI_LINK_H


