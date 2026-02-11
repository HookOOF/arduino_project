#include "../include/CameraModule.h"
#include "../include/types.h"
#include <Wire.h>

// OV7670 Register definitions
#define REG_GAIN        0x00
#define REG_BLUE        0x01
#define REG_RED         0x02
#define REG_VREF        0x03
#define REG_COM1        0x04
#define REG_COM2        0x09
#define REG_PID         0x0A
#define REG_VER         0x0B
#define REG_COM3        0x0C
#define REG_COM4        0x0D
#define REG_COM5        0x0E
#define REG_COM6        0x0F
#define REG_CLKRC       0x11
#define REG_COM7        0x12
#define REG_COM8        0x13
#define REG_COM9        0x14
#define REG_COM10       0x15
#define REG_HSTART      0x17
#define REG_HSTOP       0x18
#define REG_VSTART      0x19
#define REG_VSTOP       0x1A
#define REG_MIDH        0x1C
#define REG_MIDL        0x1D
#define REG_MVFP        0x1E
#define REG_AEW         0x24
#define REG_AEB         0x25
#define REG_VPT         0x26
#define REG_HREF        0x32
#define REG_TSLB        0x3A
#define REG_COM11       0x3B
#define REG_COM12       0x3C
#define REG_COM13       0x3D
#define REG_COM14       0x3E
#define REG_EDGE        0x3F
#define REG_COM15       0x40
#define REG_COM16       0x41
#define REG_COM17       0x42
#define REG_DENOISE     0x4C
#define REG_BRIGHT      0x55
#define REG_CONTRAST    0x56
#define REG_MANU        0x67
#define REG_MANV        0x68
#define REG_GFIX        0x69
#define REG_GGAIN       0x6A
#define REG_DBLV        0x6B
#define REG_SCALING_XSC 0x70
#define REG_SCALING_YSC 0x71
#define REG_SCALING_DCWCTR 0x72
#define REG_SCALING_PCLK_DIV 0x73
#define REG_REG76       0x76
#define REG_RGB444      0x8C
#define REG_HAECC1      0x9F
#define REG_HAECC2      0xA0
#define REG_SCALING_PCLK_DELAY 0xA2
#define REG_BD50MAX     0xA5
#define REG_HAECC3      0xA6
#define REG_HAECC4      0xA7
#define REG_HAECC5      0xA8
#define REG_HAECC6      0xA9
#define REG_HAECC7      0xAA
#define REG_BD60MAX     0xAB

// COM7 values
#define COM7_RESET      0x80
#define COM7_RGB        0x04
#define COM7_YUV        0x00

// COM8 values
#define COM8_FASTAEC    0x80
#define COM8_AECSTEP    0x40
#define COM8_BFILT      0x20
#define COM8_AGC        0x04
#define COM8_AWB        0x02
#define COM8_AEC        0x01

// COM10 values
#define COM10_VS_NEG    0x02

// COM11 values
#define COM11_HZAUTO    0x10
#define COM11_EXP       0x02

// COM13 values
#define COM13_GAMMA     0x80
#define COM13_UVSAT     0x40
#define COM13_UVSWAP    0x01

// COM15 values
#define COM15_R00FF     0xC0
#define COM15_RGB565    0x10

// COM16 values
#define COM16_AWBGAIN   0x08

// TSLB values
#define TSLB_YLAST      0x04

