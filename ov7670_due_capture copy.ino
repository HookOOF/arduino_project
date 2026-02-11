/*
 * OV7670 + AL422B FIFO Camera Capture for Arduino Due
 * 
 * Pin Configuration:
 * ------------------
 * VSYNC  -> Pin 40
 * SIO_C  -> SCL (Pin 21)
 * SIO_D  -> SDA (Pin 20)
 * /RST   -> Pin 22
 * WR     -> Pin 38
 * /WRST  -> Pin 37
 * /RRST  -> Pin 35
 * /OE    -> Pin 39
 * RCK    -> Pin 36
 * D0-D7  -> Pins 51,50,49,48,47,46,45,44
 * 
 * PWDN   -> GND (always active)
 * 
 * Connect camera to 3.3V power supply
 */

#include <Wire.h>

// ==================== PIN DEFINITIONS ====================
// Control pins
#define PIN_VSYNC   40
#define PIN_RST     22
#define PIN_WR      38   // Write Enable (active HIGH for AL422B)
#define PIN_WRST    37   // Write Reset (active LOW)
#define PIN_RRST    35   // Read Reset (active LOW)
#define PIN_OE      39   // Output Enable (active LOW)
#define PIN_RCK     36   // Read Clock

// Data pins D0-D7
#define PIN_D0      51
#define PIN_D1      50
#define PIN_D2      49
#define PIN_D3      48
#define PIN_D4      47
#define PIN_D5      46
#define PIN_D6      45
#define PIN_D7      44

// ==================== CAMERA SETTINGS ====================
#define OV7670_I2C_ADDR   0x21  // 7-bit address (0x42 >> 1)

// Image resolution (QQVGA)
#define IMG_WIDTH   160
#define IMG_HEIGHT  120

// Image resolution (QVGA) - uncomment to use
// #define IMG_WIDTH   320
// #define IMG_HEIGHT  240

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
#define COM7_BAYER      0x01
#define COM7_PBAYER     0x05
#define COM7_FMT_QVGA   0x10
#define COM7_FMT_QQVGA  0x00

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
#define TSLB_UV         0x10
#define TSLB_NEGATIVE   0x20

// RGB444 values
#define R444_ENABLE     0x02
#define R444_XBGR       0x00

// ==================== REGISTER VALUE STRUCTURE ====================
struct regval_list {
  uint8_t reg;
  uint8_t val;
};

#define END_MARKER 0xFF

// ==================== CAMERA CONFIGURATION TABLES ====================

// QQVGA settings (160x120)
const regval_list ov7670_qqvga[] = {
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
  {END_MARKER, END_MARKER}
};

// QVGA settings (320x240)
const regval_list ov7670_qvga[] = {
  {REG_HSTART, 0x16},
  {REG_HSTOP, 0x04},
  {REG_HREF, 0x00},
  {REG_VSTART, 0x02},
  {REG_VSTOP, 0x7A},
  {REG_VREF, 0x0A},
  {REG_COM3, 0x04},
  {REG_COM14, 0x19},
  {REG_SCALING_XSC, 0x3A},
  {REG_SCALING_YSC, 0x35},
  {REG_SCALING_DCWCTR, 0x11},
  {REG_SCALING_PCLK_DIV, 0xF1},
  {REG_SCALING_PCLK_DELAY, 0x02},
  {END_MARKER, END_MARKER}
};

// RGB565 format
const regval_list ov7670_rgb565[] = {
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
  {END_MARKER, END_MARKER}
};

// YUV422 format
const regval_list ov7670_yuv422[] = {
  {REG_RGB444, 0x00},
  {REG_COM15, COM15_R00FF},
  {REG_TSLB, TSLB_YLAST},
  {REG_COM1, 0x00},
  {REG_COM9, 0x68},
  {REG_BRIGHT, 0x00},
  {REG_CMATRIX_1, 0x80},
  {REG_CMATRIX_2, 0x80},
  {REG_CMATRIX_3, 0x00},
  {REG_CMATRIX_4, 0x22},
  {REG_CMATRIX_5, 0x5E},
  {REG_CMATRIX_6, 0x80},
  {REG_COM13, COM13_GAMMA | COM13_UVSAT | COM13_UVSWAP},
  {END_MARKER, END_MARKER}
};

