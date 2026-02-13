#include "../include/CameraModule.h"
#include "../include/types.h"
#include <Wire.h>

// ==================== OV7670 REGISTER DEFINITIONS ====================

#define REG_GAIN        0x00
#define REG_BLUE        0x01
#define REG_RED         0x02
#define REG_VREF        0x03
#define REG_COM1        0x04
#define REG_BAVE        0x05
#define REG_GbAVE       0x06
#define REG_AECHH       0x07
#define REG_RAVE        0x08
#define REG_COM2        0x09
#define REG_PID         0x0A
#define REG_VER         0x0B
#define REG_COM3        0x0C
#define REG_COM4        0x0D
#define REG_COM5        0x0E
#define REG_COM6        0x0F
#define REG_AECH        0x10
#define REG_CLKRC       0x11
#define REG_COM7        0x12
#define REG_COM8        0x13
#define REG_COM9        0x14
#define REG_COM10       0x15
#define REG_HSTART      0x17
#define REG_HSTOP       0x18
#define REG_VSTART      0x19
#define REG_VSTOP       0x1A
#define REG_PSHFT       0x1B
#define REG_MIDH        0x1C
#define REG_MIDL        0x1D
#define REG_MVFP        0x1E
#define REG_AEW         0x24
#define REG_AEB         0x25
#define REG_VPT         0x26
#define REG_HSYST       0x30
#define REG_HSYEN       0x31
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
#define REG_CMATRIX_1   0x4F
#define REG_CMATRIX_2   0x50
#define REG_CMATRIX_3   0x51
#define REG_CMATRIX_4   0x52
#define REG_CMATRIX_5   0x53
#define REG_CMATRIX_6   0x54
#define REG_BRIGHT      0x55
#define REG_CONTRAST    0x56
#define REG_CMATRIX_SIGN 0x58
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

// COM3 values
#define COM3_DCWEN      0x04
#define COM3_SCALEEN    0x08

// COM8 values
#define COM8_FASTAEC    0x80
#define COM8_AECSTEP    0x40
#define COM8_BFILT      0x20
#define COM8_AGC        0x04
#define COM8_AWB        0x02
#define COM8_AEC        0x01

// COM10 values
#define COM10_VS_NEG    0x02
#define COM10_HSYNC     0x40
#define COM10_PCLK_HB   0x20

// COM13 values
#define COM13_GAMMA     0x80
#define COM13_UVSAT     0x40
#define COM13_UVSWAP    0x01

// COM14 values
#define COM14_DCWEN     0x10
#define COM14_MAN_SCAL  0x08

// COM15 values
#define COM15_R00FF     0xC0
#define COM15_RGB565    0x10
#define COM15_RGB555    0x30

// COM16 values
#define COM16_AWBGAIN   0x08

// COM11 values
#define COM11_NIGHT     0x80
#define COM11_HZAUTO    0x10
#define COM11_50HZ      0x08
#define COM11_EXP       0x02

// TSLB values
#define TSLB_YLAST      0x04

// ==================== CAMERA CONFIGURATION TABLES ====================
// Ported from working ov7670_due_capture.ino

// QQVGA settings (160x120)
static const regval_list ov7670_qqvga[] = {
    {REG_HSTART, 0x16},
    {REG_HSTOP, 0x04},
    {REG_HREF, 0x24},
    {REG_VSTART, 0x02},
    {REG_VSTOP, 0x7A},
    {REG_VREF, 0x0A},
    {REG_COM3, 0x04},
    {REG_COM14, 0x1A},
    {REG_SCALING_XSC, 0x3A},
    {REG_SCALING_YSC, 0x35},
    {REG_SCALING_DCWCTR, 0x22},
    {REG_SCALING_PCLK_DIV, 0xF2},
    {REG_SCALING_PCLK_DELAY, 0x02},
    {REG_LIST_END_MARKER, REG_LIST_END_MARKER}
};

// RGB565 format
static const regval_list ov7670_rgb565[] = {
    {REG_RGB444, 0x00},
    {REG_COM15, COM15_R00FF | COM15_RGB565},
    {REG_TSLB, TSLB_YLAST},
    {REG_COM1, 0x00},
    {REG_COM9, 0x38},
    {REG_CMATRIX_1, 0xB3},
    {REG_CMATRIX_2, 0xB3},
    {REG_CMATRIX_3, 0x00},
    {REG_CMATRIX_4, 0x3D},
    {REG_CMATRIX_5, 0xA7},
    {REG_CMATRIX_6, 0xE4},
    {REG_COM13, COM13_GAMMA | COM13_UVSAT},
    {REG_LIST_END_MARKER, REG_LIST_END_MARKER}
};

