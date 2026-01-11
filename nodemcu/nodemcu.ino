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
const char* SERVER_HOST = "192.168.1.100";  // Изменить на IP вашего сервера
const int SERVER_PORT = 8000;
String SERVER_URL = "http://192.168.1.100:8000/command";  // Изменить на IP вашего сервера

// Serial настройки (для связи с Arduino Due)
const int SERIAL_BAUD = 9600;

// Таймауты
const unsigned long HTTP_TIMEOUT = 10000;      // 10 секунд на HTTP запрос
const unsigned long SERIAL_TIMEOUT = 500;      // 500 мс на чтение Serial
const unsigned long WIFI_CHECK_INTERVAL = 5000; // Проверка WiFi каждые 5 секунд

// ==================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================

WiFiClient wifiClient;
String serialBuffer = "";
unsigned long lastWiFiCheck = 0;
bool serverAvailable = false;

// Буфер для изображения
String imageBuffer = "";
bool receivingImage = false;
int imageWidth = 0;
int imageHeight = 0;

// ==================== SETUP ====================

void setup() {
    // Инициализация Serial
    Serial.begin(SERIAL_BAUD);
    delay(100);
    
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
    
    // Небольшая задержка для стабильности
    delay(10);
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
            Serial.println("WiFi disconnected! Reconnecting...");
            connectWiFi();
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
    else if (line.startsWith("IMAGE ")) {
        // Начало изображения
        // Формат: IMAGE width height base64data
        int firstSpace = line.indexOf(' ', 6);
        int secondSpace = line.indexOf(' ', firstSpace + 1);
        
        if (firstSpace > 0 && secondSpace > 0) {
            imageWidth = line.substring(6, firstSpace).toInt();
            imageHeight = line.substring(firstSpace + 1, secondSpace).toInt();
            imageBuffer = line.substring(secondSpace + 1);
            
            Serial.print("Received image: ");
            Serial.print(imageWidth);
            Serial.print("x");
            Serial.print(imageHeight);
            Serial.print(", ");
            Serial.print(imageBuffer.length());
            Serial.println(" bytes base64");
        }
    }
    else {
        // Неизвестное сообщение
        Serial.print("Unknown message: ");
        Serial.println(line.substring(0, 50));
    }
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
        delay(1);
    }
    
    result.trim();
    return result;
}

// ==================== HTTP ФУНКЦИИ ====================

void handleSensorData(String jsonData) {
    // Проверяем WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected, cannot send data");
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
    
    Serial.println("Sending data to server...");
    
    // Отправляем HTTP POST запрос
    HTTPClient http;
    http.begin(wifiClient, SERVER_URL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT);
    
    int httpCode = http.POST(jsonData);
    
    if (httpCode > 0) {
        Serial.print("HTTP Response: ");
        Serial.println(httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            handleServerResponse(response);
        } else {
            Serial.print("HTTP Error: ");
            Serial.println(httpCode);
            sendDefaultCommand();
        }
    } else {
        Serial.print("HTTP Request failed: ");
        Serial.println(http.errorToString(httpCode));
        sendDefaultCommand();
    }
    
    http.end();
}

void handleServerResponse(String response) {
    Serial.print("Server response: ");
    Serial.println(response);
    
    // Отправляем команду на Arduino Due
    // Формат: CMD {"command": "FORWARD", "duration_ms": 3000}
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

