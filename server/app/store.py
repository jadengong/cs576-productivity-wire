from threading import Lock

from app.models import TriggerEvent


class EventStore:
    """Thread-safe in-memory storage used by the initial prototype."""

    def __init__(self) -> None:
        self._events: list[TriggerEvent] = []
        self._lock = Lock()

    def add(self, event: TriggerEvent) -> None:
        with self._lock:
            self._events.append(event)

    def list(self) -> list[TriggerEvent]:
        with self._lock:
            return list(reversed(self._events))

    def clear(self) -> None:
        with self._lock:
            self._events.clear()


event_store = EventStore()

