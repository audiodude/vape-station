#!/usr/bin/env bash
# Build a universal (arm64 + x86_64) release and copy the VST3 into the
# user's plugin folder.
set -euo pipefail
cd "$(dirname "$0")"

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --target VapeStation_VST3 VapeStation_Standalone -j

SRC="build/VapeStation_artefacts/Release/VST3/VapeStation.vst3"
DEST="$HOME/Library/Audio/Plug-Ins/VST3"

mkdir -p "$DEST"
rm -rf "$DEST/VapeStation.vst3"
cp -R "$SRC" "$DEST/"
echo "Copied $SRC -> $DEST/VapeStation.vst3"
