#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
detour_root="${VMANGOS_DETOUR_ROOT:-/home/ludvig/Programming/Projects/vmangos-core/dep/recastnavigation/Detour}"
object_dir="$project_root/build/navmesh_debug_objects"
mkdir -p "$object_dir"

# The diagnostic source is warning-clean under the required strict flags.
g++ -std=c++20 -Wall -Wextra -Werror -O2 \
    -I "$detour_root/Include" \
    -c "$project_root/tools/navmesh_debug.cpp" \
    -o "$object_dir/navmesh_debug.o"

# Keep the unmodified VMaNGOS Detour sources warning-clean too. Its
# DetourNavMesh.cpp has two pre-existing GCC diagnostics; suppress only those
# for that one third-party translation unit, not for the diagnostic source.
for source in "$detour_root"/Source/*.cpp; do
    name="$(basename "${source%.cpp}")"
    third_party_suppressions=()
    if [[ "$name" == DetourNavMesh ]]; then
        third_party_suppressions=(-Wno-class-memaccess -Wno-maybe-uninitialized)
    fi
    g++ -std=c++20 -Wall -Wextra -Werror "${third_party_suppressions[@]}" \
        -O2 -I "$detour_root/Include" \
        -c "$source" -o "$object_dir/$name.o"
done
g++ "$object_dir"/*.o -o "$project_root/build/navmesh_debug"
