from datetime import datetime, timezone
from uuid import uuid4

from fastapi import FastAPI, status

from app.models import (
    HealthResponse,
    TriggerAccepted,
    TriggerEvent,
    TriggerRequest,
)
from app.store import event_store


app = FastAPI(
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
def create_trigger(trigger: TriggerRequest) -> TriggerAccepted:
    event = TriggerEvent(
        **trigger.model_dump(),
        event_id=uuid4(),
        received_at=datetime.now(timezone.utc),
    )
    event_store.add(event)
    return TriggerAccepted(event_id=event.event_id)


@app.get("/api/events", response_model=list[TriggerEvent], tags=["events"])
def list_events() -> list[TriggerEvent]:
    return event_store.list()