// Полная конфигурация OV7670 для QQVGA YUV422
// Регистры перенесены из рабочей прошивки (ov7670_due_capture)
// Уменьшаем 160x120 → 80x60 программно при чтении (берём каждый 2-й пиксель)
static const uint8_t ov7670_qqvga_yuv[] = {
    // === FORMAT: YUV422 ===
    REG_COM7, COM7_YUV,                // YUV output mode
    REG_CLKRC, 0x01,                   // Clock prescaler /2
    REG_RGB444, 0x00,                  // Disable RGB444
    REG_COM15, COM15_R00FF,            // Full output range [00-FF]
    REG_TSLB, TSLB_YLAST,             // YUYV byte order (Y first)
    REG_COM1, 0x00,                    // No CCIR656
    REG_COM9, 0x68,                    // AGC ceiling 128x
    
    // === YUV Color Matrix (from ov7670_yuv422 table) ===
    0x4F, 0x80,                        // Matrix coefficient 1
    0x50, 0x80,                        // Matrix coefficient 2
    0x51, 0x00,                        // Matrix coefficient 3
    0x52, 0x22,                        // Matrix coefficient 4
    0x53, 0x5E,                        // Matrix coefficient 5
    0x54, 0x80,                        // Matrix coefficient 6
    0x58, 0x9E,                        // Matrix sign
    REG_COM13, COM13_GAMMA | COM13_UVSAT | COM13_UVSWAP, // Gamma + UV sat + UV swap
    
    // === RESOLUTION: QQVGA (160x120) ===
    REG_COM3, 0x04,                    // Enable scaling
    REG_COM14, 0x1A,                   // Manual scaling, PCLK divider
    REG_SCALING_XSC, 0x3A,
    REG_SCALING_YSC, 0x35,
    REG_SCALING_DCWCTR, 0x22,          // Downsample by 4
    REG_SCALING_PCLK_DIV, 0xF2,
    REG_SCALING_PCLK_DELAY, 0x02,
    REG_HSTART, 0x16,
    REG_HSTOP, 0x04,
    REG_HREF, 0x24,
    REG_VSTART, 0x02,
    REG_VSTOP, 0x7A,
    REG_VREF, 0x0A,
    
    // === VSYNC ===
    REG_COM10, COM10_VS_NEG,           // VSYNC negative polarity
    
    // === Gamma Curve ===
    0x7A, 0x20, 0x7B, 0x10, 0x7C, 0x1E, 0x7D, 0x35,
    0x7E, 0x5A, 0x7F, 0x69, 0x80, 0x76, 0x81, 0x80,
    0x82, 0x88, 0x83, 0x8F, 0x84, 0x96, 0x85, 0xA3,
    0x86, 0xAF, 0x87, 0xC4, 0x88, 0xD7, 0x89, 0xE8,
    
    // === AGC and AEC Parameters (progressive enable) ===
    REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT, // Step 1: basic
    REG_GAIN, 0x00,
    0x10, 0x00,                        // AECH
    REG_COM4, 0x40,
    REG_BD50MAX, 0x05,
    REG_BD60MAX, 0x07,
    REG_AEW, 0x95,
    REG_AEB, 0x33,
    REG_VPT, 0xE3,
    REG_HAECC1, 0x78,
    REG_HAECC2, 0x68,
    0xA1, 0x03,                        // Reserved
    REG_HAECC3, 0xD8,
    REG_HAECC4, 0xD8,
    REG_HAECC5, 0xF0,
    REG_HAECC6, 0x90,
    REG_HAECC7, 0x94,
    REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC, // Step 2: +AGC +AEC
    
    // === Reserved / Magic Values ===
    REG_COM5, 0x61,
    REG_COM6, 0x4B,
    0x16, 0x02,
    REG_MVFP, 0x07,                   // Mirror/VFlip
    0x21, 0x02, 0x22, 0x91,
    0x29, 0x07, 0x33, 0x0B,
    0x35, 0x0B, 0x37, 0x1D,
    0x38, 0x71, 0x39, 0x2A,
    REG_COM12, 0x78,
    0x4D, 0x40, 0x4E, 0x20,
    REG_GFIX, 0x00,
    REG_DBLV, 0x0A,                   // PLL and regulator
    0x74, 0x10,
    0x8D, 0x4F, 0x8E, 0x00,
    0x8F, 0x00, 0x90, 0x00,
    0x91, 0x00, 0x96, 0x00,
    0x9A, 0x00, 0xB0, 0x84,
    0xB1, 0x0C, 0xB2, 0x0E,
    0xB3, 0x82, 0xB8, 0x0A,
    
    // === White Balance ===
    0x43, 0x0A, 0x44, 0xF0,
    0x45, 0x34, 0x46, 0x58,
    0x47, 0x28, 0x48, 0x3A,
    0x59, 0x88, 0x5A, 0x88,
    0x5B, 0x44, 0x5C, 0x67,
    0x5D, 0x49, 0x5E, 0x0E,
    0x6C, 0x0A, 0x6D, 0x55,
    0x6E, 0x11, 0x6F, 0x9F,
    REG_GGAIN, 0x40,                  // G channel gain
    REG_BLUE, 0x40,
    REG_RED, 0x60,
    REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC | COM8_AWB, // Step 3: +AWB
    
    // === Edge / Denoise / Contrast ===
    REG_COM16, COM16_AWBGAIN,         // AWB gain enable
    REG_EDGE, 0x00,
    0x75, 0x05,
    REG_REG76, 0xE1,
    REG_DENOISE, 0x00,                // Denoise strength
    0x77, 0x01,
    0x4B, 0x09,
    0xC9, 0x60,
    REG_BRIGHT, 0x00,
    REG_CONTRAST, 0x40,
    
    // === AEC / Exposure Timing ===
    0x34, 0x11,
    REG_COM11, COM11_EXP | COM11_HZAUTO, // Exposure + Hz auto detect
    0xA4, 0x88, 0x96, 0x00,
    0x97, 0x30, 0x98, 0x20,
    0x99, 0x30, 0x9A, 0x84,
    0x9B, 0x29, 0x9C, 0x03,
    0x9D, 0x4C, 0x9E, 0x3F,
    0x78, 0x04,
    
    // === Multiplexor Register Sequences ===
    0x79, 0x01, 0xC8, 0xF0,
    0x79, 0x0F, 0xC8, 0x00,
    0x79, 0x10, 0xC8, 0x7E,
    0x79, 0x0A, 0xC8, 0x80,
    0x79, 0x0B, 0xC8, 0x01,
    0x79, 0x0C, 0xC8, 0x0F,
    0x79, 0x0D, 0xC8, 0x20,
    0x79, 0x09, 0xC8, 0x80,
    0x79, 0x02, 0xC8, 0xC0,
    0x79, 0x03, 0xC8, 0x40,
    0x79, 0x05, 0xC8, 0x30,
    0x79, 0x26,
    
    0xFF, 0xFF                         // End marker
};

