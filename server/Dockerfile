# LLM Car Controller Server - Dockerfile

FROM python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application
COPY main.py .

# Create images directory
RUN mkdir -p images

# Expose port
EXPOSE 8000

# Environment variables
# Для OpenRouter:
ENV OPENROUTER_API_KEY="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"
ENV OPENAI_BASE_URL="https://openrouter.ai/api/v1"
ENV OPENAI_MODEL="google/gemini-3-flash-preview"

# Для OpenAI (раскомментировать):
# ENV OPENAI_API_KEY=""
# ENV OPENAI_BASE_URL="https://api.openai.com/v1"
# ENV OPENAI_MODEL="gpt-4o-mini"

# Run server
CMD ["python", "main.py"]


