#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /absolute/path/to/vmangos-core" >&2
    exit 2
fi

vmangos_root="$1"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

cmake -S "$script_dir" -B "$script_dir/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DVMANGOS_ROOT="$vmangos_root"
cmake --build "$script_dir/build" --parallel
echo "Built: $script_dir/build/libwowbot_nav.so"

