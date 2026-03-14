#!/usr/bin/env bash
# setup-vps.sh — one-time Oracle Cloud Ubuntu 22.04 server setup
# Run as: bash scripts/setup-vps.sh
set -euo pipefail

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[setup]${NC} $*"; }
warn()  { echo -e "${YELLOW}[setup]${NC} $*"; }

# ── 1. System update ──────────────────────────────────────────────────────
info "Updating system packages..."
sudo apt-get update -qq
sudo apt-get upgrade -y -qq

# ── 2. Docker Engine ──────────────────────────────────────────────────────
if ! command -v docker &>/dev/null; then
  info "Installing Docker..."
  sudo apt-get install -y -qq ca-certificates curl gnupg lsb-release
  sudo install -m 0755 -d /etc/apt/keyrings
  curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
    | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
  sudo chmod a+r /etc/apt/keyrings/docker.gpg
  echo \
    "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" \
    | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
  sudo apt-get update -qq
  sudo apt-get install -y -qq docker-ce docker-ce-cli containerd.io docker-compose-plugin
  sudo usermod -aG docker "$USER"
  info "Docker installed. NOTE: log out and back in for group membership to take effect."
else
  info "Docker already installed — skipping."
fi

# ── 3. Git ────────────────────────────────────────────────────────────────
sudo apt-get install -y -qq git

# ── 4. Firewall ───────────────────────────────────────────────────────────
info "Configuring ufw firewall..."
sudo ufw allow OpenSSH
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw --force enable
sudo ufw status

# ── 5. Clone repos ────────────────────────────────────────────────────────
REPO_ROOT="$HOME/konfig"
mkdir -p "$REPO_ROOT"
cd "$REPO_ROOT"

clone_or_pull() {
  local dir="$1" url="$2"
  if [ -d "$dir/.git" ]; then
    info "$dir already cloned — pulling latest..."
    git -C "$dir" pull --ff-only
  else
    info "Cloning $url → $dir..."
    git clone "$url" "$dir"
  fi
}

# Replace these URLs with your actual GitHub remote URLs
clone_or_pull "Konfig"              "https://github.com/YOUR_ORG/Konfig.git"
clone_or_pull "Konfig-Web-Frontend" "https://github.com/YOUR_ORG/Konfig-Web-Frontend.git"
clone_or_pull "Konfig-Web-Backend"  "https://github.com/YOUR_ORG/Konfig-Web-Backend.git"

# ── 6. .env ───────────────────────────────────────────────────────────────
ENV_FILE="$REPO_ROOT/Konfig/.env"
if [ ! -f "$ENV_FILE" ]; then
  warn ".env not found — creating from template. Edit it before starting services."
  cat > "$ENV_FILE" <<'EOF'
# PostgreSQL (Konfig C++ services)
POSTGRES_DB=configservice
POSTGRES_USER=configuser
POSTGRES_PASSWORD=CHANGE_ME_POSTGRES

# Redis
REDIS_PORT=6379

# Web auth DB
WEB_POSTGRES_DB=konfig_auth
WEB_POSTGRES_USER=webuser
WEB_POSTGRES_PASSWORD=CHANGE_ME_WEB_POSTGRES

# JWT — must be a long random string
JWT_SECRET=CHANGE_ME_LONG_RANDOM_JWT_SECRET

# Public URL of the server (used for CORS + Google OAuth redirect)
# Use your domain if you have one, otherwise http://<OCI_PUBLIC_IP>
APP_URL=http://CHANGE_ME_YOUR_IP_OR_DOMAIN
SECURE_COOKIE=false

# Super-admin seeded on first start
SUPER_ADMIN_NAME=Super Admin
SUPER_ADMIN_EMAIL=admin@konfig.local
SUPER_ADMIN_PASSWORD=CHANGE_ME_ADMIN_PASSWORD

# Google OAuth (optional — leave blank to disable)
GOOGLE_CLIENT_ID=
GOOGLE_CLIENT_SECRET=

# Grafana
GRAFANA_ADMIN_USER=admin
GRAFANA_ADMIN_PASSWORD=CHANGE_ME_GRAFANA
EOF
  warn "Edit $ENV_FILE with your real values, then run scripts/deploy.sh"
else
  info ".env already exists — skipping."
fi

info "Setup complete."
info "Next: edit Konfig/.env, then run:  bash Konfig/scripts/deploy.sh"