// Fast pin manipulation for Arduino Due
inline void pinHigh(int pin) {
    g_APinDescription[pin].pPort->PIO_SODR = g_APinDescription[pin].ulPin;
}

inline void pinLow(int pin) {
    g_APinDescription[pin].pPort->PIO_CODR = g_APinDescription[pin].ulPin;
}

inline int pinRead(int pin) {
    return (g_APinDescription[pin].pPort->PIO_PDSR & g_APinDescription[pin].ulPin) ? 1 : 0;
}

bool CameraModule::begin() {
    cameraInitialized = false;
    
    Serial.println("CameraModule: Initializing OV7670...");
    
    // Setup pins
    setupPins();
    delay(100);
    
    // Initialize I2C for camera (using Wire, not Wire1 which is for MPU6050)
    Wire.begin();
    Wire.setClock(100000); // 100kHz for SCCB
    
    // Reset camera
    resetCamera();
    
    // Check camera presence by reading product ID
    uint8_t pid = readRegister(REG_PID);
    uint8_t ver = readRegister(REG_VER);
    
    Serial.print("CameraModule: PID=0x");
    Serial.print(pid, HEX);
    Serial.print(", VER=0x");
    Serial.println(ver, HEX);
    
    // OV7670 should return PID=0x76
    if (pid != 0x76) {
        Serial.println("CameraModule: ERROR - Camera not detected!");
        return false;
    }
    
    // Configure camera for QQVGA YUV
    if (!writeRegisterList(ov7670_qqvga_yuv)) {
        Serial.println("CameraModule: ERROR - Failed to configure camera");
        return false;
    }
    
    delay(300); // Wait for settings to apply
    
    cameraInitialized = true;
    Serial.println("CameraModule: Initialized successfully");
    return true;
}

