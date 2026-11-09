#!/usr/bin/env bash
# Builds and starts the Store Management web app (Linux / macOS / WSL / Git Bash).
set -e
cd "$(dirname "$0")"
mkdir -p build
echo "Compiling the C++ backend..."
g++ -std=c++17 -O2 -pthread -o build/store_server backend/main.cpp
echo "Starting server (Ctrl+C to stop)..."
exec build/store_server --port "${PORT:-8080}" "$@"
