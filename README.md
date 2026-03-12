# Konfig - Dynamic Configuration Service

A distributed configuration management system built with C++17, gRPC, Kafka, and modern observability tools. Upload, validate, and roll out configuration changes to thousands of services in real-time.

## Quick Start

```bash
# 1. Start infrastructure (PostgreSQL, Redis, Kafka, monitoring)
make dev

# 2. Build and start all service containers
make services

# 3. Build the CLI tool
make cli

# 4. Upload a config
./bin/configctl upload my-service --file config.yaml --format yaml

# 5. Roll it out to all instances
./bin/configctl rollout my-service-v1 --strategy ALL_AT_ONCE
```

## Architecture

```
                          ┌──────────────────┐
                          │ CLI (configctl)  │
                          │    Go / gRPC     │
                          └────────┬─────────┘
                                   │ :8081
                          ┌────────▼─────────┐
                          │   API Service    │
                          │  Upload/Rollout  │
                          └────────┬─────────┘
                                   │ Kafka
                    ┌──────────────┼──────────────┐
                    │              │              │
             ┌──────▼──────┐ ┌────▼─────┐ ┌─────▼──────┐
             │  PostgreSQL  │ │  Redis   │ │Distribution│
             │  (config,    │ │ (cache)  │ │  Service   │
             │   rollouts)  │ └──────────┘ │   :8082    │
             └─────────────┘              └─────┬──────┘
                                                │ gRPC stream
                                         ┌──────▼──────┐
                                         │  Client SDK │
                                         │  (C++ lib)  │
                                         └─────────────┘
```

### Services

| Service | Port | Description |
|---------|------|-------------|
| **API Service** | 8081 | Config upload, retrieval, deletion, rollout management |
| **Distribution Service** | 8082 | Real-time config push to clients via gRPC streaming |
| **Validation Service** | 8083 | Config syntax/schema/rule validation (JSON & YAML) |

### Infrastructure

| Component | Port | Purpose |
|-----------|------|---------|
| PostgreSQL | 5432 | Config metadata, data, audit logs, rollout state |
| Redis | 6379 | Caching layer |
| Kafka | 9092/9093 | Event streaming (rollout triggers, config notifications) |
| Prometheus | 9090 | Metrics collection |
| Grafana | 3000 | Metrics dashboards (admin/admin) |
| Kafka UI | 8080 | Topic and message inspection |
| pgAdmin | 5050 | Database management |
| StatsD Exporter | 9125 | Metrics bridge to Prometheus |

## Project Structure

```
Konfig/
├── cmd/configctl/           # CLI tool (Go)
├── config/                  # Service configuration files
├── db/migrations/           # PostgreSQL schema migrations
├── docker/                  # Dockerfiles and init scripts
├── examples/                # Example configs and client
├── include/                 # C++ headers
├── internal/commands/       # CLI command implementations
├── proto/                   # Protocol Buffer definitions
├── src/
│   ├── api-service/
│   ├── distribution-service/
│   ├── validation-service/
│   ├── client-sdk/
│   └── common/
├── docker-compose.yml
└── Makefile
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
make verify            # Health check all services
```

### Build

```bash
make proto             # Generate protobuf/gRPC code
make sdk               # Build client SDK (shared + static)
make cache-test        # Build disk cache test binary (bin/cache_test)
make cli               # Build CLI tool (bin/configctl)
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
```

### Dev Container

```bash
make dev-up            # Start dev container
make dev-shell         # Enter interactive shell
make dev-build         # Build inside container
make dev-sdk           # Build client SDK inside container
```

## Local Development

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

## Client SDK

The C++ client SDK (`libconfigclient`) lets services subscribe to real-time config updates with automatic reconnection, configurable heartbeating, and disk caching. See [Client SDK](src/client-sdk/README.md) for the full reference including heartbeat configuration and advanced options.

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
./bin/configctl upload payment-service --file examples/configs/valid-config.json --format json
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
| API Service | `localhost:8081` | `api-service:8081` |
| Validation Service | `localhost:8083` | `validation-service:8083` |
| Distribution Service | `localhost:8082` | `distribution-service:8082` |

**Database credentials:** `configuser` / `configpass` / `configservice`

## Documentation

- [CLI Reference](cmd/configctl/README.md) — all commands, flags, rollout strategies
- [Client SDK](src/client-sdk/README.md) — C++ SDK usage, heartbeat config, disk cache
- [API Service](src/api-service/README.md) — gRPC API, upload flow, components
- [Distribution Service](src/distribution-service/README.md) — streaming, rollout execution, heartbeat monitor
- [Validation Service](src/validation-service/README.md) — schema validation, rules

## License

MIT
