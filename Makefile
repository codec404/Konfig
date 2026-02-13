# Dynamic Configuration Service Makefile

.PHONY: help setup infra-up infra-down infra-restart infra-logs infra-ps \
        verify cleanup proto services sdk test clean install all rebuild \
        db-shell redis-shell kafka-topics kafka-ui grafana pgadmin wait-for-services dev

# Colors
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[0;33m
BLUE := \033[0;34m
NC := \033[0m

# Docker Compose command (plugin vs standalone)
COMPOSE := $(shell docker compose version >/dev/null 2>&1 && echo "docker compose" || echo "docker-compose")

#==============================================================================
# HELP
#==============================================================================

help:
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(BLUE)  Dynamic Configuration Service - Makefile$(NC)"
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""
	@echo "$(GREEN)Infrastructure:$(NC)"
	@echo "  make dev             - Complete setup (create dirs + start infrastructure)"
	@echo "  make setup           - Alias for make dev"
	@echo "  make infra-up        - Start all infrastructure services (Docker)"
	@echo "  make infra-down      - Stop all infrastructure services"
	@echo "  make infra-restart   - Restart infrastructure services"
	@echo "  make infra-logs      - Follow infrastructure logs"
	@echo "  make infra-ps        - Show running containers"
	@echo "  make verify          - Verify all services are healthy"
	@echo "  make cleanup         - Complete cleanup (stop + remove volumes)"
	@echo ""
	@echo "$(GREEN)Development:$(NC)"
	@echo "  make proto           - Generate protobuf and gRPC code"
	@echo "  make services        - Build all C++ services"
	@echo "  make sdk             - Build client SDK"
	@echo "  make all             - Build everything"
	@echo "  make test            - Run tests"
	@echo "  make clean           - Remove build artifacts"
	@echo "  make rebuild         - Clean and rebuild"
	@echo ""
	@echo "$(GREEN)Tools:$(NC)"
	@echo "  make db-shell        - Open PostgreSQL shell"
	@echo "  make redis-shell     - Open Redis CLI"
	@echo "  make kafka-topics    - List Kafka topics"
	@echo "  make kafka-ui        - Open Kafka UI (http://localhost:8080)"
	@echo "  make grafana         - Open Grafana (http://localhost:3000)"
	@echo "  make pgadmin         - Open pgAdmin (http://localhost:5050)"
	@echo ""

#==============================================================================
# DIRECTORY SETUP
#==============================================================================

create-dirs:
	@echo "$(YELLOW)Creating directory structure...$(NC)"
	@mkdir -p proto
	@mkdir -p src/{common,client-sdk,api-service,distribution-service,validation-service}
	@mkdir -p include/configclient
	@mkdir -p tests
	@mkdir -p build bin lib
	@mkdir -p cmd/configctl
	@echo "$(GREEN)✓ Directories created$(NC)"

dev: setup

#==============================================================================
# INFRASTRUCTURE
#==============================================================================

setup: create-dirs
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(BLUE)  Starting Dynamic Configuration Service Setup$(NC)"
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""
	@$(MAKE) -s infra-up
	@$(MAKE) -s wait-for-services
	@$(MAKE) -s verify

infra-up:
	@echo "$(YELLOW)Starting infrastructure services...$(NC)"
	@$(COMPOSE) up -d
	@echo "$(GREEN)✓ Infrastructure services started$(NC)"

infra-down:
	@echo "$(YELLOW)Stopping infrastructure services...$(NC)"
	@$(COMPOSE) down
	@echo "$(GREEN)✓ Infrastructure services stopped$(NC)"

infra-restart:
	@echo "$(YELLOW)Restarting infrastructure services...$(NC)"
	@$(COMPOSE) restart
	@echo "$(GREEN)✓ Infrastructure services restarted$(NC)"

infra-logs:
	@$(COMPOSE) logs -f

infra-ps:
	@$(COMPOSE) ps

#==============================================================================
# WAIT FOR SERVICES
#==============================================================================

wait-for-services:
	@echo ""
	@echo "$(YELLOW)Waiting for services to be ready...$(NC)"
	@echo ""
	@echo -n "  PostgreSQL: "
	@count=0; \
	while [ $$count -lt 60 ]; do \
		if $(COMPOSE) exec -T postgres pg_isready -U configuser -d configservice > /dev/null 2>&1; then \
			echo "$(GREEN)✓$(NC)"; \
			break; \
		fi; \
		echo -n "."; \
		sleep 2; \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 60 ]; then \
		echo " $(RED)✗ (timeout)$(NC)"; \
		echo "$(RED)PostgreSQL failed to start. Check logs with: make infra-logs$(NC)"; \
		exit 1; \
	fi
	@echo -n "  Redis: "
	@count=0; \
	while [ $$count -lt 30 ]; do \
		if $(COMPOSE) exec -T redis redis-cli ping 2>/dev/null | grep -q PONG; then \
			echo "$(GREEN)✓$(NC)"; \
			break; \
		fi; \
		echo -n "."; \
		sleep 2; \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 30 ]; then \
		echo " $(RED)✗ (timeout)$(NC)"; \
		exit 1; \
	fi
	@echo -n "  Zookeeper: "
	@count=0; \
	while [ $$count -lt 30 ]; do \
		if $(COMPOSE) exec -T zookeeper nc -z localhost 2181 > /dev/null 2>&1; then \
			echo "$(GREEN)✓$(NC)"; \
			break; \
		fi; \
		echo -n "."; \
		sleep 2; \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 30 ]; then \
		echo " $(RED)✗ (timeout)$(NC)"; \
		exit 1; \
	fi
	@echo -n "  Kafka: "
	@count=0; \
	while [ $$count -lt 60 ]; do \
		if $(COMPOSE) exec -T kafka kafka-broker-api-versions --bootstrap-server localhost:9092 > /dev/null 2>&1; then \
			echo "$(GREEN)✓$(NC)"; \
			break; \
		fi; \
		echo -n "."; \
		sleep 2; \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 60 ]; then \
		echo " $(RED)✗ (timeout)$(NC)"; \
		exit 1; \
	fi
	@echo ""
	@echo "$(GREEN)✓ All services are ready!$(NC)"

