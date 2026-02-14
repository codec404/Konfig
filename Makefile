# Dynamic Configuration Service Makefile

.PHONY: help setup infra-up infra-down infra-restart infra-logs infra-ps \
        verify cleanup proto services sdk test clean install all rebuild \
        db-shell redis-shell kafka-topics kafka-ui grafana pgadmin wait-for-services dev \
    		format format-check \
        example test-statsd \
        proto-native sdk-native example-native all-native \
        dev-up dev-down dev-shell dev-build dev-proto dev-sdk dev-example dev-clean dev-test-statsd

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
	@echo "$(GREEN)Development Container (Recommended):$(NC)"
	@echo "  make dev-up          - Start development container"
	@echo "  make dev-shell       - Enter development container shell"
	@echo "  make dev-build       - Build everything in dev container"
	@echo "  make dev-proto       - Generate proto files in dev container"
	@echo "  make dev-sdk         - Build SDK in dev container"
	@echo "  make dev-example     - Build example in dev container"
	@echo "  make dev-test-statsd - Test StatsD in dev container"
	@echo "  make dev-clean       - Clean build artifacts in dev container"
	@echo "  make dev-down        - Stop development container"
	@echo ""
	@echo "$(GREEN)Local Development (Mac/Linux):$(NC)"
	@echo "  make proto           - Generate protobuf and gRPC code"
	@echo "  make services        - Build all C++ services"
	@echo "  make sdk             - Build client SDK"
	@echo "  make all             - Build everything"
	@echo "  make example         - Build example client"
	@echo "  make test-statsd     - Build and run StatsD test"
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
	@mkdir -p examples
	@mkdir -p build bin lib
	@mkdir -p cmd/configctl
	@mkdir -p scripts
	@echo "$(GREEN)✓ Directories created$(NC)"

dev: setup

#==============================================================================
# CODE FORMATTING
#==============================================================================

# Format all source files
format:
	@echo "$(YELLOW)Formatting code...$(NC)"
	@find src include examples -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format -i {} +
	@echo "$(GREEN)✓ Code formatted$(NC)"

# Check formatting without modifying files
format-check:
	@echo "$(YELLOW)Checking code formatting...$(NC)"
	@if find src include examples -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format --dry-run -Werror {} + 2>&1 | grep -q "error:"; then \
		echo "$(RED)✗ Code formatting issues found. Run 'make format' to fix.$(NC)"; \
		exit 1; \
	else \
		echo "$(GREEN)✓ Code formatting OK$(NC)"; \
	fi

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
	@$(COMPOSE) up -d postgres redis zookeeper kafka prometheus grafana pgadmin kafka-ui statsd-exporter
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
	@echo "  pgAdmin:     $(BLUE)http://localhost:5050$(NC) (admin@example.com/admin)"
	@echo "  StatsD:      $(BLUE)http://localhost:9102/metrics$(NC)"
	@echo ""
	@echo "$(YELLOW)Connection Info:$(NC)"
	@echo "  PostgreSQL:  localhost:5432 (configuser/configpass/configservice)"
	@echo "  Redis:       localhost:6379"
	@echo "  Kafka:       localhost:9093"
	@echo "  StatsD:      localhost:9125 (UDP)"
	@echo ""
	@echo "$(YELLOW)Quick Commands:$(NC)"
	@echo "  Database shell:  make db-shell"
	@echo "  Redis shell:     make redis-shell"
	@echo "  Test StatsD:     make test-statsd"
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
# DEVELOPMENT CONTAINER
#==============================================================================

# Start development container
dev-up:
	@echo "$(YELLOW)Starting development container...$(NC)"
	@$(COMPOSE) up -d dev-container
	@echo "$(GREEN)✓ Development container is running$(NC)"
	@echo "$(YELLOW)Use 'make dev-shell' to enter the container$(NC)"

# Stop development container
dev-down:
	@echo "$(YELLOW)Stopping development container...$(NC)"
	@$(COMPOSE) stop dev-container
	@echo "$(GREEN)✓ Development container stopped$(NC)"

# Enter development container shell
dev-shell:
	@echo "$(YELLOW)Entering development container...$(NC)"
	@$(COMPOSE) exec dev-container /bin/bash

# Build everything in dev container
dev-build:
	@echo "$(YELLOW)Building in development container...$(NC)"
	@$(COMPOSE) exec dev-container make all-native
	@echo "$(GREEN)✓ Build complete$(NC)"