ImageSnapshot CameraModule::captureIfLight(bool isDark) {
    ImageSnapshot snapshot;
    snapshot.available = false;
    snapshot.width = 0;
    snapshot.height = 0;
    snapshot.buffer = nullptr;
    snapshot.bufferSize = 0;
    
    // Если темно или камера не инициализирована - не захватываем
    if (isDark || !cameraInitialized) {
        return snapshot;
    }
    
    return capture();
}

ImageSnapshot CameraModule::capture() {
    ImageSnapshot snapshot;
    snapshot.available = false;
    snapshot.width = 0;
    snapshot.height = 0;
    snapshot.buffer = nullptr;
    snapshot.bufferSize = 0;
    
    if (!cameraInitialized) {
        return snapshot;
    }
    
    // Wait for VSYNC falling edge (start of frame)
    while (digitalRead(Hardware::CAM_VSYNC) == HIGH);
    while (digitalRead(Hardware::CAM_VSYNC) == LOW);
    
    // Reset FIFO write pointer and enable write
    fifoWriteReset();
    fifoWriteEnable();
    
    // Wait for frame to be written (next VSYNC)
    while (digitalRead(Hardware::CAM_VSYNC) == HIGH);
    
    // Disable write
    fifoWriteDisable();
    
    delay(1);
    
    // Enable FIFO output and reset read pointer
    fifoOutputEnable();
    fifoReadReset();
    
    // Read image from FIFO
    // QQVGA = 160x120, мы берем каждый второй пиксель для 80x60
    // YUV422 = 2 bytes per pixel (Y, U or V)
    // Мы берем только Y (яркость) для grayscale
    
    size_t bufIdx = 0;
    for (int y = 0; y < 120; y++) {
        for (int x = 0; x < 160; x++) {
            uint8_t byte1 = readByte(); // Y or U
            uint8_t byte2 = readByte(); // V or Y (depends on position)
            
            // Берем каждый второй пиксель по X и Y для уменьшения до 80x60
            if ((x % 2 == 0) && (y % 2 == 0)) {
                // byte1 содержит Y (яркость)
                if (bufIdx < IMAGE_BUFFER_SIZE) {
                    imageBuffer[bufIdx++] = byte1;
                }
            }
        }
    }
    
    // Disable output
    fifoOutputDisable();
    
    snapshot.available = true;
    snapshot.width = IMAGE_WIDTH;
    snapshot.height = IMAGE_HEIGHT;
    snapshot.buffer = imageBuffer;
    snapshot.bufferSize = bufIdx;
    
    return snapshot;
}

void CameraModule::setupPins() {
    // Control pins as outputs
    pinMode(Hardware::CAM_RST, OUTPUT);
    pinMode(Hardware::CAM_WR, OUTPUT);
    pinMode(Hardware::CAM_WRST, OUTPUT);
    pinMode(Hardware::CAM_RRST, OUTPUT);
    pinMode(Hardware::CAM_OE, OUTPUT);
    pinMode(Hardware::CAM_RCK, OUTPUT);
    
    // VSYNC as input
    pinMode(Hardware::CAM_VSYNC, INPUT);
    
    // Data pins as inputs
    pinMode(Hardware::CAM_D0, INPUT);
    pinMode(Hardware::CAM_D1, INPUT);
    pinMode(Hardware::CAM_D2, INPUT);
    pinMode(Hardware::CAM_D3, INPUT);
    pinMode(Hardware::CAM_D4, INPUT);
    pinMode(Hardware::CAM_D5, INPUT);
    pinMode(Hardware::CAM_D6, INPUT);
    pinMode(Hardware::CAM_D7, INPUT);
    
    // Initialize control pins to safe state
    digitalWrite(Hardware::CAM_RST, HIGH);
    digitalWrite(Hardware::CAM_WR, LOW);
    digitalWrite(Hardware::CAM_WRST, HIGH);
    digitalWrite(Hardware::CAM_RRST, HIGH);
    digitalWrite(Hardware::CAM_OE, HIGH);
    digitalWrite(Hardware::CAM_RCK, LOW);
}

