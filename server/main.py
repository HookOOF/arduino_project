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
import time
from datetime import datetime
from typing import Optional, List, Dict, Any
from io import BytesIO
from pathlib import Path

from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import JSONResponse, FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn

# OpenAI
from openai import OpenAI

# Директория для сохранения изображений
IMAGES_DIR = Path("images")
IMAGES_DIR.mkdir(exist_ok=True)
MAX_SAVED_IMAGES = 500  # Максимум сохранённых изображений

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

# Список сохранённых изображений
saved_images: List[Dict[str, Any]] = []

# История LLM взаимодействий (промпт → ответ)
llm_log: List[Dict[str, Any]] = []
MAX_LLM_LOG = 500

# Алерты (падение и др.)
alerts: List[Dict[str, Any]] = []
MAX_ALERTS = 200

# Пороги детекции падения по MPU6050
# Нормально: az ≈ 9.8 (гравитация вниз), ax ≈ 0, ay ≈ 0
FALL_THRESHOLDS = {
    "az_min": 5.0,     # Если |az| < 5 — машина не вертикально
    "ax_max": 7.0,     # Если |ax| > 7 — машина на боку
    "ay_max": 7.0,     # Если |ay| > 7 — машина на боку
    "gyro_max": 5.0,   # Если любая ось гироскопа > 5 рад/с — вращение
}

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

# Текущий системный промпт (изменяемый в рантайме)
current_system_prompt: str = SYSTEM_PROMPT


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


