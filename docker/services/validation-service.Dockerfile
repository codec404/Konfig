FROM ubuntu:22.04 as builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    protobuf-compiler \
    libprotobuf-dev \
    libgrpc++-dev \
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
RUN make validation

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libprotobuf32 \
    libgrpc++1.51 \
    libyaml-cpp0.7 \
    libspdlog1 \
    libfmt8 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/bin/validation-service /app/

EXPOSE 8083

CMD ["./validation-service"]