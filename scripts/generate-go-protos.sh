#!/bin/bash

set -e

echo "Generating Go protobuf files..."

mkdir -p build

# Generate for all proto files
protoc --proto_path=proto \
  --go_out=build \
  --go_opt=paths=source_relative \
  --go-grpc_out=build \
  --go-grpc_opt=paths=source_relative \
  proto/*.proto

echo "✓ Go proto files generated in build/"
ls -la build/*.pb.go