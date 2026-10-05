from datetime import datetime
from typing import Literal
from uuid import UUID

from pydantic import BaseModel, ConfigDict, Field


class TriggerRequest(BaseModel):
    """A tripwire event reported by a sensor device."""

    model_config = ConfigDict(str_strip_whitespace=True)

    device_id: str = Field(min_length=1, max_length=100)
    event: Literal["triggered"]
    timestamp: datetime


class TriggerEvent(TriggerRequest):
    """A trigger event accepted and identified by the server."""

    event_id: UUID
    received_at: datetime
    status: Literal["received"] = "received"


class TriggerAccepted(BaseModel):
    """Response returned after the server accepts a trigger."""

    status: Literal["accepted"] = "accepted"
    event_id: UUID


class HealthResponse(BaseModel):
    status: Literal["ok"] = "ok"

