#!/usr/bin/env bash
# Removes the Fenix plugins installed by install.sh. Pass --system for a system-wide install.
# User presets in ~/.config/fenrikos/Fenix are kept.
set -euo pipefail

vst3_dir="$HOME/.vst3"
lv2_dir="$HOME/.lv2"
sudo=""

if [[ "${1:-}" == "--system" ]]; then
    vst3_dir="/usr/lib/vst3"
    lv2_dir="/usr/lib/lv2"
    sudo="sudo"
fi

echo "Removing Fenix plugins:"
for bundle in "$vst3_dir"/Fenix\ *.vst3 "$lv2_dir"/Fenix\ *.lv2; do
    [[ -e "$bundle" ]] || continue
    $sudo rm -rf "$bundle"
    echo "  $bundle"
done
echo "Done. Your presets in ~/.config/fenrikos/Fenix were left in place."
