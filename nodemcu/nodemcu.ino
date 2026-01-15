/*
 * NodeMCU ESP8266 - WiFi Bridge for LLM Car Controller
 * 
 * Действует как мост между Arduino Due и сервером с LLM:
 * 1. Получает JSON данные от Arduino Due через Serial
 * 2. Отправляет данные на сервер через HTTP POST
 * 3. Получает команду от сервера
 * 4. Передает команду обратно на Arduino Due
 * 
 * Подключение к Arduino Due:
 * - NodeMCU RX (GPIO3) ← Arduino Due TX1 (pin 18)
 * - NodeMCU TX (GPIO1) → Arduino Due RX1 (pin 19)
 * - GND → GND
 * 
 * Автор: LLM Car Project
 * Дата: 2026
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// ==================== НАСТРОЙКИ ====================

// WiFi настройки
const char* WIFI_SSID = "TP-Link_461B";
const char* WIFI_PASSWORD = "9152910008";

// Сервер настройки
const char* SERVER_HOST = "192.168.0.93";  // Изменить на IP вашего сервера
const int SERVER_PORT = 8000;
String SERVER_URL = "http://192.168.0.93:8000/command";  // Изменить на IP вашего сервера

// Serial настройки (для связи с Arduino Due)
const int SERIAL_BAUD = 115200;

// Таймауты
const unsigned long HTTP_TIMEOUT = 10000;      // 10 секунд на HTTP запрос
const unsigned long SERIAL_TIMEOUT = 500;      // 500 мс на чтение Serial
const unsigned long WIFI_CHECK_INTERVAL = 5000; // Проверка WiFi каждые 5 секунд

// ==================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================

WiFiClient wifiClient;
String serialBuffer = "";
unsigned long lastWiFiCheck = 0;
bool serverAvailable = false;

// Структура для чанкированной передачи изображения
struct ImageTransfer {
    uint16_t width;
    uint16_t height;
    uint16_t totalChunks;
    uint16_t expectedCrc;
    uint16_t receivedChunks;
    String buffer;              // ~7KB для 80x60 base64
    bool transferInProgress;
    
    void reset() {
        width = 0;
        height = 0;
        totalChunks = 0;
        expectedCrc = 0;
        receivedChunks = 0;
        buffer = "";
        transferInProgress = false;
    }
};

ImageTransfer imageTransfer;

// Буфер для изображения (для совместимости)
String imageBuffer = "";
int imageWidth = 0;
int imageHeight = 0;

// ==================== CRC16 ФУНКЦИЯ ====================

uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

// ==================== SETUP ====================

void setup() {
    // Инициализация Serial
    Serial.begin(SERIAL_BAUD);
    Serial.setRxBufferSize(512);  // Увеличенный буфер для чанков
    delay(100);
    
    // Инициализация структуры передачи
    imageTransfer.reset();
    
    Serial.println();
    Serial.println("================================");
    Serial.println("NodeMCU WiFi Bridge for LLM Car");
    Serial.println("================================");
    
    // Подключение к WiFi
    connectWiFi();
    
    // Проверка сервера
    serverAvailable = checkServer();
    
    Serial.println();
    Serial.println("Ready to receive data from Arduino Due");
    Serial.println();
}

// ==================== MAIN LOOP ====================

void loop() {
    // Проверка WiFi соединения
    checkWiFiConnection();
    
    // Чтение данных от Arduino Due
    processSerialData();
    
    // yield вместо delay для более быстрой обработки Serial
    yield();
}

// ==================== WiFi ФУНКЦИИ ====================

void connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 60) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi connection FAILED!");
        Serial.println("Restarting in 5 seconds...");
        delay(5000);
        ESP.restart();
    }
    
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

void checkWiFiConnection() {
    if (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED) {
            // Переподключаемся без вывода в Serial (мешает протоколу)
            WiFi.mode(WIFI_STA);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            // Ждем максимум 10 секунд
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                attempts++;
            }
        }
    }
}

bool checkServer() {
    Serial.print("Checking server at ");
    Serial.print(SERVER_HOST);
    Serial.print(":");
    Serial.println(SERVER_PORT);
    
    WiFiClient testClient;
    testClient.setTimeout(3000);
    
    if (testClient.connect(SERVER_HOST, SERVER_PORT)) {
        Serial.println("Server is available!");
        testClient.stop();
        return true;
    } else {
        Serial.println("Server not available!");
        testClient.stop();
        return false;
    }
}

// ==================== SERIAL ФУНКЦИИ ====================

void processSerialData() {
    if (!Serial.available()) {
        return;
    }
    
    // Читаем строку от Arduino Due
    String line = readSerialLine();
    if (line.length() == 0) {
        return;
    }
    
    // Обработка разных типов сообщений
    if (line.startsWith("DATA ")) {
        // Получены данные датчиков
        String jsonData = line.substring(5);
        handleSensorData(jsonData);
    }
    else if (line.startsWith("IMG_START ")) {
        // Начало чанкированной передачи изображения
        // Формат: IMG_START width height totalChunks 0xCRC
        handleImageStart(line);
    }
    else if (line.startsWith("IMG_CHUNK ")) {
        // Чанк изображения
        // Формат: IMG_CHUNK idx base64data
        handleImageChunk(line);
    }
    else if (line.startsWith("IMG_END")) {
        // Конец передачи изображения
        handleImageEnd();
    }
    else if (line.startsWith("IMG_ABORT")) {
        // Отмена передачи (без отладочного вывода)
        imageTransfer.reset();
    }
    else if (line.startsWith("IMAGE ")) {
        // Устаревший формат - одним пакетом (для обратной совместимости)
        int firstSpace = line.indexOf(' ', 6);
        int secondSpace = line.indexOf(' ', firstSpace + 1);
        
        if (firstSpace > 0 && secondSpace > 0) {
            imageWidth = line.substring(6, firstSpace).toInt();
            imageHeight = line.substring(firstSpace + 1, secondSpace).toInt();
            imageBuffer = line.substring(secondSpace + 1);
        }
    }
    // Неизвестные сообщения игнорируем молча - любой вывод мешает протоколу
}

void handleImageStart(String line) {
    // Парсим: IMG_START width height totalChunks 0xCRC
    int idx1 = line.indexOf(' ', 10);
    int idx2 = line.indexOf(' ', idx1 + 1);
    int idx3 = line.indexOf(' ', idx2 + 1);
    
    if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
        // Не отправляем debug - мешает протоколу
        return;
    }
    
    imageTransfer.reset();
    imageTransfer.width = line.substring(10, idx1).toInt();
    imageTransfer.height = line.substring(idx1 + 1, idx2).toInt();
    imageTransfer.totalChunks = line.substring(idx2 + 1, idx3).toInt();
    
    // Парсим CRC (формат 0xABCD)
    String crcStr = line.substring(idx3 + 1);
    crcStr.trim();
    if (crcStr.startsWith("0x") || crcStr.startsWith("0X")) {
        imageTransfer.expectedCrc = strtol(crcStr.c_str() + 2, NULL, 16);
    } else {
        imageTransfer.expectedCrc = strtol(crcStr.c_str(), NULL, 16);
    }
    
    imageTransfer.transferInProgress = true;
    imageTransfer.buffer.reserve(imageTransfer.totalChunks * 260);  // Резервируем память
    
    // Отправляем подтверждение готовности
    Serial.println("IMG_READY");
}

void handleImageChunk(String line) {
    if (!imageTransfer.transferInProgress) {
        Serial.println("NAK -1");
        return;
    }
    
    // Парсим: IMG_CHUNK idx base64data
    int spaceIdx = line.indexOf(' ', 10);
    if (spaceIdx < 0) {
        Serial.println("NAK -2");
        return;
    }
    
    uint16_t chunkIdx = line.substring(10, spaceIdx).toInt();
    String chunkData = line.substring(spaceIdx + 1);
    
    // Проверяем порядок чанков
    if (chunkIdx != imageTransfer.receivedChunks) {
        Serial.print("NAK ");
        Serial.println(chunkIdx);
        return;
    }
    
    // Добавляем данные в буфер
    imageTransfer.buffer += chunkData;
    imageTransfer.receivedChunks++;
    
    // Отправляем ACK немедленно
    Serial.print("ACK ");
    Serial.println(chunkIdx);
    Serial.flush();  // Гарантируем отправку
}

void handleImageEnd() {
    if (!imageTransfer.transferInProgress) {
        return;
    }
    
    // Проверяем что получили все чанки
    if (imageTransfer.receivedChunks == imageTransfer.totalChunks) {
        // Копируем в основной буфер для отправки
        imageWidth = imageTransfer.width;
        imageHeight = imageTransfer.height;
        imageBuffer = imageTransfer.buffer;
    }
    
    imageTransfer.reset();
}

String readSerialLine() {
    String result = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < SERIAL_TIMEOUT) {
        if (Serial.available()) {
            char c = Serial.read();
            
            if (c == '\n') {
                result.trim();
                return result;
            } else if (c == '\r') {
                continue;
            } else {
                result += c;
                startTime = millis(); // Сброс таймаута при получении символа
            }
        }
        yield();  // Даем время WiFi стеку, но без delay
    }
    
    result.trim();
    return result;
}

// ==================== HTTP ФУНКЦИИ ====================

void handleSensorData(String jsonData) {
    // Проверяем WiFi
    if (WiFi.status() != WL_CONNECTED) {
        sendDefaultCommand();
        return;
    }
    
    // Если есть изображение, добавляем его в JSON
    if (imageBuffer.length() > 0) {
        // Вставляем данные изображения в JSON перед закрывающей скобкой
        int lastBrace = jsonData.lastIndexOf('}');
        if (lastBrace > 0) {
            // Находим секцию image и добавляем data_base64
            int imageSection = jsonData.indexOf("\"image\":");
            if (imageSection > 0) {
                int imageEnd = jsonData.indexOf('}', imageSection);
                if (imageEnd > 0) {
                    String beforeImage = jsonData.substring(0, imageEnd);
                    String afterImage = jsonData.substring(imageEnd);
                    
                    jsonData = beforeImage + ",\"data_base64\":\"" + imageBuffer + "\"" + afterImage;
                }
            }
        }
        
        // Очищаем буфер изображения
        imageBuffer = "";
    }
    
    // Отправляем HTTP POST запрос (без отладочных сообщений - они мешают протоколу!)
    HTTPClient http;
    http.begin(wifiClient, SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT);
    
    int httpCode = http.POST(jsonData);
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            handleServerResponse(response);
        } else {
            sendDefaultCommand();
        }
    } else {
        sendDefaultCommand();
    }
    
    http.end();
}

void handleServerResponse(String response) {
    // Отправляем команду на Arduino Due
    // Формат: CMD {"command": "FORWARD", "duration_ms": 3000}
    // ВАЖНО: никаких других Serial.print здесь - они мешают протоколу!
    Serial.print("CMD ");
    Serial.println(response);
}

void sendDefaultCommand() {
    // Отправляем STOP если сервер недоступен
    Serial.println("CMD {\"command\":\"STOP\",\"duration_ms\":3000}");
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ====================

void printStatus() {
    Serial.println("=== Status ===");
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.print("Server: ");
    Serial.println(serverAvailable ? "Available" : "Not available");
    Serial.print("Free heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    Serial.println("==============");
}


