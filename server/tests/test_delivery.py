from app.main import app
import pytest
from starlette.websockets import WebSocketDisconnect


TRIGGER = {"device_id": "tripwire-01", "event": "triggered", "timestamp": "2026-10-28T12:00:00Z"}


def test_delivers_command_to_configured_host(client):
    app.state.device_hosts = {"tripwire-01": "host-01"}
    with client.websocket_connect("/ws/hosts/host-01") as host:
        response = client.post("/api/triggers", json=TRIGGER)
        assert response.status_code == 202
        assert response.json()["delivery"] == "sent"
        assert host.receive_json() == {
            "command": "switch_workspace", "target": "host-01",
            "event_id": response.json()["event_id"],
        }


def test_offline_host(client):
    app.state.device_hosts = {"tripwire-01": "offline"}
    response = client.post("/api/triggers", json=TRIGGER)
    assert response.json()["delivery"] == "host_offline"
    assert len(client.get("/api/events").json()) == 1


def test_unmapped_sensor(client):
    app.state.device_hosts = {}
    assert client.post("/api/triggers", json=TRIGGER).json()["delivery"] == "unmapped"


def test_routes_to_matching_host(client):
    app.state.device_hosts = {"tripwire-01": "host-01", "tripwire-02": "host-02"}
    with client.websocket_connect("/ws/hosts/host-01") as first:
        with client.websocket_connect("/ws/hosts/host-02") as second:
            a = client.post("/api/triggers", json=TRIGGER).json()
            b = client.post("/api/triggers", json={**TRIGGER, "device_id": "tripwire-02"}).json()
            assert first.receive_json()["event_id"] == a["event_id"]
            assert second.receive_json()["event_id"] == b["event_id"]


def test_reconnect(client):
    app.state.device_hosts = {"tripwire-01": "host-01"}
    with client.websocket_connect("/ws/hosts/host-01"):
        pass
    assert client.post("/api/triggers", json=TRIGGER).json()["delivery"] == "host_offline"
    with client.websocket_connect("/ws/hosts/host-01") as host:
        assert client.post("/api/triggers", json=TRIGGER).json()["delivery"] == "sent"
        assert host.receive_json()["target"] == "host-01"


def test_duplicate_connection_does_not_replace_original(client):
    app.state.device_hosts = {"tripwire-01": "host-01"}
    with client.websocket_connect("/ws/hosts/host-01") as host:
        with pytest.raises(WebSocketDisconnect) as error:
            with client.websocket_connect("/ws/hosts/host-01"):
                pass
        assert error.value.code == 1008
        response = client.post("/api/triggers", json=TRIGGER).json()
        assert host.receive_json()["event_id"] == response["event_id"]


def test_send_failure_retains_event_and_removes_stale_connection(client):
    class BrokenSocket:
        async def send_json(self, message):
            raise OSError("connection lost")

    app.state.device_hosts = {"tripwire-01": "host-01"}
    app.state.hosts["host-01"] = BrokenSocket()
    assert client.post("/api/triggers", json=TRIGGER).json()["delivery"] == "send_failed"
    assert "host-01" not in app.state.hosts
    assert len(client.get("/api/events").json()) == 1
