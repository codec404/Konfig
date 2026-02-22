# Konfig - Dynamic Configuration Service

A distributed configuration management system built with C++17, gRPC, Kafka, and modern observability tools. Upload, validate, distribute, and roll out configuration changes across services in real-time.

## Quick Start

```bash
# 1. Start infrastructure (PostgreSQL, Redis, Kafka, monitoring)
make dev

# 2. Build and start all service containers
make services

# 3. Build the CLI tool
make cli

# 4. Upload a config
./bin/konfig upload --service my-service --file config.json --format json

# 5. Retrieve it
./bin/konfig get --id my-service-v1
```

## Architecture

```
                          ┌──────────────────┐
                          │   CLI (konfig)   │
                          │    Go / gRPC     │
                          └────────┬─────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │              │              │
             ┌──────▼──────┐ ┌────▼─────┐ ┌─────▼──────┐
             │ API Service │ │  Dist    │ │ Validation │
             │   :8081     │ │ Service  │ │  Service   │
             │  (gRPC)     │ │  :8082   │ │   :8083    │
             └──────┬──────┘ └────┬─────┘ └─────┬──────┘
                    │             │              │
        ┌───────┬───┴───┬────────┤              │
        │       │       │        │              │
   ┌────▼──┐ ┌─▼───┐ ┌─▼────┐ ┌─▼───┐    ┌────▼──┐
   │Postgres│ │Redis│ │Kafka │ │Redis│    │Postgres│
   └───────┘ └─────┘ └──────┘ └─────┘    └───────┘
```

### Services

| Service | Port | Description |
|---------|------|-------------|
| **API Service** | 8081 | Config upload, retrieval, deletion, rollout management |
| **Distribution Service** | 8082 | Real-time config push to clients via gRPC streaming |
| **Validation Service** | 8083 | Config syntax/schema/rule validation (JSON & YAML) |
| **CLI (`konfig`)** | - | Command-line tool for interacting with the API |
| **Client SDK** | - | C++ library for services to receive config updates |

### Infrastructure

| Component | Port | Purpose |
|-----------|------|---------|
| PostgreSQL | 5432 | Config metadata, data, audit logs, validation rules |
| Redis | 6379 | Caching, validation result cache |
| Kafka | 9092/9093 | Event streaming, config update notifications |
| Prometheus | 9090 | Metrics collection |
| Grafana | 3000 | Metrics dashboards (admin/admin) |
| Kafka UI | 8080 | Topic and message inspection |
| pgAdmin | 5050 | Database management |
| StatsD Exporter | 9125 | Metrics bridge to Prometheus |

## Project Structure

```
Konfig/
├── cmd/configctl/           # CLI tool (Go)
│   └── main.go
├── config/                  # Service configuration files
│   ├── api-service.yml          # Docker config
│   ├── api-service-local.yml    # Local dev config
│   ├── distribution-service.yml
│   ├── distribution-service-local.yml
│   └── validation-service.yml
├── db/migrations/           # PostgreSQL schema migrations (000-008)
├── docker/
│   ├── postgres/init.sql        # DB initialization (runs migrations)
│   ├── grafana/                 # Grafana provisioning
│   └── services/                # Per-service Dockerfiles
│       ├── api-service.Dockerfile
│       ├── distribution-service.Dockerfile
│       └── validation-service.Dockerfile
├── include/                 # C++ headers
│   ├── api_service/
│   ├── distribution_service/
│   ├── validation_service/
│   ├── configclient/            # Client SDK headers
│   └── statsdclient/            # StatsD client
├── proto/                   # Protocol Buffer definitions
│   ├── api.proto                # ConfigAPIService RPCs
│   ├── distribution.proto       # DistributionService RPCs
│   ├── validation.proto         # ValidationService RPCs
│   └── config.proto             # Shared message types
├── prometheus/              # Prometheus & StatsD config
├── scripts/                 # Build helper scripts
├── src/
│   ├── api-service/             # API Service implementation
│   ├── distribution-service/    # Distribution Service implementation
│   ├── validation-service/      # Validation Service implementation
│   ├── client-sdk/              # Client SDK library
│   └── common/                  # Shared utilities (StatsD client)
├── docker-compose.yml       # All containers
├── Dockerfile.dev           # Development build container
└── Makefile                 # Build automation
```

## Makefile Commands

### Services (Docker)

```bash
make services          # Build and start all service containers
make services-down     # Stop service containers
make services-local    # Build binaries locally (no Docker)
```

### Infrastructure

```bash
make dev               # Complete setup (dirs + infrastructure + verify)
make infra-up          # Start infrastructure containers
make infra-down        # Stop infrastructure containers
make infra-restart     # Restart infrastructure
make verify            # Health check all services
```

### Build

```bash
make proto             # Generate protobuf/gRPC code
make api-service       # Build API service locally
make distribution-service  # Build distribution service locally
make validation-service    # Build validation service locally
make sdk               # Build client SDK (shared + static)
make cache-test        # Build disk cache test binary (bin/cache_test)
make cli               # Build CLI tool (bin/konfig)
make all               # Build everything locally
make clean             # Remove build artifacts
make rebuild           # Clean + build all
```

### Database & Tools

```bash
make db-shell          # PostgreSQL interactive shell
make redis-shell       # Redis CLI
make kafka-topics      # List Kafka topics
make kafka-ui          # Open Kafka UI in browser
make grafana           # Open Grafana in browser
make pgadmin           # Open pgAdmin in browser
```

