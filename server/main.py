"""
LLM Car Controller Server

FastAPI сервер для управления моделью автомобиля с использованием OpenAI API.
Получает данные с датчиков от Arduino через NodeMCU, формирует промпт для LLM,
и возвращает команду управления.

Автор: LLM Car Project
Дата: 2026
"""

import os
import base64
import json
import logging
from datetime import datetime
from typing import Optional, List, Dict, Any
from io import BytesIO

from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn

# OpenAI
from openai import OpenAI

# Настройка логирования
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# ==================== КОНФИГУРАЦИЯ ====================

# API ключ (OpenAI или OpenRouter)
API_KEY = os.getenv("OPENAI_API_KEY", "") or os.getenv("OPENROUTER_API_KEY", "")

# Base URL для API (OpenRouter или OpenAI)
# OpenRouter: https://openrouter.ai/api/v1
# OpenAI: https://api.openai.com/v1 (по умолчанию)
API_BASE_URL = os.getenv("OPENAI_BASE_URL", "https://openrouter.ai/api/v1")

# Модель для использования
# OpenRouter модели: google/gemini-2.0-flash-exp:free, meta-llama/llama-3.1-8b-instruct:free, 
#                    openai/gpt-4o-mini, anthropic/claude-3.5-sonnet, и др.
# OpenAI модели: gpt-4o-mini, gpt-4o, gpt-3.5-turbo
API_MODEL = os.getenv("OPENAI_MODEL", "google/gemini-2.0-flash-exp:free")

# Доступные команды
AVAILABLE_COMMANDS = ["FORWARD", "BACKWARD", "LEFT", "RIGHT", "STOP"]

# Базовая длительность команды (мс)
DEFAULT_DURATION_MS = 3000

# Режим работы без API (для тестирования)
DEMO_MODE = not API_KEY

# ==================== PYDANTIC МОДЕЛИ ====================

class MPU6050Data(BaseModel):
    ax: float = 0.0
    ay: float = 0.0
    az: float = 0.0
    gx: float = 0.0
    gy: float = 0.0
    gz: float = 0.0

class SensorData(BaseModel):
    distance_cm: float = 400.0
    light_raw: int = 500
    light_dark: bool = False
    mpu6050: Optional[MPU6050Data] = None

class ImageData(BaseModel):
    available: bool = False
    width: int = 0
    height: int = 0
    format: str = "GRAY8"
    data_base64: Optional[str] = None

class CarDataRequest(BaseModel):
    session_id: int = 1
    step: int = 1
    timestamp: str = ""
    sensors: SensorData
    image: Optional[ImageData] = None

class CommandResponse(BaseModel):
    command: str
    duration_ms: int

# ==================== FASTAPI ПРИЛОЖЕНИЕ ====================

app = FastAPI(
    title="LLM Car Controller",
    description="Сервер управления моделью автомобиля с использованием LLM",
    version="1.0.0"
)

# CORS для доступа из любых источников
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# OpenAI-совместимый клиент (работает с OpenAI и OpenRouter)
openai_client = None
if API_KEY:
    openai_client = OpenAI(
        api_key=API_KEY,
        base_url=API_BASE_URL
    )
    logger.info(f"API client initialized: {API_BASE_URL}")
    logger.info(f"Model: {API_MODEL}")
else:
    logger.warning("API key not set, running in DEMO mode")

# История команд для сессии
command_history: List[Dict[str, Any]] = []

# История метрик датчиков
metrics_history: List[Dict[str, Any]] = []
MAX_METRICS_HISTORY = 1000  # Максимум записей

# ==================== ФОРМИРОВАНИЕ ПРОМПТА ====================

SYSTEM_PROMPT = """You are an AI controller for a small autonomous car. 
Your task is to analyze sensor data and decide the next movement command.

Available commands:
- FORWARD: Move forward
- BACKWARD: Move backward  
- LEFT: Turn left (in place)
- RIGHT: Turn right (in place)
- STOP: Stop moving

You must respond with ONLY a valid JSON object in this format:
{"command": "COMMAND_NAME", "duration_ms": 3000}

Rules for decision making:
1. If distance < 20cm, consider stopping or turning
2. If distance < 50cm, slow down or prepare to turn
3. If the path is clear (distance > 100cm), move forward
4. If in darkness (light_dark=true), be more cautious
5. Use MPU6050 data to detect if the car is tilted or unstable

Always prioritize safety - when in doubt, STOP."""


