from datetime import datetime, timezone
from uuid import uuid4

import json
import os
from contextlib import asynccontextmanager

from fastapi import FastAPI, status, WebSocket, WebSocketDisconnect

from app.models import (
    HealthResponse,
    TriggerAccepted,
    TriggerEvent,
    TriggerRequest,
)
from app.store import event_store


@asynccontextmanager
async def lifespan(app):
    routes = json.loads(os.environ.get("DEVICE_HOSTS", "{}"))
    if not isinstance(routes, dict) or any(
        not isinstance(k, str) or not k.strip() or not isinstance(v, str) or not v.strip()
        for k, v in routes.items()
    ):
        raise ValueError("DEVICE_HOSTS must be a JSON object mapping device IDs to host IDs")
    app.state.device_hosts = routes
    app.state.hosts = {}
    yield
    app.state.hosts.clear()


app = FastAPI(
    lifespan=lifespan,
    title="Productivity Wire Central Server",
    description="Receives tripwire events and coordinates workspace switching.",
    version="0.1.0",
)


@app.get("/health", response_model=HealthResponse, tags=["system"])
def health() -> HealthResponse:
    return HealthResponse()


@app.post(
    "/api/triggers",
    response_model=TriggerAccepted,
    status_code=status.HTTP_202_ACCEPTED,
    tags=["events"],
)
async def create_trigger(trigger: TriggerRequest) -> TriggerAccepted:
    event = TriggerEvent(
        **trigger.model_dump(),
        event_id=uuid4(),
        received_at=datetime.now(timezone.utc),
    )
    event_store.add(event)
    target = app.state.device_hosts.get(trigger.device_id)
    delivery = "unmapped"
    if target:
        socket = app.state.hosts.get(target)
        delivery = "host_offline"
        if socket:
            try:
                await socket.send_json({
                    "command": "switch_workspace", "target": target,
                    "event_id": str(event.event_id),
                })
                delivery = "sent"
            except (WebSocketDisconnect, OSError, RuntimeError):
                if app.state.hosts.get(target) is socket:
                    del app.state.hosts[target]
                delivery = "send_failed"
    return TriggerAccepted(event_id=event.event_id, delivery=delivery)


@app.websocket("/ws/hosts/{host_id}")
async def connect_host(socket: WebSocket, host_id: str):
    if not host_id.strip() or len(host_id) > 100:
        await socket.close(code=1008)
        return
    # Keep one connection per host. Reject duplicates rather than replacing a live client.
    if host_id in app.state.hosts:
        await socket.close(code=1008)
        return
    app.state.hosts[host_id] = socket
    try:
        await socket.accept()
        while True:
            await socket.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        if app.state.hosts.get(host_id) is socket:
            del app.state.hosts[host_id]


@app.get("/api/events", response_model=list[TriggerEvent], tags=["events"])
def list_events() -> list[TriggerEvent]:
    return event_store.list()

