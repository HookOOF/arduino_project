#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

#include "types.h"

// Register-value pair for OV7670 configuration
struct regval_list {
    uint8_t reg;
    uint8_t val;
};

#define REG_LIST_END_MARKER 0xFF

/**
 * Модуль камеры OV7670 + AL422B FIFO для Arduino Due
 * Захватывает изображение в RGB565, конвертирует в grayscale для LLM
 */
class CameraModule {
public:
    /**
     * Инициализация камеры OV7670
     * @return true если успешно, false в противном случае
     */
    bool begin();
    
    /**
     * Захват изображения, если достаточно света
     * @param isDark флаг темноты от фоторезистора
     * @return структура ImageSnapshot с данными изображения
     */
    ImageSnapshot captureIfLight(bool isDark);
    
    /**
     * Захват изображения без проверки освещенности
     * @return структура ImageSnapshot с данными изображения
     */
    ImageSnapshot capture();
    
    /**
     * Проверка, инициализирована ли камера
     */
    bool isInitialized() const { return cameraInitialized; }

private:
    bool cameraInitialized;
    
    // Буфер для изображения в grayscale (80x60 = 4800 байт)
    static const uint16_t IMAGE_WIDTH = Hardware::CAM_WIDTH;
    static const uint16_t IMAGE_HEIGHT = Hardware::CAM_HEIGHT;
    // Исходное разрешение камеры QQVGA
    static const uint16_t CAPTURE_WIDTH = 160;
    static const uint16_t CAPTURE_HEIGHT = 120;
    static const size_t IMAGE_BUFFER_SIZE = IMAGE_WIDTH * IMAGE_HEIGHT;
    uint8_t imageBuffer[IMAGE_BUFFER_SIZE];
    
    // OV7670 I2C address
    static const uint8_t OV7670_I2C_ADDR = 0x21;
    
    // Внутренние методы для работы с камерой
    void setupPins();
    void resetCamera();
    bool writeRegister(uint8_t reg, uint8_t val);
    uint8_t readRegister(uint8_t reg);
    bool writeRegisterList(const regval_list* list);
    uint8_t readByte();
    
    // Методы управления FIFO
    void fifoWriteEnable();
    void fifoWriteDisable();
    void fifoWriteReset();
    void fifoReadReset();
    void fifoOutputEnable();
    void fifoOutputDisable();
    void fifoReadClockHigh();
    void fifoReadClockLow();
};

#endif // CAMERA_MODULE_H
