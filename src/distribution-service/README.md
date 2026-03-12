# Distribution Service

The Distribution Service pushes configuration updates to connected clients in real-time using gRPC bidirectional streaming. When a new config is uploaded, it immediately delivers it to all subscribed client SDK instances.

## Overview

The Distribution Service acts as the push mechanism in the configuration management system. Clients maintain persistent gRPC streams and receive config updates without polling.

### Key Features

- Real-time bidirectional gRPC streaming for instant config delivery
- Rollout strategy execution: `ALL_AT_ONCE`, `CANARY`, `PERCENTAGE`
- Kafka consumer for rollout events (with DB polling as an idempotent catch-up)
- Redis-based caching to reduce database load
- Heartbeat monitor evicts timed-out clients and cancels their streams
- Version ordering: clients are never downgraded to an older config
- StatsD metrics, audit logging, and graceful disconnection handling

## gRPC API

Defined in `proto/distribution.proto` as `DistributionService`:

| RPC | Description |
|-----|-------------|
| `Subscribe` | Bidirectional stream - clients receive config updates |
| `ReportHealth` | Clients report health status during rollouts |
| `Heartbeat` | Keep-alive connection management |

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                 Distribution Service                  │
│                                                       │
│  ┌───────────┐  ┌───────────┐  ┌───────────────┐   │
│  │  gRPC     │  │  Cache    │  │   Database    │   │
│  │  Server   │──│  Manager  │──│   Manager     │   │
│  │  (:8082)  │  │  (Redis)  │  │ (PostgreSQL)  │   │
│  └───────────┘  └───────────┘  └───────────────┘   │
│       │                              │               │
│       │         ┌───────────┐       │               │
│       └────────>│  Event    │<──────┘               │
│                 │ Publisher │                        │
│                 │  (Kafka)  │                        │
│                 └───────────┘                        │
└─────────────────────────────────────────────────────┘
                        │
               ┌────────┴────────┐
               │                 │
        ┌──────▼──────┐   ┌─────▼──────┐
        │  Client SDK │   │ Client SDK │
        │  Instance 1 │   │ Instance 2 │
        └─────────────┘   └────────────┘
```

## Client Flow

```
1. Client connects with SubscribeRequest
   → Service name: "user-service"
   → Instance ID: "instance-123"
   → Current version: 0

2. Service registers instance in service_instances table
   → Publishes client_connect event to Kafka

3. Service fetches latest config
   → Check Redis cache first
   → Cache miss → query PostgreSQL (config_metadata + config_data)
   → Cache result in Redis

4. Service sends ConfigUpdate to client
   → Version: 3, Format: json, Content: {...}

5. Heartbeat every 30 seconds
   → Client: "I'm alive with v3"
   → Server: "Acknowledged"

6. New config uploaded → service pushes update
   → Client receives v4 immediately

7. Client disconnects
   → Instance status updated in service_instances
   → client_disconnect event published to Kafka
