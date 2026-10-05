# System Architecture

## Components

1. **Tripwire sensor** detects a physical trigger.
2. **ESP32 firmware** reads the sensor and reports the event over Wi-Fi.
3. **Central server** validates the event and routes a command to the intended host.
4. **Desktop client** receives the command and switches the operating-system workspace.
5. **Dashboard** optionally displays device state and event history.

## Initial transport plan

- ESP32 to server: HTTP
- Server to desktop client: WebSocket
- Message representation: JSON

The transport choices can be revised after the first end-to-end prototype.

