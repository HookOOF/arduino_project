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
    
    // Константы для чанкированной передачи изображения
    static const size_t CHUNK_RAW_SIZE = 192;      // 192 байт raw = 256 байт base64
    static const size_t CHUNK_BASE64_SIZE = 256;   // Размер base64 чанка
    static const uint8_t MAX_RETRIES = 3;          // Макс. попыток передачи чанка
    static const uint32_t ACK_TIMEOUT_MS = 500;    // Таймаут ожидания ACK (увеличен для 115200)
    
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
     * CRC16-CCITT вычисление для проверки целостности
     * @param data указатель на данные
     * @param len длина данных
     * @return 16-битная контрольная сумма
     */
    uint16_t crc16_ccitt(const uint8_t* data, size_t len);
    
    /**
     * Отправка изображения с чанкированием и подтверждением
     * @param data указатель на raw данные изображения
     * @param size размер данных
     * @param width ширина изображения
     * @param height высота изображения
     * @return true если передача успешна
     */
    bool sendImageChunked(const uint8_t* data, size_t size, uint16_t width, uint16_t height);
    
    /**
     * Отправка одного чанка с ожиданием ACK
     * @param chunkIdx индекс чанка
     * @param base64Data base64-закодированные данные чанка
     * @return true если ACK получен
     */
    bool sendChunkWithAck(uint16_t chunkIdx, const char* base64Data);
    
    /**
     * Ожидание ACK/NAK от NodeMCU
     * @param expectedChunkIdx ожидаемый индекс чанка
     * @return true если ACK получен для нужного чанка
     */
    bool waitForAck(uint16_t expectedChunkIdx);
};

#endif // WIFI_LINK_H


