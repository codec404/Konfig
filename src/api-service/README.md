# API Service

The API Service is the primary entry point for managing configurations. It exposes a gRPC API for uploading, retrieving, listing, and deleting configs, as well as managing rollouts and rollbacks.

## Overview

The API Service handles all configuration lifecycle operations. When a config is uploaded, it is validated (via the Validation Service), stored in PostgreSQL, and events are published to Kafka for downstream consumers.

### Key Features

- Config upload with format detection (JSON/YAML)
- Inline syntax validation + remote validation via Validation Service
- Versioned config storage (metadata + content split across tables)
- Gradual rollout management with percentage-based deployment
- Rollback to any previous version
- Kafka event publishing for config changes
- StatsD metrics for all operations
- Audit logging for every action

## gRPC API

Defined in `proto/api.proto` as `ConfigAPIService`:

| RPC | Description |
|-----|-------------|
| `UploadConfig` | Upload a new config version for a service |
| `GetConfig` | Retrieve a config by ID |
| `ListConfigs` | List all config versions for a service |
| `DeleteConfig` | Delete a config by ID |
| `StartRollout` | Begin gradual rollout of a config |
| `GetRolloutStatus` | Check rollout progress |
| `Rollback` | Revert to a previous config version |

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    API Service                       │
│                                                      │
│  ┌───────────┐  ┌────────────┐  ┌───────────────┐  │
│  │  gRPC     │  │ Validation │  │   Database    │  │
│  │  Server   │──│  Client    │──│   Manager     │  │
│  │  (:8081)  │  │ (→ :8083)  │  │ (PostgreSQL)  │  │
│  └───────────┘  └────────────┘  └───────────────┘  │
│       │                                │             │
│  ┌────▼──────┐              ┌─────────▼──────┐     │
│  │  Kafka    │              │  StatsD Client │     │
│  │ Producer  │              │  (Metrics)     │     │
│  └───────────┘              └────────────────┘     │
└─────────────────────────────────────────────────────┘
```

## Upload Flow

1. Client sends `UploadConfigRequest` with service name, content, and format
2. API Service runs inline syntax validation (bracket matching, trailing comma detection)
3. If Validation Service is available, sends content for full validation (schema, rules, ranges)
4. On success: stores metadata in `config_metadata`, content in `config_data`
5. Publishes `config_uploaded` event to Kafka
6. Records audit log entry
7. Returns config ID and version to client

## Components

### `api_service.cpp`

Core gRPC service implementation:
- `UploadConfig()` - Validates and stores new configs
- `GetConfig()` - Retrieves config by ID (joins metadata + data)
- `ListConfigs()` - Lists versions for a service
- `DeleteConfig()` - Removes config from both tables
- `StartRollout()` - Creates rollout entry in `rollout_state`
- `GetRolloutStatus()` - Queries rollout progress
- `Rollback()` - Copies previous version as new latest

Helper methods:
- `ValidateContent()` - Inline JSON syntax validation (brackets, trailing commas)
- `PublishEvent()` - Kafka event publishing
- `ComputeHash()` - SHA-256 content hashing
- `GenerateConfigId()` - Generates `{service}-v{version}` IDs

### `validation_client.cpp`

gRPC client for the Validation Service:
- Connects to address from config (`validation_service.address`)
- Sends config content for full validation (syntax, schema, rules, ranges)
- Returns validation errors and warnings
- Gracefully degrades if validation service is unavailable

### `database_manager.cpp`

PostgreSQL operations:
- `StoreConfig()` - Inserts into `config_metadata` + `config_data`
- `GetConfig()` - Joins metadata and data by config_id
- `ListConfigs()` - Queries by service name
- `DeleteConfig()` - Removes from both tables
- `CreateRollout()` - Inserts into `rollout_state`
- `GetRolloutState()` - Queries rollout progress
- `RecordAuditEvent()` - Writes to `audit_log` with JSONB details

### `config.cpp`

YAML configuration loading with these sections:
- `server` - Port, max connections
- `postgres` - Host, port, database, credentials
- `kafka` - Broker address, topic
- `redis` - Host, port, cache TTL
- `statsd` - Host, port, prefix
- `validation_service` - Validation service address

## Configuration

**Docker** (`config/api-service.yml`):
```yaml
server:
  port: 8081
postgres:
  host: postgres
  port: 5432
kafka:
  brokers: kafka:9092
validation_service:
  address: validation-service:8083
```

**Local** (`config/api-service-local.yml`):
```yaml
postgres:
  host: localhost
kafka:
  brokers: localhost:9092
validation_service:
  address: localhost:8083
```

## Building & Running

```bash
# Build locally
make api-service

# Run locally
./bin/api-service config/api-service-local.yml

# Build and run in Docker
make services
docker compose logs -f api-service
```

## Metrics (StatsD)

- `api.upload.count` - Upload requests
- `api.get.count` - Get requests
- `api.list.count` - List requests
- `api.delete.count` - Delete requests
- `api.validation.pass` / `api.validation.fail` - Validation results

## Code Structure

```
src/api-service/
├── main.cpp              # Entry point, gRPC server setup
├── api_service.cpp       # gRPC method implementations
├── validation_client.cpp # Validation Service gRPC client
├── database_manager.cpp  # PostgreSQL operations
└── config.cpp            # YAML config loading

include/api_service/
├── api_service.h
├── validation_client.h
├── database_manager.h
└── config.h
```

## Related

- [Proto Definition](../../proto/api.proto)
- [Database Schema](../../db/migrations/)
- [Validation Service](../validation-service/README.md)
- [CLI Tool](../../cmd/configctl/)
- [Commands Reference](../../COMMANDS.md)
