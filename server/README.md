# Central Server

This service receives tripwire events from an ESP32 or another sensor client.
The first milestone provides input validation, event IDs, timestamps, and an
in-memory event history. WebSocket delivery to desktop clients will be added in
the next milestone.

## Requirements

- Python 3.11 or newer

## Setup

From this directory:

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements-dev.txt
```

## Run the server

```bash
uvicorn app.main:app --reload
```

The API is available at `http://127.0.0.1:8000`. FastAPI's interactive API
documentation is available at `http://127.0.0.1:8000/docs`.

## Try it

```bash
curl http://127.0.0.1:8000/health
```

```bash
curl -X POST http://127.0.0.1:8000/api/triggers \
  -H 'Content-Type: application/json' \
  -d '{
    "device_id": "tripwire-01",
    "event": "triggered",
    "timestamp": "2026-10-05T21:30:00Z"
  }'
```

```bash
curl http://127.0.0.1:8000/api/events
```

## Test

```bash
pytest
```

Event history is currently stored in memory and is cleared whenever the server
restarts.

