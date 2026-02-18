# Validation Service

The Validation Service validates configuration content before it is stored. It supports JSON and YAML formats, custom validation rules per service, schema validation, and caches results in Redis.

## Overview

Called by the API Service during config uploads, the Validation Service performs multi-layer validation: syntax checking, schema validation, custom rule evaluation (required fields, value ranges), and returns detailed errors and warnings.

### Key Features

- JSON syntax validation with trailing comma detection
- YAML syntax validation
- Custom validation rules per service (required fields, value ranges)
- Dotted path support for nested field rules (e.g., `database.host`)
- Schema registration and validation
- Redis-based result caching
- Validation history audit trail in PostgreSQL

## gRPC API

Defined in `proto/validation.proto` as `ValidationService`:

| RPC | Description |
|-----|-------------|
| `ValidateConfig` | Validate config content against syntax, schema, and rules |
| `RegisterSchema` | Register a validation schema for a service |
| `GetSchema` | Retrieve a registered schema |
| `ListSchemas` | List all schemas for a service |

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                 Validation Service                     │
│                                                        │
│  ┌───────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  gRPC     │  │  Validators  │  │   Database    │  │
│  │  Server   │──│  JSON / YAML │──│   Manager     │  │
│  │  (:8083)  │  │              │  │ (PostgreSQL)  │  │
│  └───────────┘  └──────────────┘  └───────────────┘  │
│       │                                │               │
│  ┌────▼──────┐              ┌─────────▼──────┐       │
│  │  Redis    │              │  StatsD Client │       │
│  │  Cache    │              │  (Metrics)     │       │
│  └───────────┘              └────────────────┘       │
└──────────────────────────────────────────────────────┘
```

## Validation Pipeline

When `ValidateConfig` is called:

1. **Cache check** - Look up `validation:<service>:<content_hash>` in Redis
2. **Size validation** - Reject configs exceeding max size (default 1MB)
3. **Syntax validation** - Format-specific parsing:
   - JSON: bracket matching, string escaping, trailing comma detection
   - YAML: YAML parsing and structure validation
4. **Schema validation** - If a schema is registered for the service
5. **Custom rules** - Loaded from `validation_rules` table:
   - `required` rules: Check field presence (supports dotted paths like `database.host`)
   - `range` rules: Check numeric values are within min/max bounds
6. **Cache result** - Store valid/invalid in Redis with TTL
7. **Record history** - Write to `validation_history` table
8. **Return response** - Errors, warnings, and valid/invalid status

## Components

### `validation_service.cpp`

Core gRPC service with validation orchestration:
- `ValidateConfig()` - Main validation pipeline
- `RegisterSchema()` - Store schema in database
- `GetSchema()` / `ListSchemas()` - Schema retrieval
- `ApplyCustomRules()` - Evaluates per-service rules from database
- `ValidateSize()` - Config size limit check
- `ComputeHash()` - Content hashing for cache keys

### `json_validator.cpp`

JSON-specific validation:
- `ValidateSyntax()` - Bracket depth tracking, string escape handling, trailing comma detection
- `ValidateSchema()` - JSON Schema validation (placeholder for library integration)
- `ValidateRanges()` - Numeric range checks for known fields
- `ValidateRequired()` - Required field presence checks

### `yaml_validator.cpp`

YAML-specific validation:
- `ValidateSyntax()` - YAML parsing and structure validation
- `ValidateSchema()` - YAML schema validation

### `database_manager.cpp`

PostgreSQL operations:
- `GetRulesForService()` - Load custom rules from `validation_rules`
- `RecordValidation()` - Write to `validation_history` (uses `NOW()` for timestamps)
- `StoreSchema()` / `GetSchema()` / `ListSchemas()` - Schema CRUD

### `config.cpp`

YAML configuration loading:
- `server` - Port, max connections
- `postgres` - Database connection
- `redis` - Cache host, port, TTL
- `statsd` - Metrics endpoint
- `validation` - Max config size, timeout, caching toggle, strict mode

## Custom Validation Rules

Rules are stored in the `validation_rules` table and applied per service:

```sql
-- Required field rule (supports dotted paths)
INSERT INTO validation_rules (service_name, rule_type, rule_target, rule_config)
VALUES ('payment-service', 'required', 'database.host', '{}');

-- Range rule with min/max
INSERT INTO validation_rules (service_name, rule_type, rule_target, rule_config)
VALUES ('payment-service', 'range', 'settings.max_connections',
        '{"min": 1, "max": 1000}');
```

The `findKey` helper supports both JSON (`"key"`) and YAML (`key:`) key formats when searching content.

## Configuration

```yaml
server:
  port: 8083
  max_connections: 500
postgres:
  host: postgres       # or localhost for local dev
  port: 5432
redis:
  host: redis
  port: 6379
  cache_ttl: 600       # seconds
statsd:
  host: statsd-exporter
  port: 9125
  prefix: validation
validation:
  max_config_size: 1048576  # 1MB
  timeout_seconds: 5
  enable_caching: true
  strict_mode: false
```

## Building & Running

```bash
# Build locally
make validation-service

# Run locally
./bin/validation-service config/validation-service.yml

# Build and run in Docker
make services
docker compose logs -f validation-service
```

## Caching

Results are cached in Redis with key format: `validation:<service>:<content_hash>`

- Default TTL: 600 seconds (10 minutes)
- Cache is checked before running validation pipeline
- Same content for the same service returns cached result

To clear the cache during development:
```bash
make redis-shell
# Then: FLUSHDB
```

## Metrics (StatsD)

- `validation.validate.request` - Total validation requests
- `validation.validate.cache_hit` / `cache_miss` - Cache efficiency
- `validation.validate.pass` / `fail` - Validation results
- `validation.validate.duration` - Validation latency

## Code Structure

```
src/validation-service/
├── main.cpp              # Entry point, gRPC server setup
├── validation_service.cpp # Validation pipeline orchestration
├── json_validator.cpp    # JSON syntax and structure validation
├── yaml_validator.cpp    # YAML validation
├── database_manager.cpp  # PostgreSQL operations
└── config.cpp            # YAML config loading

include/validation_service/
├── validation_service.h
├── json_validator.h
├── yaml_validator.h
├── database_manager.h
└── config.h
```

## Related

- [Proto Definition](../../proto/validation.proto)
- [Database Schema](../../db/migrations/005_validation_tables.sql)
- [API Service](../api-service/README.md) (calls this service)
- [Commands Reference](../../COMMANDS.md)
