#!/usr/bin/env bash
# deploy.sh — pull latest and restart all services in production
# Run from the Konfig-Web/Konfig directory, or let the Makefile call it.
set -euo pipefail

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[deploy]${NC} $*"; }
warn()  { echo -e "${YELLOW}[deploy]${NC} $*"; }
error() { echo -e "${RED}[deploy]${NC} $*"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KONFIG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$KONFIG_DIR/.." && pwd)"

# ── Validate .env ─────────────────────────────────────────────────────────
ENV_FILE="$KONFIG_DIR/.env"
[ -f "$ENV_FILE" ] || error ".env not found at $ENV_FILE — run setup-vps.sh first."

# Warn if placeholder values remain
if grep -q "CHANGE_ME" "$ENV_FILE"; then
  warn "WARNING: .env still contains CHANGE_ME placeholder values."
  warn "Edit $ENV_FILE before continuing."
  read -rp "Continue anyway? [y/N] " reply
  [[ "$reply" =~ ^[Yy]$ ]] || exit 1
fi

# ── Pull latest code ──────────────────────────────────────────────────────
info "Pulling latest from all repos..."
for dir in Konfig Konfig-Web-Frontend Konfig-Web-Backend; do
  target="$ROOT_DIR/$dir"
  if [ -d "$target/.git" ]; then
    info "  $dir..."
    git -C "$target" pull --ff-only
  else
    warn "  $dir — not a git repo, skipping pull."
  fi
done

# ── Auto-run pending DB migrations ───────────────────────────────────────
info "Checking for pending database migrations..."
MIGRATIONS_DIR="$KONFIG_DIR/db/migrations"

# Load DB credentials from .env
DB_USER=$(grep -E '^POSTGRES_USER=' "$ENV_FILE" | cut -d= -f2 | tr -d '"' || echo "konfig")
DB_NAME=$(grep -E '^POSTGRES_DB=' "$ENV_FILE" | cut -d= -f2 | tr -d '"' || echo "konfig")

# Ensure schema_migrations table exists
docker compose exec -T postgres psql -U "$DB_USER" -d "$DB_NAME" \
  -f /dev/stdin < "$MIGRATIONS_DIR/000_migration_tracker.sql" &>/dev/null || true

# Fetch already-applied migration numbers
APPLIED=$(docker compose exec -T postgres psql -U "$DB_USER" -d "$DB_NAME" -t -A \
  -c "SELECT migration_number FROM schema_migrations WHERE status = 'success' ORDER BY migration_number;" \
  2>/dev/null || echo "")

PENDING=0
for file in $(ls "$MIGRATIONS_DIR"/*.sql | sort); do
  filename=$(basename "$file")
  # Extract leading number (e.g. 003 from 003_client_instances.sql)
  num=$(echo "$filename" | grep -oE '^[0-9]+' | sed 's/^0*//')
  [ -z "$num" ] && continue
  # Skip if already applied
  echo "$APPLIED" | grep -qx "$num" && continue

  info "  Applying migration: $filename"
  START_MS=$(date +%s%3N)
  docker compose exec -T postgres psql -U "$DB_USER" -d "$DB_NAME" \
    -f /dev/stdin < "$file" \
    && STATUS="success" || STATUS="failed"
  END_MS=$(date +%s%3N)
  ELAPSED=$((END_MS - START_MS))

  # Record in tracker
  docker compose exec -T postgres psql -U "$DB_USER" -d "$DB_NAME" -c \
    "INSERT INTO schema_migrations (migration_number, migration_name, status, execution_time_ms)
     VALUES ($num, '$filename', '$STATUS', $ELAPSED)
     ON CONFLICT (migration_number) DO UPDATE SET status='$STATUS', execution_time_ms=$ELAPSED;" \
     &>/dev/null || true

  [ "$STATUS" = "failed" ] && error "Migration $filename failed — aborting deploy."
  PENDING=$((PENDING + 1))
done

[ "$PENDING" -eq 0 ] && info "  No pending migrations." || info "  Applied $PENDING migration(s)."

# ── Build and start ───────────────────────────────────────────────────────
info "Building and starting services (production mode)..."
cd "$KONFIG_DIR"
docker compose \
  -f docker-compose.yml \
  -f docker-compose.prod.yml \
  up --build -d

# ── Health check ─────────────────────────────────────────────────────────
info "Waiting for web-backend to be ready..."
for i in $(seq 1 30); do
  if docker compose exec -T web-backend wget -qO- http://localhost:8090/api/auth/me &>/dev/null; then
    break
  fi
  # 401 is fine — it means the server is up
  STATUS=$(docker compose exec -T web-backend wget -S -qO- http://localhost:8090/api/auth/me 2>&1 | grep "HTTP/" | awk '{print $2}' || true)
  if [ "$STATUS" = "401" ]; then
    break
  fi
  sleep 2
done

info "Running containers:"
docker compose -f docker-compose.yml -f docker-compose.prod.yml ps --format "table {{.Name}}\t{{.Status}}\t{{.Ports}}"

info ""
info "Deployment complete."
info "Frontend: http://$(curl -s ifconfig.me 2>/dev/null || echo '<YOUR_IP>')/"