#==============================================================================
# VERIFICATION
#==============================================================================

verify:
	@echo ""
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(BLUE)  Verifying Services$(NC)"
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""
	@echo "$(YELLOW)Creating Kafka topics...$(NC)"
	@$(COMPOSE) exec -T kafka kafka-topics --create --if-not-exists \
		--bootstrap-server localhost:9092 --topic config.updates \
		--partitions 3 --replication-factor 1 > /dev/null 2>&1 || true
	@$(COMPOSE) exec -T kafka kafka-topics --create --if-not-exists \
		--bootstrap-server localhost:9092 --topic config.health \
		--partitions 3 --replication-factor 1 > /dev/null 2>&1 || true
	@$(COMPOSE) exec -T kafka kafka-topics --create --if-not-exists \
		--bootstrap-server localhost:9092 --topic config.audit \
		--partitions 3 --replication-factor 1 > /dev/null 2>&1 || true
	@echo "  - config.updates"
	@echo "  - config.health"
	@echo "  - config.audit"
	@echo "$(GREEN)✓ Kafka topics created$(NC)"
	@echo ""
	@echo "$(YELLOW)Database Tables:$(NC)"
	@$(COMPOSE) exec -T postgres psql -U configuser -d configservice -c "\dt" 2>/dev/null | \
		grep -E "config_|audit_|rollout_|service_|health_" | awk '{print "  - " $$3}'
	@echo ""
	@echo "$(YELLOW)Sample Data:$(NC)"
	@$(COMPOSE) exec -T postgres psql -U configuser -d configservice \
		-c "SELECT config_id, service_name, version FROM config_metadata;" 2>/dev/null | \
		sed '1,2d;$$d' | sed 's/^/  /'
	@echo ""
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(GREEN)  ✓ Setup Complete!$(NC)"
	@echo "$(BLUE)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""
	@echo "$(YELLOW)Access URLs:$(NC)"
	@echo "  Kafka UI:    $(BLUE)http://localhost:8080$(NC)"
	@echo "  Grafana:     $(BLUE)http://localhost:3000$(NC) (admin/admin)"
	@echo "  Prometheus:  $(BLUE)http://localhost:9090$(NC)"
	@echo "  pgAdmin:     $(BLUE)http://localhost:5050$(NC) (admin@config.local/admin)"
	@echo ""
	@echo "$(YELLOW)Connection Info:$(NC)"
	@echo "  PostgreSQL:  localhost:5432 (configuser/configpass/configservice)"
	@echo "  Redis:       localhost:6379"
	@echo "  Kafka:       localhost:9093"
	@echo ""
	@echo "$(YELLOW)Quick Commands:$(NC)"
	@echo "  Database shell:  make db-shell"
	@echo "  Redis shell:     make redis-shell"
	@echo "  View logs:       make infra-logs"
	@echo "  Stop services:   make infra-down"
	@echo ""

#==============================================================================
# DATABASE & TOOLS
#==============================================================================

