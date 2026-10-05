import pytest
from fastapi.testclient import TestClient

from app.main import app
from app.store import event_store


@pytest.fixture(autouse=True)
def reset_event_store():
    event_store.clear()
    yield
    event_store.clear()


@pytest.fixture
def client() -> TestClient:
    return TestClient(app)

