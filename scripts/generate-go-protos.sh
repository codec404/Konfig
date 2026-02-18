#!/bin/bash

set -e

# Ensure Go protobuf plugins are in PATH
export PATH="$PATH:$(go env GOPATH)/bin"

echo "Generating Go protobuf files..."

# Check for required tools
if ! command -v protoc-gen-go &> /dev/null; then
    echo "Installing protoc-gen-go..."
    go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
fi

if ! command -v protoc-gen-go-grpc &> /dev/null; then
    echo "Installing protoc-gen-go-grpc..."
    go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
fi

# Generate Go protobuf files into pkg/pb/
mkdir -p pkg/pb

protoc --proto_path=proto \
  --go_out=. \
  --go_opt=module=github.com/codec404/Konfig \
  --go-grpc_out=. \
  --go-grpc_opt=module=github.com/codec404/Konfig \
  proto/*.proto

echo "✓ Go proto files generated in pkg/pb/"
ls -la pkg/pb/*.go