db-shell:
	@$(COMPOSE) exec postgres psql -U configuser -d configservice

redis-shell:
	@$(COMPOSE) exec redis redis-cli

kafka-topics:
	@echo "$(YELLOW)Kafka Topics:$(NC)"
	@$(COMPOSE) exec kafka kafka-topics --list --bootstrap-server localhost:9092

kafka-ui:
	@echo "$(YELLOW)Opening Kafka UI...$(NC)"
	@open http://localhost:8080 2>/dev/null || xdg-open http://localhost:8080 2>/dev/null || \
		echo "  Open $(BLUE)http://localhost:8080$(NC) in your browser"

grafana:
	@echo "$(YELLOW)Opening Grafana...$(NC)"
	@open http://localhost:3000 2>/dev/null || xdg-open http://localhost:3000 2>/dev/null || \
		echo "  Open $(BLUE)http://localhost:3000$(NC) in your browser"

pgadmin:
	@echo "$(YELLOW)Opening pgAdmin...$(NC)"
	@open http://localhost:5050 2>/dev/null || xdg-open http://localhost:5050 2>/dev/null || \
		echo "  Open $(BLUE)http://localhost:5050$(NC) in your browser"

#==============================================================================
# BUILD CONFIGURATION
#==============================================================================

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
LDFLAGS := -pthread

BUILD_DIR := build
BIN_DIR := bin
LIB_DIR := lib
INCLUDE_DIR := include
SRC_DIR := src
PROTO_DIR := proto

INCLUDES := -I$(INCLUDE_DIR) -I$(BUILD_DIR) -I/usr/local/include
LIBS := -lprotobuf -lgrpc++ -lgrpc++_reflection -lpqxx -lpq -lhiredis -lrdkafka++ \
        -lfmt -lspdlog -lpthread -lyaml-cpp

PROTOC := protoc
GRPC_CPP_PLUGIN_PATH ?= $(shell which grpc_cpp_plugin)

# Proto files
PROTO_FILES := $(wildcard $(PROTO_DIR)/*.proto)
PROTO_SRCS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.pb.cc,$(PROTO_FILES))
PROTO_HDRS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.pb.h,$(PROTO_FILES))
GRPC_SRCS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.grpc.pb.cc,$(PROTO_FILES))
GRPC_HDRS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.grpc.pb.h,$(PROTO_FILES))

#==============================================================================
# BUILD TARGETS
#==============================================================================

all: proto services sdk

proto: $(PROTO_SRCS) $(PROTO_HDRS) $(GRPC_SRCS) $(GRPC_HDRS)
	@echo "$(GREEN)✓ Proto files generated$(NC)"

$(BUILD_DIR)/%.pb.cc $(BUILD_DIR)/%.pb.h: $(PROTO_DIR)/%.proto | $(BUILD_DIR)
	@echo "$(YELLOW)Generating protobuf for $<...$(NC)"
	@cd $(PROTO_DIR) && $(PROTOC) --proto_path=. --cpp_out=../$(BUILD_DIR) $(notdir $<)

$(BUILD_DIR)/%.grpc.pb.cc $(BUILD_DIR)/%.grpc.pb.h: $(PROTO_DIR)/%.proto | $(BUILD_DIR)
	@echo "$(YELLOW)Generating gRPC for $<...$(NC)"
	@cd $(PROTO_DIR) && $(PROTOC) --proto_path=. --grpc_out=../$(BUILD_DIR) \
		--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $(notdir $<)

$(BUILD_DIR) $(BIN_DIR) $(LIB_DIR):
	@mkdir -p $@

services: | $(BIN_DIR)
	@echo "$(YELLOW)Building services...$(NC)"
	@echo "$(BLUE)  Note: Service implementation pending$(NC)"

sdk: | $(LIB_DIR)
	@echo "$(YELLOW)Building SDK...$(NC)"
	@echo "$(BLUE)  Note: SDK implementation pending$(NC)"

test:
	@echo "$(YELLOW)Running tests...$(NC)"
	@echo "$(BLUE)  Note: Tests pending$(NC)"

clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	@echo "$(GREEN)✓ Build artifacts cleaned$(NC)"

rebuild: clean all

#==============================================================================
# CLEANUP
#==============================================================================

cleanup:
	@echo "$(YELLOW)Performing complete cleanup...$(NC)"
	@docker compose down -v
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	@echo "$(GREEN)✓ Cleanup complete$(NC)"

install:
	@echo "$(BLUE)Note: Install target will be implemented after services are built$(NC)"

.DEFAULT_GOAL := help