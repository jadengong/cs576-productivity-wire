# Central Server

This service receives tripwire events from an ESP32 or another sensor client.
The first milestone provides input validation, event IDs, timestamps, and an
in-memory event history. Hosts connect over WebSocket and receive commands for
their mapped sensors.

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

## Trigger-to-host demo (PowerShell)

Run each block in a separate terminal from `server/`, using the environment
where you installed `requirements-dev.txt`.

1. Configure routing and start the server:

```powershell
$env:DEVICE_HOSTS = '{"tripwire-01":"host-01"}'
python -m uvicorn app.main:app --host 127.0.0.1 --port 8000
```

2. Connect the mock host:

```powershell
python mock_host.py
```

3. Send a simulated sensor event:

```powershell
$body = @{ device_id = 'tripwire-01'; event = 'triggered'; timestamp = [DateTime]::UtcNow.ToString('o') } | ConvertTo-Json
Invoke-RestMethod -Method Post -Uri http://127.0.0.1:8000/api/triggers -ContentType 'application/json' -Body $body
```

The response includes `delivery: sent`, and the mock host prints a
`switch_workspace` command with the same event ID. The mock only prints messages.
It does not switch workspaces or acknowledge execution.

Configure `DEVICE_HOSTS` as a JSON object mapping sensor IDs to host IDs before
starting the server. It defaults to an empty mapping. Hosts connect at
`/ws/hosts/{host_id}`. Duplicate host connections are rejected with code 1008.

Delivery values are `sent`, `unmapped`, `host_offline`, or `send_failed`.
All accepted triggers remain in event history. `sent` means the WebSocket send
completed, not that the host executed the command. Offline events are not queued
or replayed. Reconnected hosts receive only subsequent triggers.

This prototype uses process-local connections: run one server worker. It has no
authentication and is intended for a trusted local demo, not public deployment.

