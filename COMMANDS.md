# Dynamic Configuration Service - Command Reference

Quick reference for all available `make` commands in the project.

---

## 📋 Table of Contents

- [Infrastructure Management](#️-infrastructure-management)
- [Development Container](#-development-container)
- [Build Commands](#-build-commands-localinside-container)
- [Database & Tools](#️-database--tools)
- [Testing](#-testing)
- [Cleanup](#-cleanup)

---

## 🏗️ Infrastructure Management

### `make help`

Display all available commands with descriptions.

```bash
make help
```

### `make dev` / `make setup`

Complete project setup - creates directory structure and starts all infrastructure services.

```bash
make dev
```

**What it does:**

- Creates project directories
- Starts Docker containers (PostgreSQL, Redis, Kafka, etc.)
- Waits for all services to be ready
- Verifies setup and displays access URLs

**When to use:** First time setup or after a complete cleanup

---

### `make infra-up`

Start all infrastructure services in Docker.

```bash
make infra-up
```

**Services started:**

- PostgreSQL (database)
- Redis (cache)
- Kafka + Zookeeper (messaging)
- Prometheus (metrics)
- Grafana (visualization)
- StatsD Exporter (metrics collection)
- Kafka UI (management)
- pgAdmin (database management)

**When to use:** Daily development when you want to start services

---

### `make infra-down`

Stop all infrastructure services.

```bash
make infra-down
```

**What it does:**

- Stops all Docker containers
- Keeps volumes (data persists)

**When to use:** End of work session

---

### `make infra-restart`

Restart all infrastructure services.

```bash
make infra-restart
```

**When to use:** Services are misbehaving or after config changes

---

### `make infra-logs`

Follow logs from all infrastructure services in real-time.

```bash
make infra-logs
```

**Useful for:**

- Debugging connection issues
- Monitoring service health
- Watching for errors

**Exit:** Press `Ctrl+C`

---

### `make infra-ps`

Show status of all running containers.

```bash
make infra-ps
```

**Output shows:**

- Container names
- Status (Up/Down)
- Ports
- Health status

---

### `make verify`

Verify all services are healthy and display connection information.

```bash
make verify
```

**What it checks:**

- Creates Kafka topics
- Displays database tables
- Shows sample data
- Lists all access URLs and credentials

**When to use:** After `make dev` or when troubleshooting

---

## 🐳 Development Container

The development container provides a consistent Linux build environment with all dependencies pre-installed.

### `make dev-up`

Start the development container.

```bash
make dev-up
```

**What it does:**

- Builds dev container image (first time only)
- Starts container in background
- Mounts project directory to `/workspace`
- Connects to Docker network with all services

**When to use:** Before starting development work

---

### `make dev-down`

Stop the development container.

```bash
make dev-down
```

---

### `make dev-shell`

Enter an interactive shell in the development container.

```bash
make dev-shell
```

**You'll be in:** `/workspace` (your project root)

**Available tools:**

- g++, cmake, make
- protoc, grpc_cpp_plugin
- psql, redis-cli
- All project dependencies

**Exit:** Type `exit` or press `Ctrl+D`

---

### `make dev-build`

Build the entire project inside the development container.

```bash
make dev-build
```

**Builds:**

- Proto files (gRPC/Protobuf)
- Client SDK (shared + static libraries)
- Distribution Service
- Example clients

**When to use:** After code changes, from the host machine

---

### `make dev-proto`

Generate only the protobuf/gRPC files.

```bash
make dev-proto
```

**Output:** `build/*.pb.cc`, `build/*.grpc.pb.cc`

---

### `make dev-sdk`

Build only the client SDK.

```bash
make dev-sdk
```

**Output:**

- `lib/libconfigclient.so` (shared library)
- `lib/libconfigclient.a` (static library)

---

### `make dev-example`

Build the example client application.

```bash
make dev-example
```

**Output:** `bin/simple_client`

---

### `make dev-test-statsd`

Build and run the StatsD metrics test.

```bash
make dev-test-statsd
```

**What it does:**

- Builds the StatsD test
- Runs it (sends metrics for 10 seconds)
- Displays metrics URLs

**View results:**

- Prometheus: <http://localhost:9090>
- Grafana: <http://localhost:3000>
- StatsD Exporter: <http://localhost:9102/metrics>

---

### `make dev-clean`

Clean build artifacts in the development container.

```bash
make dev-clean
```

**Removes:**

- `build/` directory
- `bin/` directory
- `lib/` directory

---

## 🔨 Build Commands (Local/Inside Container)

These commands work when you're inside the dev container or on a compatible Linux/Mac system.

### `make all`

Build everything - proto files, SDK, and services.

```bash
make all
```

**Equivalent to:** `make proto distribution-service sdk`

---

### `make proto`

Generate protobuf and gRPC code from `.proto` files.

```bash
make proto
```

**Input:** `proto/*.proto`
**Output:** `build/*.pb.{cc,h}`, `build/*.grpc.pb.{cc,h}`

---

### `make sdk`

Build the client SDK (both shared and static libraries).

```bash
make sdk
```

**Output:**

- `lib/libconfigclient.so`
- `lib/libconfigclient.a`

**Dependencies:** Requires `make proto` first

---

### `make distribution-service`

Build the Distribution Service (C++ gRPC service).

```bash
make distribution-service
```

**What it does:**

- Compiles all distribution service source files
- Links with protobuf, gRPC, PostgreSQL, Redis, Kafka libraries
- Creates `bin/distribution-service` executable

**Output:** `bin/distribution-service` (1.0MB)

**Dependencies:** Requires `make proto` first

**Configuration:** Uses `config/distribution-service.yml` (Docker) or `config/distribution-service-local.yml` (host)

**Run locally:**

```bash
./bin/distribution-service ./config/distribution-service-local.yml
```

**Run in Docker:**

```bash
docker exec config-dev /workspace/bin/distribution-service /workspace/config/distribution-service.yml
```

---

### `make services`

Placeholder for building all services (currently a no-op).

```bash
make services
```

**Note:** Use `make distribution-service` to build the distribution service specifically.

**Coming soon:**

- API Service
- Validation Service

---

### `make example`

Build the example client application.

```bash
make example
```

**Output:** `bin/simple_client`

**Run it:**

```bash
./bin/simple_client
# or specify server
./bin/simple_client localhost:8082 my-service
```

---

### `make test-statsd`

Build and run the StatsD metrics test.

```bash
make test-statsd
```

**Duration:** ~10 seconds
**Metrics sent:** Counters, gauges, timings, histograms

---

### `make clean`

Remove all build artifacts.

```bash
make clean
```

**Removes:**

- `build/`
- `bin/`
- `lib/`

**Keeps:** Source code, proto files, config files

---

### `make rebuild`

Clean and rebuild everything.

```bash
make rebuild
```

**Equivalent to:** `make clean && make all`

---

## 🗄️ Database & Tools

### `make db-shell`

Open PostgreSQL interactive shell.

```bash
make db-shell
```

**Connection:**

- Database: `configservice`
- User: `configuser`
- Password: `configpass`

**Example commands:**

```sql
\dt                    -- List tables
\d config_metadata     -- Describe table
SELECT * FROM config_metadata;
\q                     -- Quit
```

---

### `make redis-shell`

Open Redis CLI.

```bash
make redis-shell
```

**Example commands:**

```redis
PING                   -- Test connection
KEYS *                 -- List all keys
GET mykey              -- Get value
FLUSHALL               -- Clear all data
EXIT                   -- Quit
```

---

### `make kafka-topics`

List all Kafka topics.

```bash
make kafka-topics
```

**Expected topics:**

- `config.updates`
- `config.health`
- `config.audit`

---

### `make kafka-ui`

Open Kafka UI in browser.

```bash
make kafka-ui
```

**Opens:** <http://localhost:8080>

**Features:**

- View topics and messages
- Monitor consumer groups
- Manage topics

---

### `make grafana`

Open Grafana in browser.

```bash
make grafana
```

**Opens:** <http://localhost:3000>
**Login:** admin / admin

---

### `make pgadmin`

Open pgAdmin in browser.

```bash
make pgadmin
```

**Opens:** <http://localhost:5050>
**Login:** `admin@config.local` / admin

**First time setup:**

1. Add server
2. Host: `postgres`
3. Database: `configservice`
4. User: `configuser`
5. Password: `configpass`

---

## 🧪 Testing

### `make test`

Run all tests (placeholder - to be implemented).

```bash
make test
```

---

### StatsD Metrics Testing

Run StatsD metrics test.

```bash
make test-statsd
```

**What it tests:**

- StatsD client functionality
- Metric types (counters, gauges, timings)
- Network connectivity
- Prometheus integration

**Check results:**

```bash
curl http://localhost:9102/metrics | grep test_
```

---

## 🧹 Cleanup

### `make cleanup`

Complete cleanup - stop all services and remove all data.

```bash
make cleanup
```

**⚠️ WARNING:** This removes all Docker volumes (data will be lost!)

**Removes:**

- All containers
- All volumes (databases, caches)
- Build artifacts

**Use when:**

- Starting fresh
- Freeing disk space
- Resetting to clean state

---

## 🎯 Common Workflows

### First Time Setup

```bash
make dev              # Setup everything
make dev-up           # Start dev container
make dev-build        # Build project
```

### Daily Development

```bash
make infra-up         # Start infrastructure
make dev-up           # Start dev container
make dev-shell        # Enter container

# Inside container:
make clean
make all
./bin/distribution-service    # Terminal 1
./bin/simple_client           # Terminal 2
```

### Testing StatsD Metrics

```bash
make dev-test-statsd          # Run test
make grafana                  # Open Grafana
# Create dashboards with metrics
```

### Debugging Services

```bash
make infra-logs               # Watch all logs
make db-shell                 # Check database
make redis-shell              # Check cache
make kafka-topics             # Check messaging
```

### Complete Reset

```bash
make cleanup                  # Remove everything
make dev                      # Start fresh
```

---

## 📊 Access URLs

After running `make verify`, you can access:

| Service | URL | Credentials |
|---------|-----|-------------|
| Grafana | <http://localhost:3000> | admin / admin |
| Prometheus | <http://localhost:9090> | - |
| Kafka UI | <http://localhost:8080> | - |
| pgAdmin | <http://localhost:5050> | `admin@example.com` / admin |
| StatsD Metrics | <http://localhost:9102/metrics> | - |

---

## 🔌 Connection Info

### PostgreSQL

```text
Host: localhost (from host) / postgres (from container)
Port: 5432
Database: configservice
User: configuser
Password: configpass
```

### Redis

```text
Host: localhost (from host) / redis (from container)
Port: 6379
```

### Kafka

```text
Bootstrap servers: localhost:9093 (from host) / kafka:9092 (from container)
```

### StatsD

```text
Host: localhost (from host) / statsd-exporter (from container)
Port: 9125 (UDP)
```

---

## 💡 Tips

### View Make Targets

```bash
make help
```

### Check Service Status

```bash
make infra-ps
docker ps
```

### View Logs for Specific Service

```bash
docker compose logs -f postgres
docker compose logs -f grafana
```

### Rebuild Single Service

```bash
# In dev container
make clean
make sdk              # Just SDK
make services         # Just services
```

### Run Commands in Dev Container Without Entering

```bash
make dev-build        # From host
# or
docker compose exec dev-container make all
```

---

## 🆘 Troubleshooting

### "No such file or directory"

```bash
make create-dirs      # Create directory structure
```

### "Connection refused"

```bash
make infra-restart    # Restart services
make verify           # Check health
```

### "Port already in use"

```bash
make infra-down       # Stop services
docker ps             # Check for conflicts
```

### Build fails

```bash
make clean            # Clean build
make dev-build        # Rebuild in container
```

### Dev container not starting

```bash
docker compose down
docker compose build dev-container
make dev-up
```

---

## 📚 Related Documentation

- [README.md](README.md) - Project overview
- [Architecture Documentation](docs/ARCHITECTURE.md) - System design
- [API Documentation](docs/API.md) - Service APIs
- [Deployment Guide](docs/DEPLOYMENT.md) - Production deployment

---

**Need help?** Run `make help` or check the specific command documentation above!