// Default configuration values
const regval_list ov7670_default[] = {
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
  
  {END_MARKER, END_MARKER}
};

// ==================== GLOBAL VARIABLES ====================
volatile bool vsyncFlag = false;
volatile bool captureRequest = false;
volatile bool captureDone = true;
volatile bool busy = false;

// ==================== FIFO CONTROL FUNCTIONS ====================

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

// FIFO control macros
#define WRITE_ENABLE()    pinHigh(PIN_WR)    // WR high = write enabled
#define WRITE_DISABLE()   pinLow(PIN_WR)     // WR low = write disabled

#define READ_CLOCK_HIGH() pinHigh(PIN_RCK)
#define READ_CLOCK_LOW()  pinLow(PIN_RCK)

#define WRITE_RESET() do { pinLow(PIN_WRST); delayMicroseconds(1); pinHigh(PIN_WRST); } while(0)

#define READ_RESET() do { \
  pinLow(PIN_RRST); \
  READ_CLOCK_HIGH(); \
  delayMicroseconds(1); \
  READ_CLOCK_LOW(); \
  delayMicroseconds(1); \
  pinHigh(PIN_RRST); \
  READ_CLOCK_HIGH(); \
  delayMicroseconds(1); \
  READ_CLOCK_LOW(); \
} while(0)

#define OUTPUT_ENABLE()   pinLow(PIN_OE)     // /OE low = output enabled
#define OUTPUT_DISABLE()  pinHigh(PIN_OE)    // /OE high = output disabled

// ==================== I2C FUNCTIONS ====================

bool writeRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(OV7670_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  uint8_t result = Wire.endTransmission();
  delay(1);
  return (result == 0);
}

uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(OV7670_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom((uint8_t)OV7670_I2C_ADDR, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;
}

bool writeRegisterList(const regval_list* list) {
  uint8_t i = 0;
  while (list[i].reg != END_MARKER) {
    if (!writeRegister(list[i].reg, list[i].val)) {
      Serial.print("Failed to write register 0x");
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

// ==================== CAMERA INITIALIZATION ====================

void setupPins() {
  // Control pins as outputs
  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_WR, OUTPUT);
  pinMode(PIN_WRST, OUTPUT);
  pinMode(PIN_RRST, OUTPUT);
  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_RCK, OUTPUT);
  
  // VSYNC as input
  pinMode(PIN_VSYNC, INPUT);
  
  // Data pins as inputs
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
  pinMode(PIN_D2, INPUT);
  pinMode(PIN_D3, INPUT);
  pinMode(PIN_D4, INPUT);
  pinMode(PIN_D5, INPUT);
  pinMode(PIN_D6, INPUT);
  pinMode(PIN_D7, INPUT);
  
  // Initialize control pins to safe state
  digitalWrite(PIN_RST, HIGH);    // Camera not reset
  digitalWrite(PIN_WR, LOW);      // Write disabled
  digitalWrite(PIN_WRST, HIGH);   // Write reset inactive
  digitalWrite(PIN_RRST, HIGH);   // Read reset inactive
  digitalWrite(PIN_OE, HIGH);     // Output disabled
  digitalWrite(PIN_RCK, LOW);     // Read clock low
}

void resetCamera() {
  // Hardware reset
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(100);
  
  // Software reset
  writeRegister(REG_COM7, COM7_RESET);
  delay(200);
}

bool initCamera() {
  Serial.println("Initializing camera...");
  
  // Reset camera
  resetCamera();
  
  // Check camera presence by reading product ID
  uint8_t pid = readRegister(REG_PID);
  uint8_t ver = readRegister(REG_VER);
  
  Serial.print("Camera PID: 0x");
  Serial.print(pid, HEX);
  Serial.print(", VER: 0x");
  Serial.println(ver, HEX);
  
  // OV7670 should return PID=0x76, VER=0x73
  if (pid != 0x76) {
    Serial.println("ERROR: Camera not detected!");
    return false;
  }
  
  Serial.println("Camera detected: OV7670");
  
  // Set format: RGB565
  writeRegister(REG_COM7, COM7_RGB);
  if (!writeRegisterList(ov7670_rgb565)) {
    Serial.println("ERROR: Failed to configure RGB565");
    return false;
  }
  
  // Set resolution
  #if IMG_WIDTH == 160
    if (!writeRegisterList(ov7670_qqvga)) {
      Serial.println("ERROR: Failed to configure QQVGA");
      return false;
    }
    Serial.println("Resolution: QQVGA (160x120)");
  #else
    if (!writeRegisterList(ov7670_qvga)) {
      Serial.println("ERROR: Failed to configure QVGA");
      return false;
    }
    Serial.println("Resolution: QVGA (320x240)");
  #endif
  
  // Set negative VSYNC
  writeRegister(REG_COM10, COM10_VS_NEG);
  
  // Load default settings
  if (!writeRegisterList(ov7670_default)) {
    Serial.println("ERROR: Failed to configure defaults");
    return false;
  }
  
  Serial.println("Camera initialized successfully!");
  return true;
}

// ==================== VSYNC INTERRUPT ====================

void vsyncHandler() {
  if (captureRequest) {
    // Start capturing - reset write pointer and enable write
    WRITE_RESET();
    WRITE_ENABLE();
    captureRequest = false;
    captureDone = false;
  } else if (!captureDone && busy) {
    // End of frame - disable write
    WRITE_DISABLE();
    busy = false;
    captureDone = true;
  }
}

// ==================== IMAGE CAPTURE ====================

void captureStart() {
  captureRequest = true;
  busy = true;
  captureDone = false;
}

bool captureWait() {
  uint32_t timeout = millis() + 2000;  // 2 second timeout
  while (!captureDone) {
    if (millis() > timeout) {
      Serial.println("Capture timeout!");
      return false;
    }
  }
  return true;
}

uint8_t readByte() {
  uint8_t data = 0;
  
  READ_CLOCK_HIGH();
  delayMicroseconds(1);
  
  // Read data bits
  if (pinRead(PIN_D7)) data |= 0x80;
  if (pinRead(PIN_D6)) data |= 0x40;
  if (pinRead(PIN_D5)) data |= 0x20;
  if (pinRead(PIN_D4)) data |= 0x10;
  if (pinRead(PIN_D3)) data |= 0x08;
  if (pinRead(PIN_D2)) data |= 0x04;
  if (pinRead(PIN_D1)) data |= 0x02;
  if (pinRead(PIN_D0)) data |= 0x01;
  
  READ_CLOCK_LOW();
  delayMicroseconds(1);
  
  return data;
}

// Read and send image via Serial in custom binary format
void sendImage() {
  Serial.println("Capturing image...");
  
  // Request capture
  captureStart();
  
  // Wait for VSYNC
  while (digitalRead(PIN_VSYNC) == HIGH);  // Wait for LOW
  while (digitalRead(PIN_VSYNC) == LOW);   // Wait for HIGH - frame starts
  
  // Start write to FIFO
  WRITE_RESET();
  WRITE_ENABLE();
  
  // Wait for frame to be written (another VSYNC)
  while (digitalRead(PIN_VSYNC) == HIGH);  // Wait for LOW (end of frame)
  
  // Stop writing
  WRITE_DISABLE();
  
  delay(1);  // Small delay for FIFO
  
  // Enable output and reset read pointer
  OUTPUT_ENABLE();
  READ_RESET();
  
  // Send header
  Serial.println("IMG_START");
  Serial.print("WIDTH:");
  Serial.println(IMG_WIDTH);
  Serial.print("HEIGHT:");
  Serial.println(IMG_HEIGHT);
  Serial.println("FORMAT:RGB565");
  Serial.println("DATA_START");
  
  // Read and send all pixels (RGB565 = 2 bytes per pixel)
  uint32_t totalBytes = (uint32_t)IMG_WIDTH * IMG_HEIGHT * 2;
  
  for (uint32_t i = 0; i < totalBytes; i++) {
    uint8_t b = readByte();
    Serial.write(b);
  }
  
  // Disable output
  OUTPUT_DISABLE();
  
  Serial.println();
  Serial.println("DATA_END");
  Serial.println("IMG_END");
}

// Send image as hex string (for debugging)
void sendImageHex() {
  Serial.println("Capturing image (HEX)...");
  
  // Wait for VSYNC
  while (digitalRead(PIN_VSYNC) == HIGH);
  while (digitalRead(PIN_VSYNC) == LOW);
  
  WRITE_RESET();
  WRITE_ENABLE();
  
  while (digitalRead(PIN_VSYNC) == HIGH);
  
  WRITE_DISABLE();
  delay(1);
  
  OUTPUT_ENABLE();
  READ_RESET();
  
  // Send header
  Serial.println("*RDY*");  // Ready marker for Python script
  
  // Read and send all pixels as hex
  for (int y = 0; y < IMG_HEIGHT; y++) {
    for (int x = 0; x < IMG_WIDTH; x++) {
      uint8_t b1 = readByte();
      uint8_t b2 = readByte();
      
      // Convert RGB565 to RGB888
      // OV7670 outputs in little-endian: low byte first, then high byte
      uint16_t rgb565 = (b2 << 8) | b1;
      
      // RGB565 format: RRRRRGGGGGGBBBBB
      uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
      uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
      uint8_t b = (rgb565 & 0x1F) << 3;
      
      // Send as hex bytes: RRGGBB
      if (r < 16) Serial.print("0");
      Serial.print(r, HEX);
      if (g < 16) Serial.print("0");
      Serial.print(g, HEX);
      if (b < 16) Serial.print("0");
      Serial.print(b, HEX);
    }
    Serial.println();  // End of line
  }
  
  OUTPUT_DISABLE();
  Serial.println("*END*");
}

// ==================== SETUP AND LOOP ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port
  }
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("OV7670 + AL422B Camera for Arduino Due");
  Serial.println("========================================");
  
  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000);  // 100kHz for SCCB
  
  // Setup pins
  setupPins();
  delay(100);
  
  // Initialize camera
  if (!initCamera()) {
    Serial.println("Camera initialization failed!");
    while(1) {
      delay(1000);
    }
  }
  
  // Attach VSYNC interrupt
  attachInterrupt(digitalPinToInterrupt(PIN_VSYNC), vsyncHandler, FALLING);
  
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  'c' - Capture and send image (binary)");
  Serial.println("  'h' - Capture and send image (hex - for Python)");
  Serial.println("  'i' - Show camera info");
  Serial.println("  'r' - Reset camera");
  Serial.println("  't' - Test FIFO read");
  Serial.println();
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    
    switch (cmd) {
      case 'c':
      case 'C':
        sendImage();
        break;
        
      case 'h':
      case 'H':
        sendImageHex();
        break;
        
      case 'i':
      case 'I':
        Serial.print("Resolution: ");
        Serial.print(IMG_WIDTH);
        Serial.print("x");
        Serial.println(IMG_HEIGHT);
        Serial.print("Camera PID: 0x");
        Serial.println(readRegister(REG_PID), HEX);
        Serial.print("Camera VER: 0x");
        Serial.println(readRegister(REG_VER), HEX);
        break;
        
      case 'r':
      case 'R':
        Serial.println("Resetting camera...");
        initCamera();
        break;
        
      case 't':
      case 'T':
        // Test FIFO by reading a few bytes
        Serial.println("Testing FIFO...");
        OUTPUT_ENABLE();
        READ_RESET();
        for (int i = 0; i < 16; i++) {
          Serial.print(readByte(), HEX);
          Serial.print(" ");
        }
        Serial.println();
        OUTPUT_DISABLE();
        break;
    }
  }
}

