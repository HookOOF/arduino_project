# LLM Car Controller

Система управления моделью автомобиля на базе Arduino DUE с интеграцией языковой модели (OpenAI API).

## Архитектура

```
┌─────────────────────┐     Serial1      ┌──────────────────┐      HTTP       ┌─────────────────┐
│     Arduino DUE     │ ◄──────────────► │  NodeMCU ESP8266 │ ◄─────────────► │  Python Server  │
│                     │    (9600 baud)   │   (WiFi Bridge)  │   (JSON/REST)   │   (FastAPI)     │
│  • Датчики          │                  │                  │                 │                 │
│  • Камера OV7670    │                  │  • WiFi Client   │                 │  • OpenAI API   │
│  • Моторы           │                  │  • HTTP POST     │                 │  • Логика LLM   │
│  • Flash storage    │                  │  • Serial Bridge │                 │  • История      │
└─────────────────────┘                  └──────────────────┘                 └─────────────────┘
```

## Компоненты

### Arduino DUE

Основной микроконтроллер, который:
- Собирает данные с датчиков (HC-SR04, MPU6050, фоторезистор)
- Захватывает изображения с камеры OV7670 (опционально)
- Хранит справочник команд в энергонезависимой памяти (DueFlashStorage)
- Ведёт журнал выполненных команд
- Управляет моторами

### NodeMCU ESP8266

WiFi мост между Arduino DUE и сервером:
- Получает JSON данные от Arduino через Serial
- Отправляет данные на сервер через HTTP POST
- Получает команды от сервера
- Передаёт команды обратно на Arduino

### Python Server

FastAPI сервер с интеграцией OpenAI:
- Принимает данные с датчиков
- Формирует промпт для языковой модели
- Отправляет запрос в OpenAI API
- Возвращает команду управления

## Оборудование

### Arduino DUE

| Компонент | Пины |
|-----------|------|
| HC-SR04 (ультразвук) | TRIG=8, ECHO=9 |
| MPU6050 (I2C Wire1) | SDA1=70, SCL1=71 |
| Фоторезистор | A0 |
| Моторы (левый) | IN1=6, IN2=7 |
| Моторы (правый) | IN3=4, IN4=5 |
| NodeMCU Serial1 | TX1=18, RX1=19 |

### OV7670 + AL422B FIFO Camera (опционально)

| Пин камеры | Arduino DUE |
|------------|-------------|
| VSYNC | 40 |
| RST | 22 |
| WR | 38 |
| WRST | 37 |
| RRST | 35 |
| OE | 39 |
| RCK | 36 |
| D0-D7 | 51-44 |
| SIO_C | SCL (21) |
| SIO_D | SDA (20) |

### NodeMCU ESP8266

| NodeMCU | Arduino DUE |
|---------|-------------|
| GPIO3 (RX) | TX1 (18) |
| GPIO1 (TX) | RX1 (19) |
| GND | GND |

## Установка

### 1. Arduino DUE

1. Установите Arduino IDE
2. Установите библиотеки:
   - DueFlashStorage
   - Adafruit MPU6050
   - Adafruit Unified Sensor
   - NewPing
   - ArduinoJson (опционально)
3. Откройте `arduino_due/arduino_due.ino`
4. Выберите плату: Arduino Due (Programming Port)
5. Загрузите скетч

### 2. NodeMCU ESP8266

1. Установите поддержку ESP8266 в Arduino IDE
2. Откройте `nodemcu/nodemcu.ino`
3. Измените настройки WiFi и IP сервера:
   ```cpp
   const char* WIFI_SSID = "your_wifi";
   const char* WIFI_PASSWORD = "your_password";
   const char* SERVER_HOST = "192.168.1.100";
   ```
4. Выберите плату: NodeMCU 1.0 (ESP-12E Module)
5. Загрузите скетч

### 3. Python Server