void CameraModule::resetCamera() {
    // Hardware reset
    digitalWrite(Hardware::CAM_RST, LOW);
    delay(10);
    digitalWrite(Hardware::CAM_RST, HIGH);
    delay(100);
    
    // Software reset
    writeRegister(REG_COM7, COM7_RESET);
    delay(200);
}

bool CameraModule::writeRegister(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(OV7670_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    uint8_t result = Wire.endTransmission();
    delay(1);
    return (result == 0);
}

uint8_t CameraModule::readRegister(uint8_t reg) {
    Wire.beginTransmission(OV7670_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    
    Wire.requestFrom((uint8_t)OV7670_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF;
}

bool CameraModule::writeRegisterList(const uint8_t* list) {
    uint8_t i = 0;
    while (list[i] != 0xFF || list[i + 1] != 0xFF) {
        uint8_t reg = list[i];
        uint8_t val = list[i + 1];
        
        if (!writeRegister(reg, val)) {
            Serial.print("CameraModule: Failed to write reg 0x");
            Serial.println(reg, HEX);
            return false;
        }
        
        // Extra delay for reset command
        if (reg == REG_COM7 && val == COM7_RESET) {
            delay(200);
        }
        
        i += 2;
    }
    return true;
}

uint8_t CameraModule::readByte() {
    uint8_t data = 0;
    
    fifoReadClockHigh();
    delayMicroseconds(1);
    
    if (pinRead(Hardware::CAM_D7)) data |= 0x80;
    if (pinRead(Hardware::CAM_D6)) data |= 0x40;
    if (pinRead(Hardware::CAM_D5)) data |= 0x20;
    if (pinRead(Hardware::CAM_D4)) data |= 0x10;
    if (pinRead(Hardware::CAM_D3)) data |= 0x08;
    if (pinRead(Hardware::CAM_D2)) data |= 0x04;
    if (pinRead(Hardware::CAM_D1)) data |= 0x02;
    if (pinRead(Hardware::CAM_D0)) data |= 0x01;
    
    fifoReadClockLow();
    delayMicroseconds(1);
    
    return data;
}

void CameraModule::fifoWriteEnable() {
    pinHigh(Hardware::CAM_WR);
}

void CameraModule::fifoWriteDisable() {
    pinLow(Hardware::CAM_WR);
}

void CameraModule::fifoWriteReset() {
    pinLow(Hardware::CAM_WRST);
    delayMicroseconds(1);
    pinHigh(Hardware::CAM_WRST);
}

void CameraModule::fifoReadReset() {
    pinLow(Hardware::CAM_RRST);
    fifoReadClockHigh();
    delayMicroseconds(1);
    fifoReadClockLow();
    delayMicroseconds(1);
    pinHigh(Hardware::CAM_RRST);
    fifoReadClockHigh();
    delayMicroseconds(1);
    fifoReadClockLow();
}

void CameraModule::fifoOutputEnable() {
    pinLow(Hardware::CAM_OE);
}

void CameraModule::fifoOutputDisable() {
    pinHigh(Hardware::CAM_OE);
}

void CameraModule::fifoReadClockHigh() {
    pinHigh(Hardware::CAM_RCK);
}

void CameraModule::fifoReadClockLow() {
    pinLow(Hardware::CAM_RCK);
}


