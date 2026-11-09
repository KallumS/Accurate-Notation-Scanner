#!/usr/bin/env bash
# Builds the two hosts tools/autocc/compare.py runs: the real AutoCC.jsfx
# through ysfx (the EEL2 compiler REAPER uses, downloaded and built here, as
# AutoCC's own tools/build_host.sh does), and this app's port (Source/Core/AutoCC.*).
#
#   tools/autocc/build.sh            then: python3 tools/autocc/compare.py path/to/AutoCC.jsfx
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
B="${AUTOCC_BUILD:-$HERE/build}"
YSFX_COMMIT=5c3452fee62583aa3d1b7e877d0c758c4024af89
mkdir -p "$B"
if [ ! -d "$B/ysfx" ]; then
  git clone --recurse-submodules https://github.com/JoepVanlier/ysfx "$B/ysfx"
  git -C "$B/ysfx" checkout -q "$YSFX_COMMIT"
  git -C "$B/ysfx" submodule update --init --recursive -q
fi
cmake -S "$B/ysfx" -B "$B/ysfx/build" -DCMAKE_BUILD_TYPE=Release -DYSFX_PLUGIN=OFF -DYSFX_GFX=OFF > "$B/cmake.log" 2>&1
cmake --build "$B/ysfx/build" -j"$(nproc 2>/dev/null || echo 2)" > "$B/build.log" 2>&1
LIBS=$(find "$B/ysfx/build" -name "*.a" ! -name libysfx.a)
g++ -O2 -std=c++17 -I"$B/ysfx/include" "$HERE/jsfxhost.cpp" "$B/ysfx/build/libysfx.a" $LIBS -lpthread -ldl -o "$B/jsfxhost"
g++ -O2 -std=c++17 -I"$ROOT/Source/Core" "$HERE/porthost.cpp" "$ROOT/Source/Core/AutoCC.cpp" -o "$B/porthost"
echo "built: $B/jsfxhost $B/porthost"