```bash
cd server

# Создайте виртуальное окружение
python -m venv venv
venv\Scripts\activate  # Windows
# или
source venv/bin/activate  # Linux/Mac

# Установите зависимости
pip install -r requirements.txt

# Для OpenRouter (бесплатные модели доступны):
set OPENROUTER_API_KEY=sk-or-v1-xxx  # Windows
# или
export OPENROUTER_API_KEY=sk-or-v1-xxx  # Linux/Mac

# Опционально: выбор модели
set OPENAI_MODEL=google/gemini-2.0-flash-exp:free

# Для OpenAI (альтернатива):
# set OPENAI_API_KEY=sk-xxx
# set OPENAI_BASE_URL=https://api.openai.com/v1
# set OPENAI_MODEL=gpt-4o-mini

# Запустите сервер
python main.py
```

Или с Docker:
```bash
cd server
# Установите переменные в .env или передайте напрямую:
set OPENROUTER_API_KEY=sk-or-v1-xxx
docker-compose up --build
```

## Протокол обмена данными

### Arduino → NodeMCU (Serial1)

```
DATA {"session_id":1,"step":42,"timestamp":"11:01:2026 15:30:00","sensors":{"distance_cm":123.5,"light_raw":512,"light_dark":false,"mpu6050":{"ax":0.12,"ay":-0.03,"az":9.81,"gx":0.01,"gy":0.00,"gz":-0.02}},"image":{"available":true,"width":80,"height":60,"format":"GRAY8"}}
```

### NodeMCU → Server (HTTP POST)

POST `/command` с тем же JSON.

### Server → NodeMCU (HTTP Response)

```json
{"command": "FORWARD", "duration_ms": 3000}
```

### NodeMCU → Arduino (Serial1)

```
CMD {"command":"FORWARD","duration_ms":3000}
```

## Команды

| Команда | Описание |
|---------|----------|
| FORWARD | Движение вперёд |
| BACKWARD | Движение назад |
| LEFT | Поворот влево (на месте) |
| RIGHT | Поворот вправо (на месте) |
| STOP | Остановка |

## Serial Monitor команды (Arduino DUE)

| Команда | Описание |
|---------|----------|
| `help` | Показать справку |
| `status` | Статус системы |
| `log` | Показать журнал команд |
| `log clear` | Очистить журнал |
| `dict` | Показать справочник команд |
| `serial on/off` | Включить/выключить логирование |
| `time dd:MM:yyyy hh:mm:ss` | Установить время |
| `duration <ms>` | Установить длительность шага |

## API Endpoints

| Метод | URL | Описание |
|-------|-----|----------|
| GET | `/` | Статус сервера |
| GET | `/health` | Проверка здоровья |
| POST | `/command` | Получить команду (основной) |
| POST | `/data` | Альтернативный endpoint |
| GET | `/history` | История команд |
| DELETE | `/history` | Очистить историю команд |
| GET | `/metrics` | История метрик датчиков |
| GET | `/metrics/latest` | Последние метрики |
| GET | `/metrics/stats` | Статистика по метрикам |
| DELETE | `/metrics` | Очистить историю метрик |
| GET | `/config` | Конфигурация сервера |

## Режимы работы

### LLM Mode (OpenRouter / OpenAI)

При наличии API ключа сервер использует языковую модель для принятия решений.

**OpenRouter** (рекомендуется, есть бесплатные модели):
- `OPENROUTER_API_KEY` - ваш ключ от openrouter.ai
- Бесплатные модели: `google/gemini-2.0-flash-exp:free`, `meta-llama/llama-3.1-8b-instruct:free`

**OpenAI**:
- `OPENAI_API_KEY` - ваш ключ от OpenAI
- `OPENAI_BASE_URL=https://api.openai.com/v1`

### Demo Mode

Без API ключа сервер работает в демо-режиме с простой логикой:
- Расстояние < 20 см → BACKWARD
- Расстояние < 50 см → LEFT/RIGHT
- Темно → осторожно FORWARD
- Путь свободен → FORWARD

## Лицензия

MIT


