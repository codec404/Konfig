# configctl — CLI Reference

`configctl` is the command-line tool for managing configurations in Konfig.

```bash
make cli          # Build to ./bin/configctl
```

## Global Options

```
--server, -s    API server address (default: localhost:8081)
--output, -o    Output format: table | json | yaml (default: table)
--verbose, -v   Verbose output
```

The server address can also be set via the `KONFIG_SERVER` environment variable:

```bash
export KONFIG_SERVER=api-service:8081
```

---

## upload

Upload a new config version for a service.

```bash
configctl upload <service-name> --file <path> --format <json|yaml>
```

Each upload increments the version counter and creates a new config ID in the form `<service>-v<N>`. The config is validated before storage.

```bash
./bin/configctl upload payment-service --file config.yaml --format yaml
# → Created: payment-service-v3
```

---

## rollout

Start a rollout to distribute a config to connected instances.

```bash
configctl rollout <config-id> --strategy <strategy> [--percentage <n>]
```

| Flag | Default | Description |
|------|---------|-------------|
| `--strategy` | `ALL_AT_ONCE` | `ALL_AT_ONCE`, `CANARY`, or `PERCENTAGE` |
| `--percentage` | `100` | Target percentage for `PERCENTAGE` strategy |
| `--server` | `localhost:8081` | API server address |

### Strategies

| Strategy | Behaviour |
|----------|-----------|
| `ALL_AT_ONCE` | Pushes to all connected instances immediately. Completes when all instances receive the config. |
| `CANARY` | Pushes to ~10% of instances and stays `IN_PROGRESS`. Use `promote` to push to the rest after verification. |
| `PERCENTAGE` | Pushes to the specified percentage of instances (1–100). |

```bash
# Push to everything at once
./bin/configctl rollout payment-service-v3 --strategy ALL_AT_ONCE

# Canary — 10% first
./bin/configctl rollout payment-service-v3 --strategy CANARY

# Half of instances
./bin/configctl rollout payment-service-v3 --strategy PERCENTAGE --percentage 50
```

---

## promote

Promote a `CANARY` rollout that is `IN_PROGRESS` to all remaining instances.

```bash
configctl promote <config-id>
```

Validates that the rollout is `CANARY` and `IN_PROGRESS` before proceeding. Use this after verifying that the canary instances look healthy.

```bash
./bin/configctl promote payment-service-v3
# → Pushing to all instances → COMPLETED
```

---

## rollback

Rollback a service to a previous config version.

```bash
configctl rollback <service-name> --to-version <n>
```

| Flag | Description |
|------|-------------|
| `--to-version` | Target version number (`0` = previous version) |

```bash
# Rollback to version 2
./bin/configctl rollback payment-service --to-version 2

# Rollback to previous version
./bin/configctl rollback payment-service --to-version 0
```

The rollback creates a new config version copying the content from the target version, then triggers an `ALL_AT_ONCE` rollout.

---

## status

Show the rollout status of a config.

```bash
configctl status <config-id>
```

```bash
./bin/configctl status payment-service-v3
```

Output:
```
Rollout Status
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Config ID:     payment-service-v3
Strategy:      CANARY
Progress:      10% / 10%
Status:        IN_PROGRESS
Started:       2024-01-15T10:00:00Z

Instances:
INSTANCE ID                    VERSION    STATUS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
instance-abc                   3          UPDATED
instance-xyz                   2          PENDING
```

---

## get

Retrieve a config by ID.

```bash
configctl get <config-id>
```

```bash
./bin/configctl get payment-service-v3
```

---

## list

List all config versions for a service.

```bash
configctl list <service-name>
```

```bash
./bin/configctl list payment-service
```

---

## validate

Validate a config file without uploading it.

```bash
configctl validate <service-name> --file <path> --format <json|yaml>
```

```bash
./bin/configctl validate payment-service --file config.yaml --format yaml
```

---

## delete

Delete a config by ID.

```bash
configctl delete <config-id>
```

```bash
./bin/configctl delete payment-service-v1
```

---

## version

Print the CLI version.

```bash
./bin/configctl version
```

---

## Typical Workflow

```bash
# 1. Upload new config
./bin/configctl upload payment-service --file config.yaml --format yaml
# → payment-service-v5

# 2. Canary rollout to 10%
./bin/configctl rollout payment-service-v5 --strategy CANARY

# 3. Check status
./bin/configctl status payment-service-v5
# → IN_PROGRESS, 10%

# 4a. Looks good — promote to all
./bin/configctl promote payment-service-v5

# 4b. Something wrong — rollback
./bin/configctl rollback payment-service --to-version 0
```