def build_user_prompt(data: CarDataRequest) -> str:
    """Формирование промпта из данных датчиков"""
    
    prompt_parts = [
        f"Step {data.step} | Session {data.session_id}",
        f"Time: {data.timestamp}",
        "",
        "=== SENSOR DATA ===",
        f"Distance to obstacle: {data.sensors.distance_cm:.1f} cm",
        f"Light level: {data.sensors.light_raw} (Dark: {'YES' if data.sensors.light_dark else 'NO'})",
    ]
    
    if data.sensors.mpu6050:
        mpu = data.sensors.mpu6050
        prompt_parts.extend([
            "",
            "=== MPU6050 (Accelerometer/Gyroscope) ===",
            f"Acceleration: X={mpu.ax:.2f}, Y={mpu.ay:.2f}, Z={mpu.az:.2f} m/s²",
            f"Gyroscope: X={mpu.gx:.2f}, Y={mpu.gy:.2f}, Z={mpu.gz:.2f} rad/s",
        ])
    
    # Добавляем информацию об изображении
    if data.image and data.image.available:
        prompt_parts.extend([
            "",
            f"=== CAMERA IMAGE ({data.image.width}x{data.image.height}) ===",
            "Image is available for analysis",
        ])
    
    # Добавляем историю последних команд
    if command_history:
        recent = command_history[-5:]  # Последние 5 команд
        prompt_parts.extend([
            "",
            "=== RECENT COMMANDS ===",
        ])
        for cmd in recent:
            prompt_parts.append(f"Step {cmd['step']}: {cmd['command']} ({cmd['duration_ms']}ms)")
    
    prompt_parts.extend([
        "",
        "Based on this sensor data, what should the car do next?",
        "Respond with a JSON command."
    ])
    
    return "\n".join(prompt_parts)


def decode_image_for_vision(image_data: ImageData) -> Optional[str]:
    """Декодирование изображения для OpenAI Vision API"""
    if not image_data or not image_data.available or not image_data.data_base64:
        return None
    
    try:
        # Декодируем base64
        image_bytes = base64.b64decode(image_data.data_base64)
        
        # Для grayscale изображения конвертируем в PNG
        # (OpenAI Vision требует стандартные форматы)
        try:
            from PIL import Image
            import io
            
            # Создаем изображение из raw bytes
            img = Image.frombytes('L', (image_data.width, image_data.height), image_bytes)
            
            # Конвертируем в PNG
            buffer = io.BytesIO()
            img.save(buffer, format='PNG')
            png_bytes = buffer.getvalue()
            
            # Возвращаем как data URL
            return f"data:image/png;base64,{base64.b64encode(png_bytes).decode()}"
        except ImportError:
            logger.warning("PIL not installed, cannot process image for vision")
            return None
            
    except Exception as e:
        logger.error(f"Error decoding image: {e}")
        return None


