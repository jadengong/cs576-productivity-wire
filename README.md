# Productivity Wire

Productivity Wire is a CS576 group project that uses a physical tripwire sensor to trigger a workspace change on a registered host computer.

## System flow

```text
Tripwire sensor -> ESP32 -> Central server -> Desktop client -> Workspace switch
```

## Repository layout

- `server/` — central API, event routing, and communication
- `dashboard/` — optional monitoring and configuration frontend
- `firmware/` — ESP32 firmware and sensor integration
- `desktop-client/windows/` — Windows workspace-switching client
- `desktop-client/macos/` — macOS workspace-switching client
- `hardware/` — wiring diagrams, parts list, and hardware notes
- `tests/` — integration, reliability, and latency tests
- `docs/` — architecture, protocol, and project documentation

## Getting started

Implementation instructions will be added as each component is developed. Before building a component, review `docs/protocol.md` so every part of the system uses the same message formats.

## Team

- Jaden Ong — central server and communication
- Dean Kajosevic
- Nathan Chong
- Enzo Weiss
- Daud Abdinasir