// Full default configuration (gamma, AGC, AEC, AWB, white balance, matrix, etc.)
static const regval_list ov7670_default[] = {
    // Gamma curve values
    {0x7A, 0x20}, {0x7B, 0x10}, {0x7C, 0x1E}, {0x7D, 0x35},
    {0x7E, 0x5A}, {0x7F, 0x69}, {0x80, 0x76}, {0x81, 0x80},
    {0x82, 0x88}, {0x83, 0x8F}, {0x84, 0x96}, {0x85, 0xA3},
    {0x86, 0xAF}, {0x87, 0xC4}, {0x88, 0xD7}, {0x89, 0xE8},

    // AGC and AEC parameters
    {REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT},
    {REG_GAIN, 0x00}, {REG_AECH, 0x00},
    {REG_COM4, 0x40},
    {REG_BD50MAX, 0x05}, {REG_BD60MAX, 0x07},
    {REG_AEW, 0x95}, {REG_AEB, 0x33},
    {REG_VPT, 0xE3}, {REG_HAECC1, 0x78},
    {REG_HAECC2, 0x68}, {0xA1, 0x03},
    {REG_HAECC3, 0xD8}, {REG_HAECC4, 0xD8},
    {REG_HAECC5, 0xF0}, {REG_HAECC6, 0x90},
    {REG_HAECC7, 0x94},
    {REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC},

    // Reserved magic values
    {REG_COM5, 0x61}, {REG_COM6, 0x4B},
    {0x16, 0x02}, {REG_MVFP, 0x07},
    {0x21, 0x02}, {0x22, 0x91},
    {0x29, 0x07}, {0x33, 0x0B},
    {0x35, 0x0B}, {0x37, 0x1D},
    {0x38, 0x71}, {0x39, 0x2A},
    {REG_COM12, 0x78}, {0x4D, 0x40},
    {0x4E, 0x20}, {REG_GFIX, 0x00},
    {0x6B, 0x0A}, {0x74, 0x10},
    {0x8D, 0x4F}, {0x8E, 0x00},
    {0x8F, 0x00}, {0x90, 0x00},
    {0x91, 0x00}, {0x96, 0x00},
    {0x9A, 0x00}, {0xB0, 0x84},
    {0xB1, 0x0C}, {0xB2, 0x0E},
    {0xB3, 0x82}, {0xB8, 0x0A},

    // White balance tweaks
    {0x43, 0x0A}, {0x44, 0xF0},
    {0x45, 0x34}, {0x46, 0x58},
    {0x47, 0x28}, {0x48, 0x3A},
    {0x59, 0x88}, {0x5A, 0x88},
    {0x5B, 0x44}, {0x5C, 0x67},
    {0x5D, 0x49}, {0x5E, 0x0E},
    {0x6C, 0x0A}, {0x6D, 0x55},
    {0x6E, 0x11}, {0x6F, 0x9F},
    {0x6A, 0x40}, {REG_BLUE, 0x40},
    {REG_RED, 0x60},
    {REG_COM8, COM8_FASTAEC | COM8_AECSTEP | COM8_BFILT | COM8_AGC | COM8_AEC | COM8_AWB},

    // Matrix coefficients
    {0x4F, 0x80}, {0x50, 0x80},
    {0x51, 0x00}, {0x52, 0x22},
    {0x53, 0x5E}, {0x54, 0x80},
    {0x58, 0x9E},

    {REG_COM16, COM16_AWBGAIN}, {REG_EDGE, 0x00},
    {0x75, 0x05}, {0x76, 0xE1},
    {REG_DENOISE, 0x00}, {0x77, 0x01},
    {0x4B, 0x09},
    {0xC9, 0x60},
    {REG_CONTRAST, 0x40},

    {0x34, 0x11}, {REG_COM11, COM11_EXP | COM11_HZAUTO},
    {0xA4, 0x88}, {0x96, 0x00},
    {0x97, 0x30}, {0x98, 0x20},
    {0x99, 0x30}, {0x9A, 0x84},
    {0x9B, 0x29}, {0x9C, 0x03},
    {0x9D, 0x4C}, {0x9E, 0x3F},
    {0x78, 0x04},

    // Multiplexor register magic
    {0x79, 0x01}, {0xC8, 0xF0},
    {0x79, 0x0F}, {0xC8, 0x00},
    {0x79, 0x10}, {0xC8, 0x7E},
    {0x79, 0x0A}, {0xC8, 0x80},
    {0x79, 0x0B}, {0xC8, 0x01},
    {0x79, 0x0C}, {0xC8, 0x0F},
    {0x79, 0x0D}, {0xC8, 0x20},
    {0x79, 0x09}, {0xC8, 0x80},
    {0x79, 0x02}, {0xC8, 0xC0},
    {0x79, 0x03}, {0xC8, 0x40},
    {0x79, 0x05}, {0xC8, 0x30},
    {0x79, 0x26},

    {REG_LIST_END_MARKER, REG_LIST_END_MARKER}
};