# Generate proto files in dev container
dev-proto:
	@echo "$(YELLOW)Generating proto files in development container...$(NC)"
	@$(COMPOSE) exec dev-container make proto-native
	@echo "$(GREEN)✓ Proto files generated$(NC)"

# Build SDK in dev container
dev-sdk:
	@echo "$(YELLOW)Building SDK in development container...$(NC)"
	@$(COMPOSE) exec dev-container make sdk-native
	@echo "$(GREEN)✓ SDK built$(NC)"

# Build example in dev container
dev-example:
	@echo "$(YELLOW)Building example in development container...$(NC)"
	@$(COMPOSE) exec dev-container make example-native
	@echo "$(GREEN)✓ Example built$(NC)"

# Clean in dev container
dev-clean:
	@echo "$(YELLOW)Cleaning in development container...$(NC)"
	@$(COMPOSE) exec dev-container make clean
	@echo "$(GREEN)✓ Cleaned$(NC)"

# Test StatsD in dev container
dev-test-statsd:
	@echo "$(YELLOW)Testing StatsD in development container...$(NC)"
	@$(COMPOSE) exec dev-container make test-statsd

#==============================================================================
# BUILD CONFIGURATION
#==============================================================================

# Auto-detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    CXX := clang++
    INCLUDES_BASE := -I/opt/homebrew/include -I/usr/local/include
    LDFLAGS_BASE := -L/opt/homebrew/lib -L/usr/local/lib
else
    # Linux (Docker)
    CXX := g++
    INCLUDES_BASE := -I/usr/local/include
    LDFLAGS_BASE := -L/usr/local/lib
endif

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g -Wno-deprecated-declarations
LDFLAGS := -pthread $(LDFLAGS_BASE)

BUILD_DIR := build
BIN_DIR := bin
LIB_DIR := lib
INCLUDE_DIR := include
SRC_DIR := src
PROTO_DIR := proto

# Use pkg-config to get correct paths
PROTO_CFLAGS := $(shell pkg-config --cflags protobuf grpc++ 2>/dev/null || echo "")
PROTO_LIBS := $(shell pkg-config --libs protobuf grpc++ 2>/dev/null || echo "-lprotobuf -lgrpc++")

INCLUDES := -I$(INCLUDE_DIR) -I$(BUILD_DIR) $(PROTO_CFLAGS) $(INCLUDES_BASE)

# Minimal libs for SDK (only protobuf and grpc)
SDK_LIBS := $(PROTO_LIBS) -lgrpc++_reflection

# Full libs for services (will use later)
SERVICE_LIBS := $(SDK_LIBS) -lpqxx -lpq -lhiredis -lrdkafka++ \
                -lfmt -lspdlog -lyaml-cpp

LIBS := $(SERVICE_LIBS)

PROTOC := protoc
GRPC_CPP_PLUGIN_PATH ?= $(shell which grpc_cpp_plugin)