async def get_llm_command(data: CarDataRequest) -> CommandResponse:
    """Получение команды от LLM"""
    
    if DEMO_MODE or not openai_client:
        # Демо режим - простая логика без LLM
        return get_demo_command(data)
    
    try:
        user_prompt = build_user_prompt(data)
        
        messages = [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": user_prompt}
        ]
        
        # Если есть изображение, используем Vision API
        image_url = decode_image_for_vision(data.image) if data.image else None
        
        if image_url and ("gpt-4" in API_MODEL or "claude" in API_MODEL or "gemini" in API_MODEL):
            # Vision запрос
            messages = [
                {"role": "system", "content": SYSTEM_PROMPT},
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": user_prompt},
                        {"type": "image_url", "image_url": {"url": image_url}}
                    ]
                }
            ]
        
        # Запрос к API (OpenAI или OpenRouter)
        response = openai_client.chat.completions.create(
            model=API_MODEL,
            messages=messages,
            max_tokens=100,
            temperature=0.3,
        )
        
        # Парсинг ответа
        content = response.choices[0].message.content.strip()
        logger.info(f"LLM response: {content}")
        
        # Извлекаем JSON из ответа
        try:
            # Ищем JSON в ответе
            start = content.find('{')
            end = content.rfind('}') + 1
            if start >= 0 and end > start:
                json_str = content[start:end]
                result = json.loads(json_str)
                
                command = result.get("command", "STOP").upper()
                duration = result.get("duration_ms", DEFAULT_DURATION_MS)
                
                # Валидация команды
                if command not in AVAILABLE_COMMANDS:
                    logger.warning(f"Invalid command from LLM: {command}, using STOP")
                    command = "STOP"
                
                return CommandResponse(command=command, duration_ms=duration)
        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse LLM JSON response: {e}")
        
        # Если не удалось распарсить, используем STOP
        return CommandResponse(command="STOP", duration_ms=DEFAULT_DURATION_MS)
        
    except Exception as e:
        logger.error(f"OpenAI API error: {e}")
        return CommandResponse(command="STOP", duration_ms=DEFAULT_DURATION_MS)


def get_demo_command(data: CarDataRequest) -> CommandResponse:
    """Демо логика без LLM"""
    
    sensors = data.sensors
    
    # Простая логика принятия решений на основе расстояния
    if sensors.distance_cm < 20:
        # Очень близко - отъезжаем назад
        return CommandResponse(command="BACKWARD", duration_ms=1000)
    
    if sensors.distance_cm < 50:
        # Близко - поворачиваем
        # Чередуем направление на основе номера шага
        if data.step % 2 == 0:
            return CommandResponse(command="LEFT", duration_ms=1500)
        else:
            return CommandResponse(command="RIGHT", duration_ms=1500)
    
    if sensors.light_dark:
        # Темно - осторожно вперед
        return CommandResponse(command="FORWARD", duration_ms=1000)
    
    # Путь свободен - едем вперед
    return CommandResponse(command="FORWARD", duration_ms=DEFAULT_DURATION_MS)


# ==================== ENDPOINTS ====================

@app.get("/")
async def root():
    """Главная страница"""
    return {
        "service": "LLM Car Controller",
        "status": "running",
        "mode": "DEMO" if DEMO_MODE else "LLM",
        "api_base": API_BASE_URL if not DEMO_MODE else "N/A",
        "model": API_MODEL if not DEMO_MODE else "N/A",
        "time": datetime.now().isoformat(),
        "commands_processed": len(command_history),
        "metrics_stored": len(metrics_history)
    }


@app.get("/health")
async def health():
    """Проверка здоровья сервиса"""
    return {"status": "healthy"}


@app.post("/command", response_model=CommandResponse)
async def get_command(request: Request):
    """
    Основной endpoint для получения команды
    Принимает данные с датчиков, возвращает команду для автомобиля
    """
    try:
        # Парсинг входных данных
        body = await request.json()
        data = CarDataRequest(**body)
        
        logger.info(f"Received data: session={data.session_id}, step={data.step}")
        logger.info(f"Sensors: dist={data.sensors.distance_cm:.1f}cm, dark={data.sensors.light_dark}")
        
        # Сохраняем метрики датчиков
        metrics_entry = {
            "session_id": data.session_id,
            "step": data.step,
            "timestamp": data.timestamp,
            "received_at": datetime.now().isoformat(),
            "sensors": {
                "distance_cm": data.sensors.distance_cm,
                "light_raw": data.sensors.light_raw,
                "light_dark": data.sensors.light_dark,
            },
            "image_available": data.image.available if data.image else False
        }
        
        # Добавляем данные MPU6050 если есть
        if data.sensors.mpu6050:
            metrics_entry["sensors"]["mpu6050"] = {
                "ax": data.sensors.mpu6050.ax,
                "ay": data.sensors.mpu6050.ay,
                "az": data.sensors.mpu6050.az,
                "gx": data.sensors.mpu6050.gx,
                "gy": data.sensors.mpu6050.gy,
                "gz": data.sensors.mpu6050.gz,
            }
        
        metrics_history.append(metrics_entry)
        
        # Ограничиваем размер истории
        if len(metrics_history) > MAX_METRICS_HISTORY:
            metrics_history.pop(0)
        
        # Получаем команду от LLM
        response = await get_llm_command(data)
        
        # Сохраняем в историю
        command_history.append({
            "step": data.step,
            "command": response.command,
            "duration_ms": response.duration_ms,
            "timestamp": datetime.now().isoformat()
        })
        
        # Ограничиваем историю
        if len(command_history) > 1000:
            command_history.pop(0)
        
        logger.info(f"Response: {response.command} for {response.duration_ms}ms")
        
        return response
        
    except Exception as e:
        logger.error(f"Error processing request: {e}")
        return CommandResponse(command="STOP", duration_ms=DEFAULT_DURATION_MS)


