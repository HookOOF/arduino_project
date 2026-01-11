#include "../include/Sensors.h"
#include "../include/types.h"
#include <Wire.h>

bool Sensors::begin() {
    mpuInitialized = false;
    
    Serial.println("Sensors: Initializing...");
    
    // Инициализация HC-SR04 через NewPing
    static NewPing sonarInstance(Hardware::TRIG_PIN, Hardware::ECHO_PIN, Hardware::MAX_DISTANCE_CM);
    sonar = &sonarInstance;
    Serial.println("Sensors: HC-SR04 initialized");
    
    // Инициализация ИК-датчика препятствий
    pinMode(Hardware::OBSTACLE_PIN, INPUT);
    Serial.println("Sensors: Obstacle sensor initialized");
    
    // Фоторезистор не требует инициализации
    Serial.println("Sensors: Photoresistor ready");
    
    // Инициализация I2C Wire1 для MPU6050 (SDA1, SCL1)
    Wire1.begin();
    Wire1.setClock(400000);
    
    // Инициализация MPU6050
    if (!mpu.begin(0x68, &Wire1)) {
        Serial.println("Sensors: WARNING - MPU6050 not found!");
        Serial.println("Sensors: Check I2C connections (SDA1=70, SCL1=71)");
    } else {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        mpuInitialized = true;
        Serial.println("Sensors: MPU6050 initialized");
    }
    
    Serial.println("Sensors: All sensors ready");
    return true;
}

SensorSnapshot Sensors::readSnapshot() {
    SensorSnapshot snapshot = {0};
    
    // Чтение HC-SR04
    snapshot.distanceCm = readHCSR04();
    
    // Чтение ИК-датчика препятствий
    snapshot.obstacle = readObstacleSensor();
    
    // Чтение фоторезистора
    snapshot.isDark = readLightSensor(snapshot.lightRaw);
    
    // Чтение MPU6050
    if (mpuInitialized) {
        readMPU6050(snapshot.ax, snapshot.ay, snapshot.az,
                    snapshot.gx, snapshot.gy, snapshot.gz);
    }
    
    return snapshot;
}

bool Sensors::isDark() {
    int lightRaw;
    return readLightSensor(lightRaw);
}

float Sensors::readHCSR04() {
#if HAS_NEWPING
    if (sonar == nullptr) {
        return 400.0f;
    }
    
    unsigned int distance = sonar->ping_cm();
    if (distance == 0) {
        return 400.0f; // Таймаут
    }
    return (float)distance;
#else
    // Fallback без библиотеки
    pinMode(Hardware::TRIG_PIN, OUTPUT);
    pinMode(Hardware::ECHO_PIN, INPUT);
    
    digitalWrite(Hardware::TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(Hardware::TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(Hardware::TRIG_PIN, LOW);
    
    long duration = pulseIn(Hardware::ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
        return 400.0f;
    }
    
    return (duration * 0.0343f) / 2.0f;
#endif
}

bool Sensors::readObstacleSensor() {
    return (digitalRead(Hardware::OBSTACLE_PIN) == HIGH);
}

bool Sensors::readLightSensor(int& lightRaw) {
    lightRaw = analogRead(Hardware::LIGHT_PIN);
    return (lightRaw < Hardware::LIGHT_THRESHOLD);
}

bool Sensors::readMPU6050(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
    if (!mpuInitialized) {
        ax = ay = az = gx = gy = gz = 0.0f;
        return false;
    }
    
    sensors_event_t accel, gyro, temp;
    if (!mpu.getEvent(&accel, &gyro, &temp)) {
        return false;
    }
    
    ax = accel.acceleration.x;
    ay = accel.acceleration.y;
    az = accel.acceleration.z;
    gx = gyro.gyro.x;
    gy = gyro.gyro.y;
    gz = gyro.gyro.z;
    
    return true;
}

