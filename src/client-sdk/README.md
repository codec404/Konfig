# Client SDK

The C++ client SDK (`libconfigclient`) lets services subscribe to real-time config updates with automatic reconnection, heartbeating, and disk caching.

## Building

```bash
make sdk            # builds lib/libconfigclient.a and lib/libconfigclient.so
make dev-sdk        # build inside the dev container (Linux)
```

Headers are in `include/configclient/`. Libraries are in `lib/` after building.

## Constructor

```
ConfigClient(server_address, service_name, instance_id, cache_dir,
             heartbeat_interval_seconds, max_heartbeat_failures)
```

| Parameter | Default | Description |
|-----------|---------|-------------|
| `server_address` | — | Distribution service address, e.g. `distribution-service:8082` |
| `service_name` | — | Name of the service subscribing to configs |
| `instance_id` | `""` | Unique instance identifier (defaults to hostname) |
| `cache_dir` | `""` | Directory for disk cache (defaults to `~/.konfig/cache/`) |
| `heartbeat_interval_seconds` | `30` | How often to send keep-alive heartbeats |
| `max_heartbeat_failures` | `3` | Consecutive failures before reconnecting |

## Lifecycle

| Method | Description |
|--------|-------------|
| `Start()` | Loads disk cache, then connects and subscribes. Returns `false` if already running. |
| `Stop()` | Cancels the stream, joins threads, shuts down cleanly. |
| `IsConnected()` | Returns `true` when the gRPC stream is active. |
| `GetCurrentConfig()` | Thread-safe access to the latest `ConfigData`. |
| `GetCurrentVersion()` | Thread-safe access to the current config version. |

## Callbacks

Register callbacks before calling `Start()`:

| Callback | Signature | When called |
|----------|-----------|-------------|
| `OnConfigUpdate` | `void(const ConfigData&)` | Immediately from disk cache on startup, then on every live update |
| `OnConnectionStatus` | `void(bool connected)` | When the connection state changes |

## Heartbeat

The client sends periodic heartbeats over the gRPC stream to keep the connection alive. If `max_heartbeat_failures` consecutive writes fail, the stream is cancelled and reconnection starts automatically.

**Conservative** (long interval, tolerant of transient failures):
- `heartbeat_interval_seconds = 60`, `max_heartbeat_failures = 5`

**Aggressive** (detect dead connections faster):
- `heartbeat_interval_seconds = 10`, `max_heartbeat_failures = 2`

## Disk Cache

On every config update the SDK writes a binary cache to `~/.konfig/cache/<service>.cache`. On the next startup the cached config is served **before** the gRPC connection is established.

| Scenario | Behaviour |
|----------|-----------|
| First start, server up | No cache — waits for live stream |
| Restart, server up | Serves cache immediately, then receives live updates |
| Restart, server down | Serves cache, retries connection every 5 s |
| Corrupted cache | Discards file, falls back to live stream |

## Reconnection

The SDK runs a background stream thread. On disconnect (server restart, network issue, or heartbeat timeout) it waits 5 seconds and reconnects automatically. On reconnect, it sends its current version so the server only pushes the config if a newer one exists.

## Linking

```makefile
$(CXX) $(CXXFLAGS) $(INCLUDES) $(LDFLAGS) my_service.cpp \
    lib/libconfigclient.a -lgrpc++ -lgrpc -lprotobuf -lpthread -o bin/my_service
```

## Code Structure

```
src/client-sdk/
├── config_client.cpp       # Public ConfigClient wrapper
├── config_client_impl.cpp  # Stream thread + heartbeat thread
└── disk_cache.cpp          # Binary cache read/write

include/configclient/
├── config_client.h         # Public API
├── config_client_impl.h    # Implementation header
└── disk_cache.h            # DiskCache header
```

## Example

See `examples/simple_client/` for a complete working example.

```bash
# Build (inside dev container)
make dev-build

# Run against a local distribution service
./bin/simple_client localhost:8082 my-service
```