def save_image(image_data: ImageData, session_id: int, step: int) -> Optional[Dict[str, Any]]:
    """Сохранение изображения на диск"""
    global saved_images
    
    if not image_data or not image_data.data_base64:
        return None
    
    try:
        # Декодируем base64
        image_bytes = base64.b64decode(image_data.data_base64)
        
        # Генерируем имя файла
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"session{session_id}_step{step}_{timestamp}.png"
        filepath = IMAGES_DIR / filename
        
        # Конвертируем grayscale в PNG
        try:
            from PIL import Image
            
            # Создаём изображение из raw bytes (grayscale)
            img = Image.frombytes('L', (image_data.width, image_data.height), image_bytes)
            img.save(filepath, format='PNG')
            
            logger.info(f"Image saved: {filename}")
            
            # Добавляем в список
            image_info = {
                "filename": filename,
                "path": str(filepath),
                "session_id": session_id,
                "step": step,
                "width": image_data.width,
                "height": image_data.height,
                "timestamp": datetime.now().isoformat(),
                "size_bytes": len(image_bytes)
            }
            saved_images.append(image_info)
            
            # Удаляем старые изображения если превышен лимит
            while len(saved_images) > MAX_SAVED_IMAGES:
                old_image = saved_images.pop(0)
                old_path = Path(old_image["path"])
                if old_path.exists():
                    old_path.unlink()
                    logger.info(f"Deleted old image: {old_image['filename']}")
            
            return image_info
            
        except ImportError:
            # Если PIL не установлен, сохраняем raw данные
            filepath = IMAGES_DIR / f"session{session_id}_step{step}_{timestamp}.raw"
            with open(filepath, 'wb') as f:
                f.write(image_bytes)
            logger.warning("PIL not installed, saved raw image data")
            return {"filename": filepath.name, "path": str(filepath)}
            
    except Exception as e:
        logger.error(f"Error saving image: {e}")
        return None


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
    global llm_log
    
    if DEMO_MODE or not openai_client:
        # Демо режим - простая логика без LLM
        result = get_demo_command(data)
        # Логируем даже демо-команды
        llm_log.append({
            "timestamp": datetime.now().isoformat(),
            "session_id": data.session_id,
            "step": data.step,
            "mode": "DEMO",
            "system_prompt": None,
            "user_prompt": build_user_prompt(data),
            "raw_response": None,
            "parsed_command": result.command,
            "parsed_duration_ms": result.duration_ms,
            "latency_ms": 0,
            "error": None,
            "image_sent": False,
        })
        if len(llm_log) > MAX_LLM_LOG:
            llm_log.pop(0)
        return result
    
    t_start = time.time()
    log_entry: Dict[str, Any] = {
        "timestamp": datetime.now().isoformat(),
        "session_id": data.session_id,
        "step": data.step,
        "mode": "LLM",
        "system_prompt": current_system_prompt,
        "user_prompt": None,
        "raw_response": None,
        "parsed_command": None,
        "parsed_duration_ms": None,
        "latency_ms": None,
        "error": None,
        "image_sent": False,
        "model": API_MODEL,
        "tokens_prompt": None,
        "tokens_completion": None,
    }
    
    try:
        user_prompt = build_user_prompt(data)
        log_entry["user_prompt"] = user_prompt
        
        messages = [
            {"role": "system", "content": current_system_prompt},
            {"role": "user", "content": user_prompt}
        ]
        
        # Проверяем доступность изображения: imageAvailable == true И есть data_base64
        image_available = (
            data.image is not None and 
            data.image.available and 
            data.image.data_base64 is not None and 
            len(data.image.data_base64) > 0
        )
        
        # Если изображение доступно, используем Vision API
        image_url = None
        if image_available:
            image_url = decode_image_for_vision(data.image)
            if image_url:
                logger.info(f"Image available for Vision API: {data.image.width}x{data.image.height}")
        
        # Модели с поддержкой Vision
        vision_models = ["gpt-4", "claude", "gemini", "llava", "vision"]
        supports_vision = any(model in API_MODEL.lower() for model in vision_models)
        
        if image_url and supports_vision:
            # Vision запрос с изображением
            logger.info("Sending request to LLM Vision API with image")
            messages = [
                {"role": "system", "content": current_system_prompt},
                {
                    "role": "user",
                    "content": [
                        {"type": "text", "text": user_prompt},
                        {"type": "image_url", "image_url": {"url": image_url}}
                    ]
                }
            ]
            log_entry["image_sent"] = True
        else:
            # Текстовый запрос без изображения
            if image_available and not supports_vision:
                logger.warning(f"Model {API_MODEL} may not support vision, sending text-only request")
            logger.info("Sending text-only request to LLM API")
        
        # Запрос к API (OpenAI или OpenRouter)
        response = openai_client.chat.completions.create(
            model=API_MODEL,
            messages=messages,
            max_tokens=100,
            temperature=0.3,
        )
        
        latency_ms = round((time.time() - t_start) * 1000)
        log_entry["latency_ms"] = latency_ms
        
        # Извлекаем информацию о токенах если доступна
        if response.usage:
            log_entry["tokens_prompt"] = response.usage.prompt_tokens
            log_entry["tokens_completion"] = response.usage.completion_tokens
        
        # Парсинг ответа — безопасная обработка None
        content = None
        if response.choices and len(response.choices) > 0 and response.choices[0].message:
            content = response.choices[0].message.content
            if content:
                content = content.strip()
            else:
                log_entry["error"] = "API returned content=None"
                logger.error("LLM response content was None.")
        else:
            log_entry["error"] = "API returned empty choices or no message"
            logger.error("LLM response choices were empty or message was missing.")
        
        if not content:
            # content пустой — возвращаем STOP
            log_entry["parsed_command"] = "STOP"
            log_entry["parsed_duration_ms"] = DEFAULT_DURATION_MS
            llm_log.append(log_entry)
            if len(llm_log) > MAX_LLM_LOG:
                llm_log.pop(0)
            return CommandResponse(command="STOP", duration_ms=DEFAULT_DURATION_MS)
        
        logger.info(f"LLM response: {content}")
        log_entry["raw_response"] = content
        
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
                
                log_entry["parsed_command"] = command
                log_entry["parsed_duration_ms"] = duration
                
                llm_log.append(log_entry)
                if len(llm_log) > MAX_LLM_LOG:
                    llm_log.pop(0)
                
                return CommandResponse(command=command, duration_ms=duration)
        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse LLM JSON response: {e}")
            log_entry["error"] = f"JSON parse error: {e}"
        
        # Если не удалось распарсить, используем STOP
        log_entry["parsed_command"] = "STOP"
        log_entry["parsed_duration_ms"] = DEFAULT_DURATION_MS
        llm_log.append(log_entry)
        if len(llm_log) > MAX_LLM_LOG:
            llm_log.pop(0)
        
        return CommandResponse(command="STOP", duration_ms=DEFAULT_DURATION_MS)
        
    except Exception as e:
        logger.error(f"OpenAI API error: {e}")
        log_entry["error"] = str(e)
        log_entry["latency_ms"] = round((time.time() - t_start) * 1000)
        log_entry["parsed_command"] = "STOP"
        log_entry["parsed_duration_ms"] = DEFAULT_DURATION_MS
        llm_log.append(log_entry)
        if len(llm_log) > MAX_LLM_LOG:
            llm_log.pop(0)
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


