#!/bin/bash

set -e

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Building Distribution Service"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Check if in dev container
if [ ! -f /.dockerenv ]; then
    echo "⚠️  Not in dev container. Use: make dev-shell"
    echo "   Then run: make distribution-service"
    exit 1
fi

# Clean previous build
echo "Cleaning previous build..."
rm -f bin/distribution-service
rm -rf build/distribution-service

# Build
echo ""
echo "Building..."
make clean
make proto
make distribution-service

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  ✓ Build complete!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Run with:"
echo "  ./bin/distribution-service config/distribution-service.yml"