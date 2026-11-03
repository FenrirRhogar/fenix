#!/usr/bin/env bash
# Installs the Fenix plugins. Default: for the current user (~/.vst3, ~/.lv2).
# --system: for all users (/usr/lib/vst3, /usr/lib/lv2), using sudo.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
vst3_dir="$HOME/.vst3"
lv2_dir="$HOME/.lv2"
sudo=""

if [[ "${1:-}" == "--system" ]]; then
    vst3_dir="/usr/lib/vst3"
    lv2_dir="/usr/lib/lv2"
    sudo="sudo"
fi

install_bundles() {
    local from="$1" to="$2"
    $sudo mkdir -p "$to"
    for bundle in "$from"/*; do
        local name
        name="$(basename "$bundle")"
        $sudo rm -rf "${to:?}/$name"
        $sudo cp -r "$bundle" "$to/"
        echo "  $to/$name"
    done
}

echo "Installing Fenix plugins:"
install_bundles "$here/VST3" "$vst3_dir"
install_bundles "$here/LV2" "$lv2_dir"
echo "Done. Rescan plugins in your host."
