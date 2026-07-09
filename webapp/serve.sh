#!/usr/bin/env bash
# Serve the webapp locally (module scripts + AudioWorklet need http, not file://).
cd "$(dirname "$0")"
echo "VapeStation web: http://localhost:8137"
exec python3 -m http.server 8137