# Proto files
PROTO_FILES := $(wildcard $(PROTO_DIR)/*.proto)
PROTO_SRCS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.pb.cc,$(PROTO_FILES))
PROTO_HDRS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.pb.h,$(PROTO_FILES))
GRPC_SRCS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.grpc.pb.cc,$(PROTO_FILES))
GRPC_HDRS := $(patsubst $(PROTO_DIR)/%.proto,$(BUILD_DIR)/%.grpc.pb.h,$(PROTO_FILES))

# Common source files (includes StatsD)
COMMON_SRCS := $(wildcard $(SRC_DIR)/common/*.cpp)
COMMON_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

# SDK source files
SDK_SRCS := $(wildcard $(SRC_DIR)/client-sdk/*.cpp)
SDK_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SDK_SRCS))

# Proto object files
PROTO_OBJS := $(PROTO_SRCS:.cc=.o) $(GRPC_SRCS:.cc=.o)

# SDK library targets
SDK_SHARED := $(LIB_DIR)/libconfigclient.so
SDK_STATIC := $(LIB_DIR)/libconfigclient.a

# StatsD standalone object (for testing without full SDK)
STATSD_OBJ := $(BUILD_DIR)/common/statsd_client.o

#==============================================================================
# BUILD TARGETS
#==============================================================================

all: proto services sdk

proto: $(PROTO_SRCS) $(PROTO_HDRS) $(GRPC_SRCS) $(GRPC_HDRS)
	@echo "$(GREEN)✓ Proto files generated$(NC)"

# Generate protobuf files
$(BUILD_DIR)/%.pb.cc $(BUILD_DIR)/%.pb.h: $(PROTO_DIR)/%.proto | $(BUILD_DIR)
	@echo "$(YELLOW)Generating protobuf for $<...$(NC)"
	@cd $(PROTO_DIR) && $(PROTOC) --proto_path=. --cpp_out=../$(BUILD_DIR) $(notdir $<)

# Generate gRPC files
$(BUILD_DIR)/%.grpc.pb.cc $(BUILD_DIR)/%.grpc.pb.h: $(PROTO_DIR)/%.proto | $(BUILD_DIR)
	@echo "$(YELLOW)Generating gRPC for $<...$(NC)"
	@cd $(PROTO_DIR) && $(PROTOC) --proto_path=. --grpc_out=../$(BUILD_DIR) \
		--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $(notdir $<)

# Create directories
$(BUILD_DIR) $(BIN_DIR) $(LIB_DIR):
	@mkdir -p $@

$(BUILD_DIR)/common $(BUILD_DIR)/client-sdk:
	@mkdir -p $@

# Compile common source files (StatsD, utilities)
$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.cpp | $(BUILD_DIR)/common
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Compile SDK source files
$(BUILD_DIR)/client-sdk/%.o: $(SRC_DIR)/client-sdk/%.cpp | $(BUILD_DIR)/client-sdk
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Compile proto objects
$(BUILD_DIR)/%.pb.o: $(BUILD_DIR)/%.pb.cc
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

$(BUILD_DIR)/%.grpc.pb.o: $(BUILD_DIR)/%.grpc.pb.cc
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Build shared SDK library (use minimal libs)
$(SDK_SHARED): $(SDK_OBJS) $(COMMON_OBJS) $(PROTO_OBJS) | $(LIB_DIR)
	@echo "$(YELLOW)Building shared SDK library...$(NC)"
	@$(CXX) -shared $(LDFLAGS) $^ $(SDK_LIBS) -o $@
	@echo "$(GREEN)✓ Built $(SDK_SHARED)$(NC)"

# Build static SDK library (no linking needed for static)
$(SDK_STATIC): $(SDK_OBJS) $(COMMON_OBJS) $(PROTO_OBJS) | $(LIB_DIR)
	@echo "$(YELLOW)Building static SDK library...$(NC)"
	@ar rcs $@ $^
	@echo "$(GREEN)✓ Built $(SDK_STATIC)$(NC)"

# SDK target (builds both shared and static)
sdk: proto $(SDK_SHARED) $(SDK_STATIC)
	@echo "$(GREEN)✓ Client SDK built successfully$(NC)"

# Services placeholder
services: | $(BIN_DIR)
	@echo "$(YELLOW)Building services...$(NC)"
	@echo "$(BLUE)  Note: Service implementation pending$(NC)"

# Examples and tests
$(BIN_DIR)/statsd_test: examples/statsd_test.cpp $(STATSD_OBJ) | $(BIN_DIR)
	@echo "$(YELLOW)Building StatsD test (standalone)...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@
	@echo "$(GREEN)✓ Built $@$(NC)"

$(BIN_DIR)/simple_client: examples/simple_client.cpp $(SDK_STATIC) | $(BIN_DIR)
	@echo "$(YELLOW)Building example client...$(NC)"
	@$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(SDK_STATIC) $(SDK_LIBS) -o $@
	@echo "$(GREEN)✓ Built $@$(NC)"

# Convenience targets
example: $(BIN_DIR)/simple_client

test-statsd: $(BIN_DIR)/statsd_test
	@echo "$(YELLOW)Running StatsD test...$(NC)"
	@echo ""
	@./$(BIN_DIR)/statsd_test

test:
	@echo "$(YELLOW)Running tests...$(NC)"
	@echo "$(BLUE)  Note: Tests pending$(NC)"

clean:
	@echo "$(YELLOW)Cleaning build artifacts...$(NC)"
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	@echo "$(GREEN)✓ Build artifacts cleaned$(NC)"

rebuild: clean all

#==============================================================================
# NATIVE BUILD TARGETS (used by dev container)
#==============================================================================

proto-native: $(PROTO_SRCS) $(PROTO_HDRS) $(GRPC_SRCS) $(GRPC_HDRS)
	@echo "$(GREEN)✓ Proto files generated$(NC)"

sdk-native: proto-native $(SDK_SHARED) $(SDK_STATIC)
	@echo "$(GREEN)✓ Client SDK built$(NC)"

example-native: $(BIN_DIR)/simple_client
	@echo "$(GREEN)✓ Example client built$(NC)"

all-native: proto-native sdk-native
	@echo "$(GREEN)✓ All components built$(NC)"

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