# ==================== ДЕТЕКЦИЯ ПАДЕНИЯ ====================

def check_fall_detection(data: CarDataRequest):
    """Проверяет данные MPU6050 на предмет падения или опрокидывания."""
    global alerts
    if not data.sensors.mpu6050:
        return

    mpu = data.sensors.mpu6050
    
    is_tilted_x = abs(mpu.ax) > FALL_THRESHOLDS["ax_max"]
    is_tilted_y = abs(mpu.ay) > FALL_THRESHOLDS["ay_max"]
    is_not_upright = abs(mpu.az) < FALL_THRESHOLDS["az_min"]
    is_rotating = (
        abs(mpu.gx) > FALL_THRESHOLDS["gyro_max"]
        or abs(mpu.gy) > FALL_THRESHOLDS["gyro_max"]
        or abs(mpu.gz) > FALL_THRESHOLDS["gyro_max"]
    )

    # Машина считается «упавшей» если она наклонена И вращается
    # ИЛИ если она явно не вертикальна (az далёк от 9.8)
    if (is_tilted_x or is_tilted_y or is_not_upright) and is_rotating:
        alert_message = "⚠ Car might have FALLEN or is unstable!"
        alert_details = {
            "ax": mpu.ax, "ay": mpu.ay, "az": mpu.az,
            "gx": mpu.gx, "gy": mpu.gy, "gz": mpu.gz,
            "is_tilted_x": is_tilted_x,
            "is_tilted_y": is_tilted_y,
            "is_not_upright": is_not_upright,
            "is_rotating": is_rotating,
        }
        alert_entry = {
            "timestamp": datetime.now().isoformat(),
            "session_id": data.session_id,
            "step": data.step,
            "message": alert_message,
            "details": alert_details,
            "type": "FALL_DETECTION",
        }
        alerts.append(alert_entry)
        if len(alerts) > MAX_ALERTS:
            alerts.pop(0)
        logger.warning(f"🚨 ALERT: {alert_message} | MPU: ax={mpu.ax} ay={mpu.ay} az={mpu.az} gx={mpu.gx} gy={mpu.gy} gz={mpu.gz}")


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
        "metrics_stored": len(metrics_history),
        "images_saved": len(saved_images)
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
        
        # Проверяем наличие изображения
        has_image = (
            data.image is not None and 
            data.image.available and 
            data.image.data_base64 is not None and 
            len(data.image.data_base64) > 0
        )
        
        logger.info(f"Received data: session={data.session_id}, step={data.step}")
        logger.info(f"Sensors: dist={data.sensors.distance_cm:.1f}cm, dark={data.sensors.light_dark}")
        if has_image:
            logger.info(f"Image available: {data.image.width}x{data.image.height}, {len(data.image.data_base64)} bytes base64")
        
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
        
        # Сохраняем изображение если есть
        if data.image and data.image.available and data.image.data_base64:
            image_info = save_image(data.image, data.session_id, data.step)
            if image_info:
                metrics_entry["image_path"] = image_info["filename"]
        
        metrics_history.append(metrics_entry)
        
        # Ограничиваем размер истории
        if len(metrics_history) > MAX_METRICS_HISTORY:
            metrics_history.pop(0)
        
        # Проверка на падение по MPU6050
        check_fall_detection(data)
        
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


