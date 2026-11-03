#!/usr/bin/env bash
# Builds the release plugins inside an Ubuntu 22.04 container, so they need only glibc 2.35
# instead of whatever the host distro ships, then packages them into dist/.
#   tools/build-portable.sh [path-to-JUCE]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
juce="${1:-$(sed -n 's/^JUCE_DIR:PATH=//p' "$root/build/CMakeCache.txt")}"
image="fenix-build:ubuntu22.04"

docker build -t "$image" -f "$root/packaging/Dockerfile" "$root/packaging"
docker run --rm --user "$(id -u):$(id -g)" -v "$root:/src" -v "$juce:/juce:ro" -w /src "$image" bash -c '
    cmake -B build-portable -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/juce -DFENIX_COPY_AFTER_BUILD=OFF -DFENIX_LTO=OFF &&
    cmake --build build-portable -j"$(nproc)"'

python3 "$root/tools/package.py" linux "$root/build-portable" --juce "$juce"
