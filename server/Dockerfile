# LLM Car Controller Server - Dockerfile

FROM python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application
COPY main.py .

# Expose port
EXPOSE 8000

# Environment variables
ENV OPENAI_API_KEY=""
ENV OPENAI_MODEL="gpt-4o-mini"

# Run server
CMD ["python", "main.py"]


