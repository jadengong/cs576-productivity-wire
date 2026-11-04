# Communication Protocol

This document is the shared contract between the firmware, central server, and desktop clients.

## Trigger event

Sent by the ESP32 to the central server.

```json
{
  "device_id": "tripwire-01",
  "event": "triggered",
  "timestamp": "2026-10-05T21:30:00Z"
}
```

## Host command

Sent by the central server to a connected desktop client.

```json
{
  "command": "switch_workspace",
  "target": "host-01",
  "event_id": "event-123"
}
```

## Host acknowledgement

Sent by the desktop client after attempting the command.

```json
{
  "event_id": "event-123",
  "status": "completed"
}
```

These formats are preliminary and should be updated whenever the integration contract changes.

## Implemented transport

Sensors POST trigger events to `/api/triggers`. Hosts connect to
`/ws/hosts/{host_id}`. The server reads a `DEVICE_HOSTS` JSON environment variable
to map each device ID to its destination host. Commands use UUID event IDs.

HTTP 202 confirms event acceptance. The response also contains `delivery`:
`sent`, `unmapped`, `host_offline`, or `send_failed`. A successful socket send
does not confirm command execution. Host acknowledgements above remain planned.
There is no offline replay. Duplicate host IDs are rejected. Use one worker.