```

## Rollout Execution

Rollouts are triggered in two ways:

1. **Kafka event** — The API service publishes a `config.rollout_started` event after `StartRollout`. The distribution service consumes this immediately and begins pushing configs to the appropriate set of instances.
2. **DB polling** — Every 30 seconds, the service polls `rollout_state` for any `IN_PROGRESS` or `PENDING` rollouts. This handles the Kafka consumer rebalance window (2–3 s gap at startup) and acts as an idempotent catch-up mechanism.

### Version Ordering

When pushing a config to a client, the distribution service skips any client whose `current_version` is already equal to or greater than the rollout version. This prevents clients from being downgraded when a periodic poll re-executes an older rollout.

### Rollout Strategies

| Strategy | Behaviour |
|----------|-----------|
| `ALL_AT_ONCE` | Pushed to all connected instances. Completed when all instances receive the config (or no instances are connected). |
| `CANARY` | Pushed to ~10% of instances. Stays `IN_PROGRESS` until `configctl promote` is called. If no clients are connected, stays `IN_PROGRESS` (does not auto-complete). |
| `PERCENTAGE` | Pushed to the specified fraction of instances. Completed when the target percentage is reached. |

## Components

### `distribution_service.cpp`

Core gRPC service:
- `Subscribe()` — Bidirectional streaming. Registers the client, sends the latest rolled-out config, then reads heartbeats
- `ExecuteRollout()` — Pushes a config to the appropriate subset of instances based on strategy
- `PollPendingRollouts()` — DB catch-up: re-runs any open rollouts on startup and every 30 s
- `HeartbeatMonitorLoop()` — Evicts clients that have not sent a heartbeat within the timeout, cancels their stream context

### `database_manager.cpp`

PostgreSQL operations:
- `GetLatestConfig()` - Fetch latest config for a service
- `GetConfigByVersion()` - Fetch specific version
- `ListConfigs()` - List all configs for a service
- `UpdateClientVersion()` - Track client's current config version in `service_instances`
- `RecordConfigDelivery()` - Write to `audit_log`

### `cache_manager.cpp`

Redis caching layer:
- `Get()` - Retrieve cached config
- `Set()` - Store config with TTL (5 minutes default)
- `Invalidate()` - Remove cache entry
- `Clear()` - Flush all entries

### `event_publisher.cpp`

Kafka event publishing:
- `PublishClientConnect()` - New client connected
- `PublishClientDisconnect()` - Client disconnected
- `PublishConfigUpdate()` - Config delivered to client
- `PublishClientTimeout()` - Client heartbeat timeout

### `metrics_client.cpp`

StatsD metrics:
- `distribution.clients.connected` - Active connections
- `distribution.clients.disconnected` - Disconnection count
- `distribution.config.delivered` - Delivery count
- `distribution.cache.hit` / `cache.miss` - Cache efficiency
- `distribution.db.query.time` - Database latency

### `config.cpp`

YAML configuration loading:
- `server` - Port, max connections
- `postgres` - Database connection
- `redis` - Cache settings
- `kafka` - Broker and topic configuration
- `statsd` - Metrics endpoint
- `monitoring` - Heartbeat interval, health check port

## Configuration

**Docker** (`config/distribution-service.yml`):
```yaml
server:
  port: 8082
  max_connections: 1000
postgres:
  host: postgres
  port: 5432
redis:
  host: redis
  port: 6379
  cache_ttl: 300
kafka:
  brokers:
    - kafka:9092
  topic: config.updates
statsd:
  host: statsd-exporter
  port: 9125
  prefix: distribution
monitoring:
  heartbeat_interval: 30s
  health_check_port: 8083
```

**Local** (`config/distribution-service-local.yml`):
```yaml
postgres:
  host: localhost
redis:
  host: localhost
kafka:
  brokers:
    - localhost:9092
statsd:
  host: localhost
```

## Building & Running

```bash
# Build locally
make distribution-service

# Run locally
./bin/distribution-service config/distribution-service-local.yml

# Build and run in Docker
make services
docker compose logs -f distribution-service
```

## Testing

```bash
# Start the service
./bin/distribution-service config/distribution-service-local.yml

# In another terminal, run the example client
./bin/simple_client localhost:8082 test-service
```

Expected output:
```
>>> CONFIG UPDATE <<<
Config ID: test-config-v1
Version: 1
Format: json
Content: {"test": true, "message": "Hello from config!"}
```

## Kafka Events

Published to `config.updates` topic:

```json
{
  "event_type": "config_update",
  "service_name": "user-service",
  "instance_id": "instance-123",
  "version": 3,
  "timestamp": 1708300200
}
```

## Performance

- **Concurrent clients**: 1,000+ simultaneous connections
- **Throughput**: 10,000+ config deliveries per second
- **Latency**: <10ms p95 (cache hit), <50ms p95 (cache miss)
- **Cache hit rate**: >95% in steady state

## Code Structure

```
src/distribution-service/
├── main.cpp                # Entry point, signal handling
├── distribution_service.cpp # gRPC streaming implementation
├── database_manager.cpp    # PostgreSQL operations
├── cache_manager.cpp       # Redis caching layer
├── event_publisher.cpp     # Kafka event publishing
├── metrics_client.cpp      # StatsD metrics
└── config.cpp              # YAML config loading

include/distribution_service/
├── distribution_service.h
├── database_manager.h
├── cache_manager.h
├── event_publisher.h
├── metrics_client.h
└── config.h
```

## Related

- [Proto Definition](../../proto/distribution.proto)
- [Client SDK](../client-sdk/README.md)
- [Database Schema](../../db/migrations/)
- [API Service](../api-service/README.md)
- [Commands Reference](../../COMMANDS.md)
- [CLI Reference](../../cmd/configctl/README.md)
