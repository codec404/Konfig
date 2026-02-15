# Dynamic Configuration Service

A distributed configuration management system built with C++, gRPC, Kafka, and modern observability tools.

## Quick Start

```bash
# 1. Complete setup (creates directories and starts all infrastructure)
make dev

# 2. Verify everything is running
make verify

# 3. Access web interfaces
make kafka-ui    # Kafka UI at http://localhost:8080
make grafana     # Grafana at http://localhost:3000 (admin/admin)
make pgadmin     # pgAdmin at http://localhost:5050
```

## Architecture

### Services

- **API Service**: Configuration upload and management (gRPC)
- **Distribution Service**: Push configurations to clients (gRPC streaming)
- **Validation Service**: Configuration validation and schema checking
- **Client SDK**: C++ library for services to receive configurations

### Infrastructure Components

- **PostgreSQL**: Metadata and configuration storage
- **Redis**: Caching and state management
- **Kafka**: Event streaming and audit logging
- **Prometheus + Grafana**: Metrics and monitoring
- **Kafka UI**: Kafka topic and message inspection
- **pgAdmin**: PostgreSQL database management

## Makefile Commands

### Infrastructure

```bash
make dev             # Complete dev setup
make setup           # Alias for make dev
make infra-up        # Start infrastructure
make infra-down      # Stop infrastructure
make infra-restart   # Restart infrastructure
make infra-logs      # View logs
make verify          # Verify health
```

### Development

```bash
make proto           # Generate proto code
make services        # Build all services
make sdk             # Build client SDK
make all             # Build everything
make clean           # Clean build artifacts
```

### Database & Tools

```bash
make db-shell        # PostgreSQL shell
make redis-shell     # Redis CLI
make kafka-topics    # List topics
```

📚 For a complete command reference, see [COMMANDS.md](COMMANDS.md)

## Connection Info

| Service | URL/Port | Credentials |
|---------|----------|-------------|
| PostgreSQL | `localhost:5432` | user: `configuser` / pass: `configpass` |
| Redis | `localhost:6379` | - |
| Kafka | `localhost:9093` | - |
| Kafka UI | <http://localhost:8080> | - |
| Grafana | <http://localhost:3000> | admin / admin |
| Prometheus | <http://localhost:9090> | - |
| pgAdmin | <http://localhost:5050> | `admin@example.com` / admin |

Note: If you use the pgAdmin container, register the Postgres server with host `postgres` (not `localhost`).

## Development Setup

See individual service READMEs:

- [API Service](src/api-service/README.md)
- [Distribution Service](src/distribution-service/README.md)
- [Client SDK](src/client-sdk/README.md)

## License

```text
MIT
```

## Directory Structure

```text
dynamic-config-service/
├── docker-compose.yml          # Docker Compose configuration
├── .env                        # Environment variables
├── .gitignore                  # Git ignore rules
├── Makefile                    # Build and infrastructure automation
├── README.md                   # Project documentation
├── docker/
│   ├── postgres/
│   │   └── init.sql           # PostgreSQL initialization
│   ├── grafana/
│   │   └── provisioning/      # Grafana datasources and dashboards
│   └── services/              # Dockerfiles for services
│       ├── api-service.Dockerfile
│       ├── distribution-service.Dockerfile
│       └── validation-service.Dockerfile
└── prometheus/
    └── prometheus.yml         # Prometheus configuration
```
