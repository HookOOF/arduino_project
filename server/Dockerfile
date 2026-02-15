# LLM Car Controller Server - Dockerfile

FROM python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application
COPY main.py .
COPY static/ static/

# Create images directory
RUN mkdir -p images

# Expose port
EXPOSE 8000

# Environment variables
# Для OpenRouter:
ENV OPENROUTER_API_KEY=""
ENV OPENAI_BASE_URL="https://openrouter.ai/api/v1"
ENV OPENAI_MODEL="openai/gpt-4o-mini"

# Для OpenAI (раскомментировать):
# ENV OPENAI_API_KEY=""
# ENV OPENAI_BASE_URL="https://api.openai.com/v1"
# ENV OPENAI_MODEL="gpt-4o-mini"

# Run server
CMD ["python", "main.py"]