# ==================== IMAGES ENDPOINTS ====================

@app.get("/images")
async def get_images(limit: int = 50):
    """Список сохранённых изображений"""
    return {
        "total": len(saved_images),
        "images": saved_images[-limit:]
    }


@app.get("/images/latest")
async def get_latest_image():
    """Получить последнее изображение"""
    if not saved_images:
        raise HTTPException(status_code=404, detail="No images available")
    
    latest = saved_images[-1]
    filepath = Path(latest["path"])
    
    if not filepath.exists():
        raise HTTPException(status_code=404, detail="Image file not found")
    
    return FileResponse(
        filepath,
        media_type="image/png",
        filename=latest["filename"]
    )


@app.get("/images/{filename}")
async def get_image(filename: str):
    """Получить изображение по имени файла"""
    filepath = IMAGES_DIR / filename
    
    if not filepath.exists():
        raise HTTPException(status_code=404, detail="Image not found")
    
    media_type = "image/png" if filename.endswith(".png") else "application/octet-stream"
    return FileResponse(filepath, media_type=media_type, filename=filename)


@app.delete("/images")
async def clear_images():
    """Удалить все сохранённые изображения"""
    global saved_images
    
    deleted_count = 0
    for image_info in saved_images:
        filepath = Path(image_info["path"])
        if filepath.exists():
            filepath.unlink()
            deleted_count += 1
    
    saved_images.clear()
    return {"status": "cleared", "deleted": deleted_count}


@app.get("/images/stats")
async def get_images_stats():
    """Статистика по изображениям"""
    if not saved_images:
        return {"total": 0}
    
    total_size = sum(img.get("size_bytes", 0) for img in saved_images)
    
    return {
        "total": len(saved_images),
        "total_size_bytes": total_size,
        "total_size_mb": round(total_size / (1024 * 1024), 2),
        "first_image": saved_images[0]["timestamp"],
        "last_image": saved_images[-1]["timestamp"],
        "directory": str(IMAGES_DIR.absolute())
    }


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


# ==================== SYSTEM PROMPT ENDPOINTS ====================

@app.get("/system-prompt")
async def get_system_prompt():
    """Получение текущего системного промпта"""
    return {
        "system_prompt": current_system_prompt,
        "default_prompt": SYSTEM_PROMPT,
        "is_default": current_system_prompt == SYSTEM_PROMPT,
    }


@app.put("/system-prompt")
async def set_system_prompt(request: Request):
    """Изменение системного промпта на лету"""
    global current_system_prompt
    body = await request.json()
    new_prompt = body.get("system_prompt", "").strip()
    if not new_prompt:
        raise HTTPException(status_code=400, detail="system_prompt cannot be empty")
    current_system_prompt = new_prompt
    logger.info(f"System prompt updated ({len(new_prompt)} chars)")
    return {"status": "updated", "length": len(new_prompt)}


@app.post("/system-prompt/reset")
async def reset_system_prompt():
    """Сброс системного промпта к значению по умолчанию"""
    global current_system_prompt
    current_system_prompt = SYSTEM_PROMPT
    logger.info("System prompt reset to default")
    return {"status": "reset", "system_prompt": current_system_prompt}


# ==================== LLM LOG ENDPOINTS ====================

@app.get("/llm-log")
async def get_llm_log(limit: int = 50):
    """Получение лога LLM взаимодействий"""
    return {
        "total": len(llm_log),
        "entries": llm_log[-limit:]
    }


@app.get("/llm-log/latest")
async def get_latest_llm_log():
    """Получение последней записи лога LLM"""
    if not llm_log:
        return {"error": "No LLM log entries"}
    return llm_log[-1]


