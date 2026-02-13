# Service Connection Details

## pgAdmin

- URL: <http://localhost:5050>
- Login: `admin@config.local` / admin

### Add PostgreSQL Server in pgAdmin

- **Host**: postgres
- **Port**: 5432
- **Database**: configservice
- **Username**: configuser
- **Password**: configpass

## Direct PostgreSQL Connection

```bash
# From host machine
psql -h localhost -p 5432 -U configuser -d configservice
# Password: configpass

# From Docker
make db-shell
```

## Redis

```bash
# From host machine
redis-cli -h localhost -p 6379

# From Docker
make redis-shell
```

## Kafka

- Bootstrap Server: localhost:9093
- UI: <http://localhost:8080>

## Grafana

- URL: <http://localhost:3000>
- Login: admin / admin

## Prometheus

- URL: <http://localhost:9090>
