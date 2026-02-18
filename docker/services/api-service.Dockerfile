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

# Copy source code
COPY proto/ ./proto/
COPY src/ ./src/
COPY include/ ./include/
COPY Makefile ./

# Build
RUN make proto
RUN make api-service

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
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/bin/api-service /app/

EXPOSE 8081

CMD ["./api-service"]