@app.delete("/llm-log")
async def clear_llm_log():
    """Очистка лога LLM"""
    llm_log.clear()
    return {"status": "cleared"}


@app.get("/llm-log/stats")
async def get_llm_log_stats():
    """Статистика LLM лога"""
    if not llm_log:
        return {"total": 0}
    
    latencies = [e["latency_ms"] for e in llm_log if e.get("latency_ms") is not None and e["latency_ms"] > 0]
    errors = [e for e in llm_log if e.get("error")]
    commands_count: Dict[str, int] = {}
    for e in llm_log:
        cmd = e.get("parsed_command", "UNKNOWN")
        commands_count[cmd] = commands_count.get(cmd, 0) + 1
    
    return {
        "total": len(llm_log),
        "errors": len(errors),
        "error_rate": round(len(errors) / len(llm_log) * 100, 1) if llm_log else 0,
        "latency": {
            "min_ms": min(latencies) if latencies else 0,
            "max_ms": max(latencies) if latencies else 0,
            "avg_ms": round(sum(latencies) / len(latencies)) if latencies else 0,
        },
        "commands_distribution": commands_count,
    }


# ==================== ALERTS ====================

@app.get("/alerts")
async def get_alerts(limit: int = 50):
    """Получить алерты"""
    return {"alerts": alerts[-limit:], "total": len(alerts)}


@app.delete("/alerts")
async def clear_alerts():
    """Очистить алерты"""
    global alerts
    count = len(alerts)
    alerts = []
    return {"message": f"Cleared {count} alerts"}


# ==================== DASHBOARD ====================

@app.get("/dashboard", response_class=HTMLResponse)
async def dashboard():
    """Веб-интерфейс для мониторинга"""
    dashboard_path = Path(__file__).parent / "static" / "dashboard.html"
    if not dashboard_path.exists():
        raise HTTPException(status_code=404, detail="Dashboard not found")
    return HTMLResponse(content=dashboard_path.read_text(encoding="utf-8"))


# Полная сводка для dashboard polling
@app.get("/dashboard/poll")
async def dashboard_poll():
    """Единый endpoint для polling всех данных дашборда"""
    latest_metrics = metrics_history[-1] if metrics_history else None
    latest_llm = llm_log[-1] if llm_log else None
    
    # Статистика по командам
    commands_count: Dict[str, int] = {}
    for e in llm_log:
        cmd = e.get("parsed_command", "UNKNOWN")
        commands_count[cmd] = commands_count.get(cmd, 0) + 1
    
    # Статистика latency
    latencies = [e["latency_ms"] for e in llm_log if e.get("latency_ms") and e["latency_ms"] > 0]
    
    return {
        "status": {
            "mode": "DEMO" if DEMO_MODE else "LLM",
            "model": API_MODEL,
            "api_base": API_BASE_URL if not DEMO_MODE else "N/A",
            "commands_processed": len(command_history),
            "metrics_stored": len(metrics_history),
            "llm_log_count": len(llm_log),
            "images_saved": len(saved_images),
            "uptime": datetime.now().isoformat(),
        },
        "latest_metrics": latest_metrics,
        "latest_llm": latest_llm,
        "recent_commands": command_history[-20:],
        "recent_llm_log": llm_log[-20:],
        "recent_metrics": metrics_history[-50:],
        "system_prompt": current_system_prompt,
        "is_default_prompt": current_system_prompt == SYSTEM_PROMPT,
        "commands_distribution": commands_count,
        "latency_stats": {
            "min_ms": min(latencies) if latencies else 0,
            "max_ms": max(latencies) if latencies else 0,
            "avg_ms": round(sum(latencies) / len(latencies)) if latencies else 0,
        },
        "error_count": sum(1 for e in llm_log if e.get("error")),
        "alerts": alerts[-20:],
        "alerts_count": len(alerts),
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
    print(f"  Dashboard: http://0.0.0.0:8000/dashboard")
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


