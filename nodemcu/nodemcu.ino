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
const char* WIFI_SSID = "ard";
const char* WIFI_PASSWORD = "54321987";

// Сервер настройки
const char* SERVER_HOST = "10.223.177.203";  
const int SERVER_PORT = 8000;
String SERVER_URL = "http://10.223.177.203:8000/command";
String SERVER_IMG_START_URL = "http://10.223.177.203:8000/image/start";
String SERVER_IMG_CHUNK_URL = "http://10.223.177.203:8000/image/chunk";
String SERVER_IMG_END_URL   = "http://10.223.177.203:8000/image/end";

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
    String imageId;          // image_id от сервера
    bool transferInProgress;
    
    void reset() {
        width = 0;
        height = 0;
        totalChunks = 0;
        expectedCrc = 0;
        receivedChunks = 0;
        imageId = "";
        transferInProgress = false;
    }
};

ImageTransfer imageTransfer;

// image_id последнего успешно загруженного изображения (для вставки в DATA)
String currentImageId = "";

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
    // Устаревший формат IMAGE больше не поддерживается
    // Неизвестные сообщения игнорируем молча - любой вывод мешает протоколу
}

void handleImageStart(String line) {
    // Парсим: IMG_START width height totalChunks 0xCRC
    int idx1 = line.indexOf(' ', 10);
    int idx2 = line.indexOf(' ', idx1 + 1);
    int idx3 = line.indexOf(' ', idx2 + 1);
    
    if (idx1 < 0 || idx2 < 0 || idx3 < 0) {
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
    
    // Отправляем POST /image/start на сервер и получаем image_id
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    HTTPClient http;
    http.begin(wifiClient, SERVER_IMG_START_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);
    
    String body = "{\"session_id\":0,\"step\":0,\"width\":";
    body += imageTransfer.width;
    body += ",\"height\":";
    body += imageTransfer.height;
    body += ",\"total_chunks\":";
    body += imageTransfer.totalChunks;
    body += ",\"crc\":\"0x";
    body += String(imageTransfer.expectedCrc, HEX);
    body += "\"}";
    
    int httpCode = http.POST(body);
    
    if (httpCode == 200) {
        String response = http.getString();
        // Парсим image_id из {"image_id":"..."}
        int idStart = response.indexOf("\"image_id\":\"");
        if (idStart >= 0) {
            idStart += 12;
            int idEnd = response.indexOf('"', idStart);
            if (idEnd > idStart) {
                imageTransfer.imageId = response.substring(idStart, idEnd);
            }
        }
    }
    
    http.end();
    
    if (imageTransfer.imageId.length() == 0) {
        imageTransfer.reset();
        return;
    }
    
    imageTransfer.transferInProgress = true;
    
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
    
    imageTransfer.receivedChunks++;
    
    // Отправляем ACK немедленно (Arduino ждёт его)
    Serial.print("ACK ");
    Serial.println(chunkIdx);
    Serial.flush();
    
    // Отправляем чанк на сервер по HTTP (не храним в памяти)
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(wifiClient, SERVER_IMG_CHUNK_URL);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        String body = "{\"image_id\":\"";
        body += imageTransfer.imageId;
        body += "\",\"chunk_idx\":";
        body += chunkIdx;
        body += ",\"data\":\"";
        body += chunkData;
        body += "\"}";
        
        http.POST(body);
        http.end();
    }
}

void handleImageEnd() {
    if (!imageTransfer.transferInProgress) {
        return;
    }
    
    // Проверяем что получили все чанки
    if (imageTransfer.receivedChunks == imageTransfer.totalChunks) {
        // Отправляем POST /image/end на сервер
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(wifiClient, SERVER_IMG_END_URL);
            http.addHeader("Content-Type", "application/json");
            http.setTimeout(10000);
            
            String body = "{\"image_id\":\"";
            body += imageTransfer.imageId;
            body += "\"}";
            
            int httpCode = http.POST(body);
            
            if (httpCode == 200) {
                // Сохраняем image_id для вставки в следующий DATA запрос
                currentImageId = imageTransfer.imageId;
            }
            
            http.end();
        }
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
    
    // Если есть загруженное изображение, вставляем image_id в секцию image
    if (currentImageId.length() > 0) {
        int imageSection = jsonData.indexOf("\"image\":");
        if (imageSection > 0) {
            int imageEnd = jsonData.indexOf('}', imageSection);
            if (imageEnd > 0) {
                String beforeEnd = jsonData.substring(0, imageEnd);
                String afterEnd = jsonData.substring(imageEnd);
                
                jsonData = beforeEnd + ",\"image_id\":\"" + currentImageId + "\"" + afterEnd;
            }
        }
        
        // Очищаем после использования
        currentImageId = "";
    }
    
    // Отправляем HTTP POST запрос
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


