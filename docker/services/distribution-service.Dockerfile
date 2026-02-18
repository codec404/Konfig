FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libprotobuf-dev \
    libgrpc++-dev \
    libpqxx-dev \
    libhiredis-dev \
    librdkafka-dev \
    libyaml-cpp-dev \
    libspdlog-dev \
    libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY proto/ ./proto/
COPY src/ ./src/
COPY include/ ./include/
COPY Makefile ./

RUN make proto
RUN make distribution-service

# Runtime image
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libprotobuf23 \
    libgrpc++1 \
    libpqxx-6.4 \
    libhiredis0.14 \
    librdkafka++1 \
    libyaml-cpp0.7 \
    libspdlog1 \
    libfmt8 \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -r -s /bin/false appuser

WORKDIR /app

COPY --from=builder /build/bin/distribution-service /app/

RUN chown -R appuser:appuser /app

USER appuser

EXPOSE 8082

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD nc -z localhost 8082 || exit 1

CMD ["./distribution-service"]