See [COMMANDS.md](COMMANDS.md) for the complete command reference.

## CLI Usage

```bash
# Upload configuration
./bin/konfig upload --service payment-service --file config.json --format json

# Get config by ID
./bin/konfig get --id payment-service-v1

# List configs for a service
./bin/konfig list --service payment-service

# Validate without uploading
./bin/konfig validate --service payment-service --file config.json --format json

# Delete a config
./bin/konfig delete --id payment-service-v1

# Check rollout status
./bin/konfig status --id payment-service-v1

# Rollback to previous version
./bin/konfig rollback --service payment-service --version 1
```

## Database Schema

Managed via migrations in `db/migrations/` (000-008):

| Table | Purpose |
|-------|---------|
| `config_metadata` | Service name, version, format, timestamps |
| `config_data` | Actual config content and hashes (FK to metadata) |
| `rollout_state` | Gradual rollout tracking |
| `service_instances` | Connected client instances |
| `audit_log` | All config actions with JSONB details |
| `health_checks` | Service health status |
| `validation_schemas` | Registered validation schemas |
| `validation_rules` | Custom validation rules per service |
| `validation_history` | Validation result audit trail |

## Development

### Local Development

```bash
# Start infrastructure
make infra-up

# Build services locally
make services-local

# Run with local configs (localhost addresses)
./bin/api-service config/api-service-local.yml
./bin/validation-service config/validation-service.yml
./bin/distribution-service config/distribution-service-local.yml
```

### Docker Development

```bash
# Start everything
make infra-up && make services

# View logs
docker compose logs -f api-service
docker compose logs -f validation-service

# Rebuild after code changes
make services
```

### Dev Container (for building inside Linux)

```bash
make dev-up            # Start dev container
make dev-shell         # Enter interactive shell
make dev-build         # Build inside container
make dev-sdk           # Build client SDK inside container
make dev-cache-test    # Build disk cache test binary inside container
```

## Client SDK

The C++ client SDK (`libconfigclient`) lets services subscribe to real-time config updates with automatic reconnection and disk caching.

```cpp
#include "configclient/config_client.h"

configservice::ConfigClient client("distribution-service:8082", "payment-service");

client.OnConfigUpdate([](const configservice::ConfigData& config) {
    // called immediately from disk cache on startup, then on every live update
    std::cout << "Config v" << config.version() << ": " << config.content() << std::endl;
});

client.Start();   // loads disk cache, then connects
// ...
client.Stop();
```

### Disk Cache

On every update the SDK writes a binary cache to `~/.konfig/cache/<service>.cache`. On the next startup the cached config is served **before** the gRPC connection is established — so services have a valid config immediately, even when the distribution service is temporarily unreachable.

| Scenario | Behaviour |
|----------|-----------|
| First start, server up | No cache — waits for live stream |
| Restart, server up | Serves cache immediately, updates live |
| Restart, server down | Serves cache, retries every 5 s |
| Corrupted cache | Discards file, falls back to live config |

### Testing the disk cache (`bin/cache_test`)

```bash
# Build (inside dev container)
make dev-sdk && make dev-cache-test

# Step 1 — first run (no cache)
./bin/cache_test distribution-service:8082 payment-service

# Step 2 — upload a config (separate terminal)
./bin/konfig upload --service payment-service --file examples/configs/valid-config.json --format json
# cache_test prints: >>> CONFIG UPDATE <<<  and [DiskCache] Saved config v1 ...

# Step 3 — restart: cache loads before gRPC connects
./bin/cache_test distribution-service:8082 payment-service
# prints: Cache readable: YES (v1)  then  >>> CONFIG UPDATE <<<  then  Connected

# Step 4 — offline fallback
make services-down
./bin/cache_test distribution-service:8082 payment-service
# prints: Cache readable: YES (v1)  then  Disconnected / Reconnecting in 5 seconds...

# Step 5 — corruption: bad cache is discarded
echo "garbage" > ~/.konfig/cache/payment-service.cache
./bin/cache_test distribution-service:8082 payment-service
# prints: Cache readable: NO (corrupt — will be discarded)  then  Connected
```

> **Note on hostnames:** Use `distribution-service:8082` from inside the dev container. Use `localhost:8082` when running the binary directly on the host machine.

### Linking against the SDK

```makefile
# Static link
$(CXX) $(CXXFLAGS) $(INCLUDES) $(LDFLAGS) my_service.cpp \
    lib/libconfigclient.a $(SDK_LIBS) -o bin/my_service
```

Headers are in `include/configclient/`. Libraries are in `lib/` after `make sdk`.

## Connection Info

| Service | From Host | From Container |
|---------|-----------|----------------|
| PostgreSQL | `localhost:5432` | `postgres:5432` |
| Redis | `localhost:6379` | `redis:6379` |
| Kafka | `localhost:9093` | `kafka:9092` |
| StatsD | `localhost:9125` | `statsd-exporter:9125` |
| API Service | `localhost:8081` | `api-service:8081` |
| Validation Service | `localhost:8083` | `validation-service:8083` |
| Distribution Service | `localhost:8082` | `distribution-service:8082` |

**Database credentials:** `configuser` / `configpass` / `configservice`

## Service Documentation

- [API Service](src/api-service/README.md)
- [Distribution Service](src/distribution-service/README.md)
- [Validation Service](src/validation-service/README.md)

## License

MIT