// ==================== FAST PIN MANIPULATION ====================

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

// ==================== INITIALIZATION ====================

bool CameraModule::begin() {
    cameraInitialized = false;

    Serial.println("CameraModule: Initializing OV7670 (RGB565)...");

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

    Serial.println("CameraModule: Camera detected: OV7670");

    // === Configuration order matches working ov7670_due_capture.ino ===

    // 1. Set RGB565 output format
    writeRegister(REG_COM7, COM7_RGB);
    if (!writeRegisterList(ov7670_rgb565)) {
        Serial.println("CameraModule: ERROR - Failed to configure RGB565");
        return false;
    }
    Serial.println("CameraModule: RGB565 format configured");

    // 2. Set QQVGA resolution (160x120)
    if (!writeRegisterList(ov7670_qqvga)) {
        Serial.println("CameraModule: ERROR - Failed to configure QQVGA");
        return false;
    }
    Serial.println("CameraModule: QQVGA resolution configured");

    // 3. Set negative VSYNC
    writeRegister(REG_COM10, COM10_VS_NEG);

    // 4. Load full default settings (gamma, AGC, AEC, AWB, white balance, etc.)
    if (!writeRegisterList(ov7670_default)) {
        Serial.println("CameraModule: ERROR - Failed to configure defaults");
        return false;
    }
    Serial.println("CameraModule: Default settings loaded");

    delay(300); // Wait for settings to apply

    cameraInitialized = true;
    Serial.println("CameraModule: Initialized successfully (RGB565 QQVGA 160x120 -> 160x120 grayscale)");
    return true;
}

// ==================== CAPTURE ====================

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

    // Wait for VSYNC (frame boundary)
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

    // Read RGB565 image from FIFO (160x120)
    // Convert RGB565 -> grayscale: Y = (R*77 + G*150 + B*29) >> 8
    // No downsampling - full resolution 160x120

    size_t bufIdx = 0;
    for (int y = 0; y < CAPTURE_HEIGHT; y++) {
        for (int x = 0; x < CAPTURE_WIDTH; x++) {
            // RGB565: 2 bytes per pixel, little-endian from OV7670 FIFO
            uint8_t b1 = readByte();  // Low byte
            uint8_t b2 = readByte();  // High byte

            // Decode RGB565: RRRRRGGG GGGBBBBB (big-endian after combining)
            uint16_t rgb565 = (b2 << 8) | b1;

            uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;  // 5-bit R -> 8-bit
            uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;   // 6-bit G -> 8-bit
            uint8_t b = (rgb565 & 0x1F) << 3;           // 5-bit B -> 8-bit

            // Convert to grayscale using integer luminance formula
            // Y = (R*77 + G*150 + B*29) >> 8
            uint8_t gray = (uint8_t)(((uint16_t)r * 77 + (uint16_t)g * 150 + (uint16_t)b * 29) >> 8);

            if (bufIdx < IMAGE_BUFFER_SIZE) {
                imageBuffer[bufIdx++] = gray;
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

// ==================== PIN SETUP ====================

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
    digitalWrite(Hardware::CAM_RST, HIGH);    // Camera not reset
    digitalWrite(Hardware::CAM_WR, LOW);      // Write disabled
    digitalWrite(Hardware::CAM_WRST, HIGH);   // Write reset inactive
    digitalWrite(Hardware::CAM_RRST, HIGH);   // Read reset inactive
    digitalWrite(Hardware::CAM_OE, HIGH);     // Output disabled
    digitalWrite(Hardware::CAM_RCK, LOW);     // Read clock low
}

// ==================== I2C REGISTER ACCESS ====================

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

bool CameraModule::writeRegisterList(const regval_list* list) {
    uint8_t i = 0;
    while (list[i].reg != REG_LIST_END_MARKER) {
        if (!writeRegister(list[i].reg, list[i].val)) {
            Serial.print("CameraModule: Failed to write reg 0x");
            Serial.println(list[i].reg, HEX);
            return false;
        }

        // Extra delay for reset command
        if (list[i].reg == REG_COM7 && list[i].val == COM7_RESET) {
            delay(200);
        }
        i++;
    }
    return true;
}

// ==================== FIFO DATA READ ====================

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

// ==================== FIFO CONTROL ====================

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
