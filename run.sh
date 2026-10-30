#!/bin/sh
# Builds the server if needed and starts it (Linux / macOS / MinGW shell).
cd "$(dirname "$0")"
[ -f netsim ] || make netsim || exit 1
./netsim "${1:-8080}"
