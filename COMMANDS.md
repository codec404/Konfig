# Konfig - Command Reference

Quick reference for all available `make` commands.

---

## Table of Contents

- [Infrastructure Management](#infrastructure-management)
- [Service Containers](#service-containers)
- [Build Commands](#build-commands)
- [CLI Tool](#cli-tool)
- [Development Container](#development-container)
- [Database & Tools](#database--tools)
- [Testing](#testing)
- [Cleanup](#cleanup)
- [Common Workflows](#common-workflows)

---

## Infrastructure Management

### `make dev` / `make setup`

Complete project setup - creates directories and starts all infrastructure.

```bash
make dev
```

**What it does:**
- Creates project directories
- Starts Docker containers (PostgreSQL, Redis, Kafka, Prometheus, Grafana, etc.)
- Waits for all services to be healthy
- Verifies setup and displays access URLs

---

### `make infra-up`

Start all infrastructure services in Docker.

```bash
make infra-up
```

**Services started:** PostgreSQL, Redis, Kafka + Zookeeper, Prometheus, Grafana, StatsD Exporter, Kafka UI, pgAdmin

---

### `make infra-down`

Stop all infrastructure services. Keeps volumes (data persists).

```bash
make infra-down
```

---

### `make infra-restart`

Restart all infrastructure services.

```bash
make infra-restart
```

---

### `make infra-logs`

Follow logs from all infrastructure services in real-time. Exit with `Ctrl+C`.

```bash
make infra-logs
```

---

### `make infra-ps`

Show status of all running containers.

```bash
make infra-ps
```

---

### `make verify`

Verify all services are healthy and display connection information.

```bash
make verify
```

---

## Service Containers

### `make services`

Build Docker images and start all three service containers (API, Distribution, Validation).

```bash
make services
```

**What it does:**
- Builds each service using its multi-stage Dockerfile
- Starts containers with proper dependencies (waits for healthy postgres, redis, kafka)
- Services are accessible on ports 8081, 8082, 8083

**Equivalent to:** `docker compose up --build -d api-service distribution-service validation-service`

---

### `make services-down`

Stop all service containers.

```bash
make services-down
```

---

### `make services-local`

Build all service binaries locally (no Docker). Output goes to `bin/`.

```bash
make services-local
```

**Output:**
- `bin/api-service`
- `bin/distribution-service`
- `bin/validation-service`

---

### Individual Service Containers

Rebuild and restart a single service:

```bash
docker compose up --build -d api-service
docker compose up --build -d distribution-service
docker compose up --build -d validation-service
```

View logs for a single service:

```bash
docker compose logs -f api-service
docker compose logs -f distribution-service
docker compose logs -f validation-service
```

---

## Build Commands

These work on the host machine (macOS/Linux) or inside the dev container.

### `make all`

Build everything locally - proto files, SDK, services, and CLI.

```bash
make all
```

**Equivalent to:** `make proto sdk services-local cli`

---

### `make proto`

Generate protobuf and gRPC code from `.proto` files.

```bash
make proto
```

**Input:** `proto/*.proto`
**Output:** `build/*.pb.{cc,h}`, `build/*.grpc.pb.{cc,h}`

---

### `make api-service`

Build the API Service binary locally.

```bash
make api-service
```

**Output:** `bin/api-service`

**Run locally:**
```bash
./bin/api-service config/api-service-local.yml
```

---

### `make distribution-service`

Build the Distribution Service binary locally.

```bash
make distribution-service
```

**Output:** `bin/distribution-service`

**Run locally:**
```bash
./bin/distribution-service config/distribution-service-local.yml
```

---

### `make validation-service`

Build the Validation Service binary locally.

```bash
make validation-service
```

**Output:** `bin/validation-service`

**Run locally:**
```bash
./bin/validation-service config/validation-service.yml
```

---

### `make sdk`

Build the client SDK (shared and static libraries).

```bash
make sdk
```

**Output:**
- `lib/libconfigclient.so` (shared library)
- `lib/libconfigclient.a` (static library)

---

### `make cli`

Build the CLI tool (`konfig`).

```bash
make cli
```

**Output:** `bin/konfig`

**Requires:** Go toolchain and generated Go proto files

---

### `make example`

Build the example client application.

```bash
make example
```

**Output:** `bin/simple_client`

---

### `make clean`

Remove all build artifacts (`build/`, `bin/`, `lib/`).

```bash
make clean
```

---

### `make rebuild`

Clean and rebuild everything.

```bash
make rebuild
```

---

### `make format`

Format all C++ source files using clang-format.

```bash
make format
```

---

### `make format-check`

Check C++ formatting without modifying files.

```bash
make format-check
```

---

## CLI Tool

The `konfig` CLI communicates with the API Service via gRPC.

### Upload Configuration

```bash
./bin/konfig upload --service payment-service --file config.json --format json
```

### Get Configuration

```bash
./bin/konfig get --id payment-service-v1
```

### List Configurations

```bash
./bin/konfig list --service payment-service
```

### Validate Configuration

```bash
./bin/konfig validate --service payment-service --file config.json --format json
```

### Delete Configuration

```bash
./bin/konfig delete --id payment-service-v1
```

### Rollout Status

```bash
./bin/konfig status --id payment-service-v1
```

### Rollback

```bash
./bin/konfig rollback --service payment-service --version 1
```

### Global Flags

| Flag | Description | Default |
|------|-------------|---------|
| `-s, --server` | API server address | `localhost:8081` |
| `-o, --output` | Output format: table, json, yaml | `table` |
| `-v, --verbose` | Verbose output | `false` |

### Version

```bash
./bin/konfig version
```

---

## Development Container

The dev container provides a Linux build environment with all C++ dependencies pre-installed.

### `make dev-up`

Start the development container.

```bash
make dev-up
```

---

### `make dev-down`

Stop the development container.

```bash
make dev-down
```

---

### `make dev-shell`

Enter an interactive shell in the dev container (`/workspace`).

```bash
make dev-shell
```

Available tools: g++, cmake, make, protoc, grpc_cpp_plugin, psql, redis-cli

---

### `make dev-build`

Build the entire project inside the dev container.

```bash
make dev-build
```

---

### `make dev-proto` / `make dev-sdk` / `make dev-example`

Build individual components inside the dev container.

```bash
make dev-proto
make dev-sdk
make dev-example
```

---

### `make dev-clean`

Clean build artifacts inside the dev container.

```bash
make dev-clean
```

---

### `make dev-test-statsd`

Build and run StatsD metrics test inside the dev container.

```bash
make dev-test-statsd
```

---

## Database & Tools

### `make db-shell`

Open PostgreSQL interactive shell.

```bash
make db-shell
```

**Useful commands:**
```sql
\dt                        -- List tables
\d config_metadata         -- Describe table
SELECT * FROM config_metadata;
SELECT * FROM audit_log ORDER BY created_at DESC LIMIT 10;
\q                         -- Quit
```

---

### `make redis-shell`

Open Redis CLI.

```bash
make redis-shell
```

**Useful commands:**
```
PING                       -- Test connection
KEYS *                     -- List all keys
KEYS validation:*          -- List validation cache keys
FLUSHDB                    -- Clear current database
```

---

### `make kafka-topics`

List all Kafka topics.

```bash
make kafka-topics
```

**Expected topics:** `config.events`, `config.updates`, `config.health`, `config.audit`

---

### `make kafka-ui`

Open Kafka UI in browser at http://localhost:8080.

```bash
make kafka-ui
```

---

### `make grafana`

Open Grafana in browser at http://localhost:3000. Login: admin / admin.

```bash
make grafana
```

---

### `make pgadmin`

Open pgAdmin in browser at http://localhost:5050. Login: `admin@example.com` / admin.

```bash
make pgadmin
```

**First time:** Add server with host `postgres`, database `configservice`, user `configuser`, password `configpass`.

---

## Testing

### `make test`

Run all tests.

```bash
make test
```

---

### `make test-statsd`

Build and run StatsD metrics test (sends metrics for 10 seconds).

```bash
make test-statsd
```

**Check results:**
```bash
curl http://localhost:9102/metrics | grep test_
```

---

## Cleanup

### `make cleanup`

Complete cleanup - stop all services and remove all Docker volumes.

```bash
make cleanup
```

**WARNING:** This removes all data (databases, caches, metrics).

---

## Common Workflows

### First Time Setup

```bash
make dev                   # Start infrastructure
make services              # Build and start service containers
make cli                   # Build CLI
./bin/konfig list --service test  # Verify
```

### Daily Development

```bash
make infra-up              # Start infrastructure
make services              # Build and start containers
docker compose logs -f api-service  # Watch logs

# After code changes:
make services              # Rebuilds and restarts
```

### Local Development (no Docker for services)

```bash
make infra-up              # Start infrastructure
make services-local        # Build binaries
./bin/validation-service config/validation-service.yml &
./bin/api-service config/api-service-local.yml &
./bin/konfig upload --service test --file config.json
```

### Testing with CLI

```bash
# Upload
./bin/konfig upload --service payment-service --file config.json --format json

# Verify
./bin/konfig list --service payment-service
./bin/konfig get --id payment-service-v1

# Validate only (no upload)
./bin/konfig validate --service payment-service --file config.json --format json
```

### Debugging

```bash
docker compose logs -f api-service          # Service logs
make db-shell                               # Check database
make redis-shell                            # Check cache
make kafka-topics                           # Check messaging
docker compose ps                           # Container status
```

### Complete Reset

```bash
make cleanup               # Remove everything
make dev                   # Start fresh
```

---

## Access URLs

| Service | URL | Credentials |
|---------|-----|-------------|
| API Service | `localhost:8081` (gRPC) | - |
| Distribution Service | `localhost:8082` (gRPC) | - |
| Validation Service | `localhost:8083` (gRPC) | - |
| Grafana | http://localhost:3000 | admin / admin |
| Prometheus | http://localhost:9090 | - |
| Kafka UI | http://localhost:8080 | - |
| pgAdmin | http://localhost:5050 | `admin@example.com` / admin |
| StatsD Metrics | http://localhost:9102/metrics | - |

## Connection Info

| Service | From Host | From Container |
|---------|-----------|----------------|
| PostgreSQL | `localhost:5432` | `postgres:5432` |
| Redis | `localhost:6379` | `redis:6379` |
| Kafka | `localhost:9093` | `kafka:9092` |
| StatsD | `localhost:9125` | `statsd-exporter:9125` |

**Database credentials:** `configuser` / `configpass` / `configservice`
