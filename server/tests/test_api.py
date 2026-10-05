from fastapi.testclient import TestClient


VALID_TRIGGER = {
    "device_id": "tripwire-01",
    "event": "triggered",
    "timestamp": "2026-10-05T21:30:00Z",
}


def test_health_check(client: TestClient) -> None:
    response = client.get("/health")

    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_accepts_and_records_trigger(client: TestClient) -> None:
    create_response = client.post("/api/triggers", json=VALID_TRIGGER)

    assert create_response.status_code == 202
    assert create_response.json()["status"] == "accepted"
    assert create_response.json()["event_id"]

    events_response = client.get("/api/events")
    events = events_response.json()

    assert events_response.status_code == 200
    assert len(events) == 1
    assert events[0]["event_id"] == create_response.json()["event_id"]
    assert events[0]["device_id"] == "tripwire-01"
    assert events[0]["status"] == "received"
    assert events[0]["received_at"]


def test_rejects_unknown_event_type(client: TestClient) -> None:
    response = client.post(
        "/api/triggers",
        json={**VALID_TRIGGER, "event": "opened"},
    )

    assert response.status_code == 422


def test_rejects_missing_device_id(client: TestClient) -> None:
    invalid_trigger = dict(VALID_TRIGGER)
    del invalid_trigger["device_id"]

    response = client.post("/api/triggers", json=invalid_trigger)

    assert response.status_code == 422


def test_returns_newest_event_first(client: TestClient) -> None:
    client.post("/api/triggers", json=VALID_TRIGGER)
    client.post(
        "/api/triggers",
        json={**VALID_TRIGGER, "device_id": "tripwire-02"},
    )

    events = client.get("/api/events").json()

    assert [event["device_id"] for event in events] == [
        "tripwire-02",
        "tripwire-01",
    ]