@app.post("/data")
async def receive_data(request: Request):
    """
    Альтернативный endpoint для совместимости
    """
    return await get_command(request)


@app.get("/history")
async def get_history():
    """Получение истории команд"""
    return {
        "total": len(command_history),
        "commands": command_history[-100:]  # Последние 100
    }


@app.delete("/history")
async def clear_history():
    """Очистка истории команд"""
    command_history.clear()
    return {"status": "cleared"}


@app.get("/metrics")
async def get_metrics(limit: int = 100):
    """Получение истории метрик датчиков"""
    return {
        "total": len(metrics_history),
        "metrics": metrics_history[-limit:]
    }


@app.get("/metrics/latest")
async def get_latest_metrics():
    """Получение последних метрик"""
    if not metrics_history:
        return {"error": "No metrics available"}
    return metrics_history[-1]


@app.get("/metrics/stats")
async def get_metrics_stats():
    """Статистика по метрикам"""
    if not metrics_history:
        return {"error": "No metrics available"}
    
    distances = [m["sensors"]["distance_cm"] for m in metrics_history]
    light_values = [m["sensors"]["light_raw"] for m in metrics_history]
    dark_count = sum(1 for m in metrics_history if m["sensors"]["light_dark"])
    image_count = sum(1 for m in metrics_history if m.get("image_available", False))
    
    return {
        "total_records": len(metrics_history),
        "distance": {
            "min": min(distances),
            "max": max(distances),
            "avg": sum(distances) / len(distances)
        },
        "light": {
            "min": min(light_values),
            "max": max(light_values),
            "avg": sum(light_values) / len(light_values),
            "dark_percentage": (dark_count / len(metrics_history)) * 100
        },
        "images_captured": image_count,
        "first_record": metrics_history[0]["received_at"],
        "last_record": metrics_history[-1]["received_at"]
    }


@app.delete("/metrics")
async def clear_metrics():
    """Очистка истории метрик"""
    metrics_history.clear()
    return {"status": "cleared"}


@app.get("/config")
async def get_config():
    """Получение конфигурации сервера"""
    return {
        "mode": "DEMO" if DEMO_MODE else "LLM",
        "api_base": API_BASE_URL,
        "model": API_MODEL,
        "available_commands": AVAILABLE_COMMANDS,
        "default_duration_ms": DEFAULT_DURATION_MS
    }


# ==================== ЗАПУСК ====================

if __name__ == "__main__":
    print("\n" + "=" * 50)
    print("  LLM Car Controller Server")
    print("=" * 50)
    print(f"  Mode: {'DEMO' if DEMO_MODE else 'LLM'}")
    if not DEMO_MODE:
        print(f"  API: {API_BASE_URL}")
        print(f"  Model: {API_MODEL}")
    print(f"  Server: http://0.0.0.0:8000")
    print("=" * 50 + "\n")
    
    if DEMO_MODE:
        print("  DEMO MODE: Set OPENROUTER_API_KEY or OPENAI_API_KEY")
        print("  Example: set OPENROUTER_API_KEY=sk-or-v1-xxx")
        print()
    
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        log_level="info"
    )


