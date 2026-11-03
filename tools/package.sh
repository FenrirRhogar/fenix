#!/usr/bin/env bash
# Packages a Release build into dist/Fenix-<version>-Linux-x64.zip, ready to upload.
#   tools/package.sh [build-dir]
# Build first with: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="${1:-$root/build}"
version="$(sed -n 's/^project(Fenix VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")"
juce="$(sed -n 's/^JUCE_DIR:PATH=//p' "$build/CMakeCache.txt")"
sdk="$juce/modules/juce_audio_processors_headless/format_types"
name="Fenix-$version-Linux-x64"
stage="$root/dist/$name"

rm -rf "$stage" "$root/dist/$name.zip"
mkdir -p "$stage/VST3" "$stage/LV2"

for plugin in Bus Drive Shelf HFComp Width Iron; do
    artefacts="$build/${plugin}_artefacts/Release"
    cp -r "$artefacts"/VST3/*.vst3 "$stage/VST3/"
    cp -r "$artefacts"/LV2/*.lv2 "$stage/LV2/"
done
find "$stage" -name '*.so' -exec strip --strip-unneeded {} +

# The newest glibc symbol any binary needs is the oldest glibc the package runs on.
glibc="$(find "$stage" -name '*.so' -exec objdump -T {} + | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1 | cut -d_ -f2)"

sed -e "s/@VERSION@/$version/" -e "s/@GLIBC@/$glibc/" "$root/packaging/README.txt" > "$stage/README.txt"
cp "$root/packaging/EULA.txt" "$root/packaging/CHANGELOG.txt" "$stage/"
install -m 755 "$root/packaging/install.sh" "$root/packaging/uninstall.sh" "$stage/"

{
    echo "FENIX COLOUR SUITE - THIRD-PARTY NOTICES"
    echo
    echo "Made with JUCE (https://juce.com), used under the JUCE 8 licence."
    echo
    echo "=== VST 3 SDK (Steinberg Media Technologies GmbH) ==="
    cat "$sdk/VST3_SDK/LICENSE.txt"
    echo
    echo "=== LV2 (lv2plug.in) ==="
    cat "$sdk/LV2_SDK/lv2/COPYING"
} > "$stage/THIRD_PARTY_NOTICES.txt"

(cd "$root/dist" && zip -qry "$name.zip" "$name")
echo "$root/dist/$name.zip (needs glibc $glibc+)"
